#include "BS2QuadCore.h"
#include "AnalyticRecovery.h"
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepCheck.hxx>
#include <BRepCheck_Result.hxx>
#include <BRepCheck_ListIteratorOfListOfStatus.hxx>
#include <BRepLib.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_ElementarySurface.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Geom2d_Line.hxx>
#include <ShapeFix_Edge.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeExtend_Status.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <chrono>
#include <sstream>

namespace BS2Quad {
namespace {
double ValidTolerance(double value) {
    if(!std::isfinite(value)||value<=0) throw std::invalid_argument("tolerance must be finite and positive (mm)");
    return value;
}
int Count(const TopoDS_Shape& shape,TopAbs_ShapeEnum type) {
    int count=0;for(TopExp_Explorer e(shape,type);e.More();e.Next()) ++count;return count;
}
std::string TopologyErrors(BRepCheck_Analyzer& check,const TopoDS_Face& face) {
    std::ostringstream details;
    TopTools_IndexedMapOfShape shapes;
    TopExp::MapShapes(face,shapes);
    for(int i=1;i<=shapes.Extent();++i) {
        const auto result=check.Result(shapes(i));
        if(result.IsNull()) continue;
        const auto append=[&](const BRepCheck_ListOfStatus& statuses) {
            for(BRepCheck_ListIteratorOfListOfStatus it(statuses);it.More();it.Next())
                if(it.Value()!=BRepCheck_NoError) {details<<" type="<<shapes(i).ShapeType()<<":";BRepCheck::Print(it.Value(),details);}
        };
        append(result->Status());
        for(result->InitContextIterator();result->MoreShapeInContext();result->NextShapeInContext()) append(result->StatusOnShape());
    }
    for(TopExp_Explorer wires(face,TopAbs_WIRE);wires.More();wires.Next()) {
        for(TopExp_Explorer e(wires.Current(),TopAbs_EDGE);e.More();e.Next()) {
            const auto edge=TopoDS::Edge(e.Current());
            double first,last;const auto curve=BRep_Tool::CurveOnSurface(edge,face,first,last);
            if(curve.IsNull()) continue;
            auto from=curve->Value(first),to=curve->Value(last);
            if(edge.Orientation()==TopAbs_REVERSED) std::swap(from,to);
            details<<" [deg="<<BRep_Tool::Degenerated(edge)<<" uv="<<from.X()<<","<<from.Y()<<" -> "<<to.X()<<","<<to.Y()<<"]";
        }
    }
    return details.str();
}

// Canonical recovery may reverse the normal. Resolve orientation at the same
// spatial point, not at equal (u,v) coordinates in unrelated parameterizations.
bool AlignNormals(const Handle(Geom_BSplineSurface)& source,Handle(Geom_Surface)& target) {
    double u0,u1,v0,v1;source->Bounds(u0,u1,v0,v1);
    for(double t:{0.37,0.61,0.23}) {
        gp_Pnt p,q;gp_Vec su,sv,tu,tv;
        source->D1(u0+(u1-u0)*t,v0+(v1-v0)*0.43,p,su,sv);
        if(su.Crossed(sv).SquareMagnitude()<1e-24) continue;
        GeomAPI_ProjectPointOnSurf projection(p,target);
        if(projection.NbPoints()<1) return false;
        double u,v;projection.LowerDistanceParameters(u,v);target->D1(u,v,q,tu,tv);
        const auto normal=tu.Crossed(tv);
        if(normal.SquareMagnitude()<1e-24) continue;
        if(su.Crossed(sv).Dot(normal)<0) target->UReverse();
        return true;
    }
    return false;
}

void AlignPeriodicFrame(const Handle(Geom_BSplineSurface)& source,Handle(Geom_Surface)& target) {
    auto elementary=Handle(Geom_ElementarySurface)::DownCast(target);
    if(elementary.IsNull()||GeomAdaptor_Surface(target).GetType()==GeomAbs_Plane) return;
    double u0,u1,v0,v1;source->Bounds(u0,u1,v0,v1);
    auto frame=elementary->Position();
    if(GeomAdaptor_Surface(target).GetType()==GeomAbs_Sphere) {
        const auto south=source->Value(u0+(u1-u0)*0.17,v0);
        const auto north=source->Value(u0+(u1-u0)*0.17,v1);
        if(south.Distance(source->Value(u0+(u1-u0)*0.63,v0))<1e-8&&
           north.Distance(source->Value(u0+(u1-u0)*0.63,v1))<1e-8&&south.Distance(north)>1e-8)
            frame.SetDirection(gp_Dir(gp_Vec(south,north)));
    }
    const auto point=source->Value(u0,(v0+v1)*0.5);
    const gp_Vec offset(frame.Location(),point),axis(frame.Direction());
    const auto radial=offset-axis.Multiplied(offset.Dot(axis));
    // Put the analytic seam through the input seam before projecting closed
    // boundary curves. Otherwise a closed circle can choose a different start
    // branch and produce a complementary/collapsed trim region.
    if(radial.SquareMagnitude()>1e-20) frame.SetXDirection(gp_Dir(radial));
    elementary->SetPosition(frame);
}

TopoDS_Face RebuildTrimmedFace(const TopoDS_Face& original,
    const Handle(Geom_Surface)& target,double tolerance,std::string& reason) {
    // Deep-copy first: adding pcurves or repairing tolerances must never mutate
    // an input edge shared with another face in the caller's model.
    BRepBuilderAPI_Copy copy(original,true,false);
    if(!copy.IsDone()) {reason="TOPOLOGY_FAILURE: cannot copy input face";return {};}
    TopoDS_Face face=TopoDS::Face(copy.Shape());
    TopLoc_Location placement;
    BRep_Tool::Surface(face,placement);
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(face,TopAbs_EDGE,edges);
    std::vector<std::vector<TopoDS_Edge>> orderedWires;
    for(TopExp_Explorer wire(face,TopAbs_WIRE);wire.More();wire.Next()) {
        std::vector<TopoDS_Edge> ordered;
        for(BRepTools_WireExplorer e(TopoDS::Wire(wire.Current()),face);e.More();e.Next()) ordered.push_back(e.Current());
        orderedWires.push_back(std::move(ordered));
    }
    std::vector<bool> seams;
    for(int i=1;i<=edges.Extent();++i) {
        const auto edge=TopoDS::Edge(edges(i));
        seams.push_back(BRep_Tool::IsClosed(edge,face));
    }
    BRep_Builder builder;
    builder.UpdateFace(face,target,placement,std::min(tolerance,1e-5));
    for(int i=1;i<=edges.Extent();++i) {
        auto edge=TopoDS::Edge(edges(i));
        if(BRep_Tool::Degenerated(edge)) continue;
        ShapeFix_Edge fix;
        fix.FixAddPCurve(edge,face,seams[i-1],std::min(tolerance,1e-5));
        if(fix.Status(ShapeExtend_FAIL)) {reason="PCURVE_FAILURE: projecting trim curve failed";return {};}
        fix.FixReversed2d(edge,face);
        if(seams[i-1]) {edge.Reverse();fix.FixReversed2d(edge,face);edge.Reverse();}
        double first,last;
        if(BRep_Tool::CurveOnSurface(edge,face,first,last).IsNull()) {
            reason="PCURVE_FAILURE: missing parameter curve";return {};
        }
        if(seams[i-1]) {
            edge.Reverse();
            if(BRep_Tool::CurveOnSurface(edge,face,first,last).IsNull()) {
                reason="PCURVE_FAILURE: missing second seam pcurve";return {};
            }
            edge.Reverse();
        }
        fix.FixSameParameter(edge,face,std::min(tolerance,1e-5));
    }
    const auto mapPoles=[&]() -> bool {
    for(int i=1;i<=edges.Extent();++i) {
        auto edge=TopoDS::Edge(edges(i));
        if(!BRep_Tool::Degenerated(edge)) continue;
        const auto vertex=TopExp::FirstVertex(edge);
        if(vertex.IsNull()||!target->IsUPeriodic()) {
            reason="UNSUPPORTED_TRIM: collapsed edge has no periodic analytic pole mapping";return false;
        }
        const auto pole=BRep_Tool::Pnt(vertex).Transformed(placement.Transformation().Inverted());
        GeomAPI_ProjectPointOnSurf projection(pole,target);
        if(projection.NbPoints()<1||projection.LowerDistance()>1e-5) {
            reason="POLE_FAILURE: degenerated edge is not on the analytic support";return false;
        }
        double u,v;projection.LowerDistanceParameters(u,v);
        gp_Pnt point;gp_Vec du,dv;target->D1(u,v,point,du,dv);
        if(du.SquareMagnitude()>1e-14) {
            reason="UNSUPPORTED_TRIM: degenerated input edge does not map to an analytic pole";return false;
        }
        gp_Pnt2d from,to;bool mapped=false;
        for(const auto& wire:orderedWires) for(std::size_t k=0;k<wire.size();++k) {
            if(!wire[k].IsSame(edge)||wire.size()<3) continue;
            const auto previous=wire[(k+wire.size()-1)%wire.size()],next=wire[(k+1)%wire.size()];
            if(BRep_Tool::Degenerated(previous)||BRep_Tool::Degenerated(next)) continue;
            double first,last;
            const auto before=BRep_Tool::CurveOnSurface(previous,face,first,last);
            if(before.IsNull()) continue;
            from=before->Value(previous.Orientation()==TopAbs_REVERSED?first:last);
            const auto after=BRep_Tool::CurveOnSurface(next,face,first,last);
            if(after.IsNull()) continue;
            to=after->Value(next.Orientation()==TopAbs_REVERSED?last:first);
            if(wire[k].Orientation()==TopAbs_REVERSED) std::swap(from,to);
            mapped=true;
        }
        if(!mapped||target->Value(from.X(),from.Y()).Distance(pole)>1e-5||target->Value(to.X(),to.Y()).Distance(pole)>1e-5||
           std::abs(to.X()-from.X())<1e-7||std::abs(to.X()-from.X())>target->UPeriod()+1e-7) {
            reason="POLE_FAILURE: adjacent pcurves do not determine a single pole interval";return false;
        }
        // This is a collapsed 3D edge: its new horizontal parameter curve
        // follows the angular interval determined by its incident seam edges.
        // No 3D boundary is dropped or replaced by a sampled approximation.
        Handle(Geom2d_Curve) curve=new Geom2d_Line(gp_Pnt2d(from.X(),v),gp_Dir2d(to.X()>from.X()?1:-1,0));
        builder.UpdateEdge(edge,curve,face,std::min(tolerance,1e-5));
        builder.Range(edge,face,0.0,std::abs(to.X()-from.X()));
        builder.SameParameter(edge,true);
        builder.Degenerated(edge,true);
    }
        return true;
    };
    if(!mapPoles()) return {};
    BRepCheck_Analyzer beforeRepair(face,true);
    std::string beforeErrors;
    if(!beforeRepair.IsValid()) {
        beforeErrors=TopologyErrors(beforeRepair,face);
        // A new periodic chart can reverse the outer wire's UV winding even
        // after its spatial normals are aligned. Try the exact same edge
        // occurrences with reversed wire winding before geometric healing.
        TopoDS_Face reversed=TopoDS::Face(face.EmptyCopied());
        for(TopExp_Explorer wire(face,TopAbs_WIRE);wire.More();wire.Next()) builder.Add(reversed,wire.Current().Reversed());
        if(BRepCheck_Analyzer(reversed,true).IsValid()) face=reversed;
        else {
            ShapeFix_Face fix(face);
            fix.SetPrecision(std::min(tolerance,1e-5));
            fix.SetMaxTolerance(1e-5);
            fix.Perform();
            face=fix.Face();
            // Seam repair can swap its two periodic pcurves. Collapsed pole
            // intervals must follow the final seam branches, not the old ones.
            if(!mapPoles()) return {};
        }
    }
    face.Orientation(original.Orientation());
    if(Count(original,TopAbs_WIRE)!=Count(face,TopAbs_WIRE)||Count(original,TopAbs_EDGE)!=Count(face,TopAbs_EDGE)) {
        reason="TOPOLOGY_FAILURE: repair changed trim loop/edge count";return {};
    }
    BRepCheck_Analyzer topology(face,true);
    if(!topology.IsValid()) {reason="TOPOLOGY_FAILURE: invalid reconstructed face"+TopologyErrors(topology,face)+" before_repair="+beforeErrors;return {};}
    GProp_GProps oldArea,newArea,oldBoundary,newBoundary;
    // Default fixed quadrature is inaccurate for rational circles/tori (it
    // biased a 377 mm^2 cylinder by 0.248 mm^2 in the regression fixture).
    BRepGProp::SurfaceProperties(original,oldArea,1e-10);
    BRepGProp::SurfaceProperties(face,newArea,1e-10);
    BRepGProp::LinearProperties(original,oldBoundary);
    BRepGProp::LinearProperties(face,newBoundary);
    // A complementary region on a periodic support can have identical trim
    // curves. Area is an additional rejection gate, alongside bidirectional
    // interior classification in AccuracyValidator; it is not a trim proof.
    const double areaBudget=2e-5*(oldBoundary.Mass()+newBoundary.Mass())+16e-10;
    if(!std::isfinite(oldArea.Mass())||!std::isfinite(newArea.Mass())||oldArea.Mass()<=0||newArea.Mass()<=0||
       std::abs(oldArea.Mass()-newArea.Mass())>areaBudget) {
        reason="TRIM_REGION_FAILURE: area_before="+std::to_string(oldArea.Mass())+", area_after="+std::to_string(newArea.Mass())+", budget="+std::to_string(areaBudget);return {};
    }
    for(TopExp_Explorer e(face,TopAbs_EDGE);e.More();e.Next())
        if(BRep_Tool::Tolerance(TopoDS::Edge(e.Current()))>1e-5) {
            reason="TOPOLOGY_FAILURE: edge tolerance exceeds 1e-5 mm";return {};
        }
    for(TopExp_Explorer e(face,TopAbs_VERTEX);e.More();e.Next())
        if(BRep_Tool::Tolerance(TopoDS::Vertex(e.Current()))>1e-5) {
            reason="TOPOLOGY_FAILURE: vertex tolerance exceeds 1e-5 mm";return {};
        }
    return face;
}
}

BS2QuadCore::BS2QuadCore(double tolerance)
    : m_tolerance(ValidTolerance(tolerance)),m_enablePerfTest(false),
      m_totalConversions(0),m_successfulConversions(0),m_totalConversionTime(0.0) {
    m_validator=std::make_unique<AccuracyValidator>(tolerance);
}
BS2QuadCore::~BS2QuadCore() = default;

ConversionResult BS2QuadCore::ConvertImpl(const Handle(Geom_BSplineSurface)& source,const TopoDS_Face* face) {
    const auto start=std::chrono::steady_clock::now();
    ConversionResult result;
    ++m_totalConversions;
    const auto finish=[&]() {
        result.conversionTime=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(result.decisionTime==0.0) result.decisionTime=result.conversionTime;
        m_totalConversionTime+=result.conversionTime;
        if(result.success) ++m_successfulConversions;
        return result;
    };
    try {
        result.error=CheckNurbsInput(source,result.message);
        if(result.error!=ErrorType::SUCCESS) return finish();
        if(face&&!BRepCheck_Analyzer(*face,true).IsValid()) {
            result.error=ErrorType::DATA_CORRUPT;result.message="DATA_CORRUPT: invalid input face topology";return finish();
        }
        auto candidate=RecoverAnalytic(source,m_tolerance);
        result.quadricType=candidate.type;
        if(candidate.surface.IsNull()) {result.error=candidate.error;result.message=candidate.message;return finish();}
        result.accuracyResult=m_validator->ValidateAccuracy(source,candidate.surface);
        if(!result.accuracyResult.meetsRequirements) {
            result.error=ErrorType::HIGH_ERROR;result.message="GEOMETRY_REJECTED: "+result.accuracyResult.message;return finish();
        }
        result.decisionTime=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(face) {
            AlignPeriodicFrame(source,candidate.surface);
            if(!AlignNormals(source,candidate.surface)) {result.error=ErrorType::HIGH_ERROR;result.message="NORMAL_FAILURE: no regular correspondence";return finish();}
            auto rebuilt=RebuildTrimmedFace(*face,candidate.surface,m_tolerance,result.message);
            if(rebuilt.IsNull()) {result.error=ErrorType::HIGH_ERROR;return finish();}
            result.accuracyResult=m_validator->ValidateAccuracy(*face,rebuilt);
            if(!result.accuracyResult.meetsRequirements) {
                result.error=ErrorType::HIGH_ERROR;result.message="FACE_REJECTED: "+result.accuracyResult.message;return finish();
            }
            result.quadricFace=rebuilt;
        }
        result.success=true;result.error=ErrorType::SUCCESS;
        result.quadricSurface=face?BRep_Tool::Surface(result.quadricFace):candidate.surface;
        result.message=face?"converted trimmed face; boundary and normal checks complete; adjacency requires neighbor context":"converted support surface with independent full-domain distance bound";
        if(m_enablePerfTest) result.performanceResult=face?
            m_validator->ComparePerformance(*face,result.quadricFace):m_validator->ComparePerformance(source,candidate.surface);
    } catch(const Standard_Failure& failure) {
        result.success=false;result.quadricFace.Nullify();result.quadricSurface.Nullify();result.error=ErrorType::UNKNOWN_ERROR;
        result.message=std::string("KERNEL_FAILURE: ")+(failure.GetMessageString()?failure.GetMessageString():"OCCT exception");
    } catch(const std::exception& failure) {
        result.success=false;result.quadricFace.Nullify();result.quadricSurface.Nullify();result.error=ErrorType::UNKNOWN_ERROR;
        result.message=std::string("CONVERSION_FAILURE: ")+failure.what();
    }
    return finish();
}

ConversionResult BS2QuadCore::Convert(const Handle(Geom_BSplineSurface)& source) {return ConvertImpl(source,nullptr);}
ConversionResult BS2QuadCore::Convert(const TopoDS_Face& face) {
    if(face.IsNull()) return ConvertImpl({},nullptr);
    try {
        TopLoc_Location placement;
        auto support=BRep_Tool::Surface(face,placement);
        if(auto trim=Handle(Geom_RectangularTrimmedSurface)::DownCast(support);!trim.IsNull()) support=trim->BasisSurface();
        return ConvertImpl(Handle(Geom_BSplineSurface)::DownCast(support),&face);
    } catch(const Standard_Failure& failure) {
        auto result=ConvertImpl({},nullptr);result.message=std::string("FACE_READ_FAILURE: ")+(failure.GetMessageString()?failure.GetMessageString():"OCCT exception");return result;
    }
}
std::vector<ConversionResult> BS2QuadCore::ConvertBatch(const std::vector<Handle(Geom_BSplineSurface)>& sources) {
    std::vector<ConversionResult> results;results.reserve(sources.size());
    for(const auto& source:sources) results.push_back(Convert(source));
    return results;
}
ConversionResult BS2QuadCore::Convert(const TopoDS_Face& face,const std::vector<TopoDS_Face>& neighbors) {
    const auto start=std::chrono::steady_clock::now();
    auto result=Convert(face);
    if(!result.success||neighbors.empty()) return result;
    bool accepted=true;
    bool adjacencyChecked=false;
    double maximumAngle=0.0;
    try {
        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(face,TopAbs_EDGE,edges);
        for(const auto& neighbor:neighbors) {
            if(neighbor.IsNull()||neighbor.IsSame(face)) {
                result.message="INVALID_NEIGHBOR: expected a distinct nonnull adjacent face";accepted=false;break;
            }
            bool shared=false;
            TopTools_IndexedMapOfShape otherEdges;
            TopExp::MapShapes(neighbor,TopAbs_EDGE,otherEdges);
            for(int i=1;i<=edges.Extent();++i) {
                if(!otherEdges.Contains(edges(i))) continue;
                shared=true;
                const auto check=m_validator->ValidateNormalsAlongEdge(result.quadricFace,neighbor,TopoDS::Edge(edges(i)));
                if(check.adjacencyChecked) {
                    adjacencyChecked=true;
                    maximumAngle=std::max(maximumAngle,check.g1ContinuityError);
                }
                if(!check.meetsRequirements) {
                    result.message="G1_REJECTED: "+check.message;accepted=false;break;
                }
                maximumAngle=std::max(maximumAngle,check.g1ContinuityError);
            }
            if(!shared) {result.message="INVALID_NEIGHBOR: faces do not share an input topology edge";accepted=false;}
            if(!accepted) break;
        }
    } catch(const Standard_Failure& failure) {
        accepted=false;result.message=std::string("G1_FAILURE: ")+(failure.GetMessageString()?failure.GetMessageString():"kernel exception");
    }
    result.accuracyResult.adjacencyChecked=adjacencyChecked;
    result.accuracyResult.validationComplete=accepted;
    result.accuracyResult.g1ContinuityError=adjacencyChecked?maximumAngle:std::numeric_limits<double>::infinity();
    result.accuracyResult.meetsRequirements=accepted;
    if(!accepted) {
        result.success=false;result.error=ErrorType::HIGH_ERROR;
        result.quadricSurface.Nullify();result.quadricFace.Nullify();--m_successfulConversions;
    } else result.message="converted trimmed face; supplied tangent-neighbor G1 checks passed";
    const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    m_totalConversionTime+=elapsed-result.conversionTime;result.conversionTime=elapsed;
    return result;
}
void BS2QuadCore::SetTolerance(double tolerance) {
    ValidTolerance(tolerance);m_validator->SetTolerance(tolerance);m_tolerance=tolerance;
}
std::string BS2QuadCore::GetStatistics() const {
    std::ostringstream out;
    out<<"backend=OpenCASCADE; SGK 1.7 not integrated\nconversions="<<m_totalConversions<<"; accepted="<<m_successfulConversions
       <<"; total_ms="<<m_totalConversionTime<<"; mean_ms="<<(m_totalConversions?m_totalConversionTime/m_totalConversions:0.0)<<"\n";
    return out.str();
}
}
