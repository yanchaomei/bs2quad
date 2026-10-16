#include "AccuracyValidator.h"
#include "ImplicitCertificate.h"
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_IntSS.hxx>
#include <Geom_Plane.hxx>
#include <Geom_Curve.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <gp_Pln.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <Extrema_ExtPC.hxx>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace BS2Quad {
namespace {
Handle(Geom_BSplineSurface) BSplineSupport(Handle(Geom_Surface) surface) {
    while(true) {
        const auto trim=Handle(Geom_RectangularTrimmedSurface)::DownCast(surface);
        if(trim.IsNull()) break;
        surface=trim->BasisSurface();
    }
    return Handle(Geom_BSplineSurface)::DownCast(surface);
}
constexpr double Infinity=std::numeric_limits<double>::infinity();
bool Finite(const gp_Pnt& p) { return std::isfinite(p.X())&&std::isfinite(p.Y())&&std::isfinite(p.Z()); }
bool Finite(const gp_Vec& p) { return std::isfinite(p.X())&&std::isfinite(p.Y())&&std::isfinite(p.Z()); }
void RequireTolerance(double t) {
    if(!std::isfinite(t)||t<=0) throw std::invalid_argument("tolerance must be finite and positive");
}
void RequireBounds(double a,double b,double c,double d) {
    // OCCT represents infinity by large finite sentinels as well as IEEE inf.
    if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(c)||!std::isfinite(d)||
       std::max({std::abs(a),std::abs(b),std::abs(c),std::abs(d)})>=1e99||a>=b||c>=d)
        throw std::runtime_error("surface requires a finite nonempty trim domain");
}
Handle(Geom_Surface) WorldSurface(const TopoDS_Face& face) {
    // The overload without a Location returns geometry with face placement
    // applied. Evaluating raw support without this transform loses placement.
    auto surface=BRep_Tool::Surface(face);
    if(surface.IsNull()) throw std::runtime_error("face has no supporting surface");
    return surface;
}

void ProjectStatistics(const std::vector<gp_Pnt>& points,const Handle(Geom_Surface)& target,AccuracyResult& result) {
    if(points.empty()) throw std::runtime_error("no valid source samples in trim domain");
    double maximum=0;
    long double sum=0,squares=0;
    result.sampleCount=points.size();
    for(const auto& p:points) {
        try {
            if(!Finite(p)) throw std::runtime_error("non-finite evaluated sample");
            GeomAPI_ProjectPointOnSurf projection(p,target);
            if(!projection.IsDone()||projection.NbPoints()==0) throw std::runtime_error("point-to-surface projection failed");
            const double d=projection.LowerDistance();
            if(!std::isfinite(d)||d<0) throw std::runtime_error("non-finite projection distance");
            maximum=std::max(maximum,d);
            sum+=d; squares+=static_cast<long double>(d)*d;
        } catch(...) { ++result.failedSampleCount; }
    }
    if(result.failedSampleCount!=0) throw std::runtime_error("one or more source samples could not be projected");
    result.maxDistance=maximum;
    result.avgDistance=static_cast<double>(sum/points.size());
    result.rmsDistance=static_cast<double>(std::sqrt(squares/points.size()));
    if(!std::isfinite(result.avgDistance)||!std::isfinite(result.rmsDistance)) throw std::runtime_error("sample statistics overflow");
    result.surfaceChecked=true;
}

std::vector<gp_Pnt> Sample(const Handle(Geom_Surface)& surface,int nu,int nv,
                          const TopoDS_Face* trim,AccuracyResult& result) {
    double u0,u1,v0,v1;
    if(trim) BRepTools::UVBounds(*trim,u0,u1,v0,v1);
    else surface->Bounds(u0,u1,v0,v1);
    RequireBounds(u0,u1,v0,v1);
    std::vector<gp_Pnt> points;
    for(int i=0;i<nu;++i) for(int j=0;j<nv;++j) {
        const double u=u0+(u1-u0)*i/(nu-1),v=v0+(v1-v0)*j/(nv-1);
        try {
            if(trim) {
                BRepClass_FaceClassifier classifier(*trim,gp_Pnt2d(u,v),1e-9);
                if(classifier.State()==TopAbs_OUT) continue;
                if(classifier.State()!=TopAbs_IN&&classifier.State()!=TopAbs_ON)
                    throw std::runtime_error("trim classification failed");
            }
            const auto p=surface->Value(u,v);
            if(!Finite(p)) throw std::runtime_error("non-finite evaluated source point");
            points.push_back(p);
        } catch(...) { ++result.failedSampleCount; }
    }
    if(result.failedSampleCount) throw std::runtime_error("one or more source evaluations failed");
    return points;
}

void ApplyCertificate(const Handle(Geom_BSplineSurface)& source,const Handle(Geom_Surface)& target,
                      double tolerance,AccuracyResult& result) {
    const auto certificate=CertifyImplicitDistance(source,target);
    result.maxDistanceCertified=certificate.certified;
    result.certifiedMaxDistance=certificate.upperBound;
    if(!certificate.certified) { result.message="full-domain certificate unavailable: "+certificate.message; return; }
    result.validationComplete=true;
    // maxDistance remains an observed maximum. The independent upper bound
    // closes the between-sample hole; averages impose no invented tighter gate.
    result.meetsRequirements=result.maxDistance<=tolerance&&certificate.upperBound<=tolerance;
    result.message=result.meetsRequirements?
        "surface samples and full-domain directed support bound passed; boundaries, normals and adjacency not checked":
        "surface distance exceeds tolerance (sample or full-domain bound)";
}

struct EdgeGeometry {
    TopoDS_Edge edge;
    Handle(Geom_Curve) curve;
    double first=0,last=0;
    gp_Pnt point;
    bool degenerate=false;
};
std::vector<EdgeGeometry> Edges(const TopoDS_Face& face) {
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(face,TopAbs_EDGE,map);
    std::vector<EdgeGeometry> edges;
    for(int i=1;i<=map.Extent();++i) {
        const auto edge=TopoDS::Edge(map(i));
        EdgeGeometry geometry;
        geometry.edge=edge;
        geometry.degenerate=BRep_Tool::Degenerated(edge);
        if(geometry.degenerate) {
            const auto vertex=TopExp::FirstVertex(edge);
            if(vertex.IsNull()) throw std::runtime_error("degenerate boundary has no vertex");
            geometry.point=BRep_Tool::Pnt(vertex);
        } else {
            geometry.curve=BRep_Tool::Curve(edge,geometry.first,geometry.last);
            if(geometry.curve.IsNull()||!std::isfinite(geometry.first)||!std::isfinite(geometry.last)||
               geometry.first>=geometry.last||std::max(std::abs(geometry.first),std::abs(geometry.last))>=1e99)
                throw std::runtime_error("boundary has no finite curve domain");
            geometry.point=geometry.curve->Value(geometry.first);
        }
        if(!Finite(geometry.point)) throw std::runtime_error("non-finite boundary point");
        edges.push_back(geometry);
    }
    if(edges.empty()) throw std::runtime_error("face has no boundary edges");
    return edges;
}
double PointToEdge(const gp_Pnt& p,const EdgeGeometry& edge) {
    if(edge.degenerate) {
        const double d=p.Distance(edge.point);
        if(!std::isfinite(d)) throw std::runtime_error("non-finite degenerate boundary distance");
        return d;
    }
    // A stationary-point projector can report no solution when the trimmed
    // curve minimum is an endpoint. The shape-distance solver includes those
    // endpoints, and exposes a genuine completion flag for the whole edge.
    BRepExtrema_DistShapeShape projection(BRepBuilderAPI_MakeVertex(p).Vertex(),edge.edge);
    if(!projection.IsDone()||projection.NbSolution()==0)
        throw std::runtime_error("boundary projection failed");
    const double d=projection.Value();
    if(!std::isfinite(d)||d<0) throw std::runtime_error("non-finite boundary projection distance");
    return d;
}
double DirectedBoundary(const std::vector<EdgeGeometry>& from,const std::vector<EdgeGeometry>& to) {
    double maximum=0;
    for(const auto& edge:from) for(int i=0;i<(edge.degenerate?1:33);++i) {
        const gp_Pnt p=edge.degenerate?edge.point:edge.curve->Value(edge.first+(edge.last-edge.first)*i/32.0);
        if(!Finite(p)) throw std::runtime_error("non-finite boundary evaluation");
        double minimum=Infinity;
        for(const auto& target:to) minimum=std::min(minimum,PointToEdge(p,target));
        // Directed Hausdorff is max over source points of min over the target
        // UNION. Max over all edge pairs incorrectly rejects an unchanged box.
        maximum=std::max(maximum,minimum);
    }
    return maximum;
}

gp_Vec NormalAt(const Handle(Geom_Surface)& surface,double u,double v,TopAbs_Orientation orientation) {
    gp_Pnt p; gp_Vec du,dv;
    surface->D1(u,v,p,du,dv);
    if(!Finite(p)||!Finite(du)||!Finite(dv)) throw std::runtime_error("non-finite face derivative");
    auto normal=du.Crossed(dv);
    if(!Finite(normal)||normal.SquareMagnitude()<=1e-28) throw std::runtime_error("singular face normal");
    normal.Normalize();
    if(orientation==TopAbs_REVERSED) normal.Reverse();
    else if(orientation!=TopAbs_FORWARD) throw std::runtime_error("face normal has unsupported orientation");
    return normal;
}
gp_Vec ProjectedNormal(const Handle(Geom_Surface)& surface,const gp_Pnt& p,TopAbs_Orientation orientation) {
    GeomAPI_ProjectPointOnSurf projection(p,surface);
    if(!projection.IsDone()||projection.NbPoints()==0) throw std::runtime_error("normal correspondence projection failed");
    double u,v; projection.LowerDistanceParameters(u,v);
    if(!std::isfinite(u)||!std::isfinite(v)) throw std::runtime_error("non-finite normal correspondence parameters");
    return NormalAt(surface,u,v,orientation);
}
std::vector<double> EquivalentParameters(double value,double low,double high,
                                         bool periodic,double period) {
    if(!std::isfinite(value)) throw std::runtime_error("non-finite trim correspondence parameter");
    if(!periodic) return {value};
    if(!std::isfinite(period)||period<=0) throw std::runtime_error("invalid support period");
    const double turns=std::round(((low+(high-low)*0.5)-value)/period);
    std::vector<double> values;
    for(int k=-1;k<=1;++k) {
        const double candidate=value+(turns+k)*period;
        if(std::isfinite(candidate)&&candidate>=low-1e-9&&candidate<=high+1e-9) values.push_back(candidate);
    }
    return values;
}

void RequireTrimCorrespondence(const gp_Pnt& point,const TopoDS_Face& face,
                               const Handle(Geom_Surface)& support,double tolerance) {
    double u0,u1,v0,v1;
    BRepTools::UVBounds(face,u0,u1,v0,v1);
    RequireBounds(u0,u1,v0,v1);
    GeomAPI_ProjectPointOnSurf projection(point,support);
    if(!projection.IsDone()||projection.NbPoints()==0||
       !std::isfinite(projection.LowerDistance())||projection.LowerDistance()>tolerance)
        throw std::runtime_error("interior point has no face-support correspondence within tolerance");
    for(int i=1;i<=projection.NbPoints();++i) {
        const double distance=projection.Distance(i);
        if(!std::isfinite(distance)||distance<0) throw std::runtime_error("invalid trim projection distance");
        if(distance>tolerance) continue;
        double u,v; projection.Parameters(i,u,v);
        const auto us=EquivalentParameters(u,u0,u1,support->IsUPeriodic(),support->IsUPeriodic()?support->UPeriod():0);
        const auto vs=EquivalentParameters(v,v0,v1,support->IsVPeriodic(),support->IsVPeriodic()?support->VPeriod():0);
        for(const auto pu:us) for(const auto pv:vs) {
            if(pu<u0-1e-9||pu>u1+1e-9||pv<v0-1e-9||pv>v1+1e-9) continue;
            BRepClass_FaceClassifier classifier(face,gp_Pnt2d(pu,pv),1e-9);
            if(classifier.State()==TopAbs_IN||classifier.State()==TopAbs_ON) return;
            if(classifier.State()!=TopAbs_OUT) throw std::runtime_error("target trim classification failed");
        }
    }
    throw std::runtime_error("projected interior point is outside the corresponding trimmed face");
}

void RequireSameTrimRegion(const TopoDS_Face& first,const TopoDS_Face& second,double tolerance) {
    int firstWires=0,secondWires=0;
    for(TopExp_Explorer it(first,TopAbs_WIRE);it.More();it.Next()) ++firstWires;
    for(TopExp_Explorer it(second,TopAbs_WIRE);it.More();it.Next()) ++secondWires;
    if(firstWires==0||firstWires!=secondWires) throw std::runtime_error("face trim loop counts do not correspond");
    const auto from=WorldSurface(first),to=WorldSurface(second);
    // Do both directions: support equality plus a boundary sample check does
    // not select the same interior region on periodic supports or around holes.
    for(int direction=0;direction<2;++direction) {
        const auto& source=direction==0?first:second;
        const auto& target=direction==0?second:first;
        const auto& surface=direction==0?from:to;
        const auto& other=direction==0?to:from;
        double u0,u1,v0,v1;
        BRepTools::UVBounds(source,u0,u1,v0,v1);
        RequireBounds(u0,u1,v0,v1);
        int count=0;
        for(int i=0;i<8;++i) for(int j=0;j<8;++j) {
            const double u=u0+(u1-u0)*(i+0.5)/8.0,v=v0+(v1-v0)*(j+0.5)/8.0;
            BRepClass_FaceClassifier classifier(source,gp_Pnt2d(u,v),1e-9);
            if(classifier.State()==TopAbs_OUT) continue;
            if(classifier.State()!=TopAbs_IN&&classifier.State()!=TopAbs_ON) throw std::runtime_error("source trim classification failed");
            const auto point=surface->Value(u,v);
            if(!Finite(point)) throw std::runtime_error("non-finite interior face point");
            RequireTrimCorrespondence(point,target,other,tolerance);
            ++count;
        }
        if(count==0) throw std::runtime_error("no valid interior trim correspondence samples");
    }
}

double AngleDegrees(const gp_Vec& a,const gp_Vec& b,bool tangentPlanes) {
    double cosine=std::clamp(a.Dot(b),-1.0,1.0);
    if(tangentPlanes) cosine=std::abs(cosine);
    // atan2 retains precision at zero and pi, unlike acos(dot) on near-unit
    // floating normals (which can create a spurious micro-degree deviation).
    const double sine=a.Crossed(b).Magnitude();
    return std::atan2(sine,cosine)*180.0/M_PI;
}
} // namespace

AccuracyValidator::AccuracyValidator(double tolerance)
    :m_tolerance(tolerance),m_perfIterations(10),m_samplesU(16),m_samplesV(16) { RequireTolerance(tolerance); }
AccuracyValidator::~AccuracyValidator()=default;
void AccuracyValidator::SetTolerance(double tolerance) { RequireTolerance(tolerance); m_tolerance=tolerance; }
void AccuracyValidator::SetPerformanceIterations(int iterations) {
    if(iterations<=0) throw std::invalid_argument("performance iterations must be positive");
    m_perfIterations=iterations;
}

AccuracyResult AccuracyValidator::ValidateAccuracy(const Handle(Geom_BSplineSurface)& source,const Handle(Geom_Surface)& target) {
    AccuracyResult result;
    if(source.IsNull()||target.IsNull()) { result.message="null input surface"; return result; }
    try {
        const auto points=Sample(source,m_samplesU,m_samplesV,nullptr,result);
        ProjectStatistics(points,target,result);
        ApplyCertificate(source,target,m_tolerance,result);
    } catch(const std::exception& e) { result.message=e.what(); }
      catch(...) { result.message="geometry kernel failed during surface validation"; }
    return result;
}

AccuracyResult AccuracyValidator::ValidateAccuracy(const TopoDS_Face& source,const TopoDS_Face& target) {
    AccuracyResult result;
    if(source.IsNull()||target.IsNull()) { result.message="null input face"; return result; }
    try {
        const auto from=WorldSurface(source),to=WorldSurface(target);
        const auto points=Sample(from,m_samplesU,m_samplesV,&source,result);
        ProjectStatistics(points,to,result);
        const auto spline=BSplineSupport(from);
        if(spline.IsNull()) throw std::runtime_error("source face support is not a B-spline");
        ApplyCertificate(spline,to,m_tolerance,result);
        const bool surfacePass=result.meetsRequirements;
        result.validationComplete=false;
        result.meetsRequirements=false;
        result.hausdorffDistance=ComputeHausdorffDistance(source,target);
        result.boundaryChecked=true;
        RequireSameTrimRegion(source,target,m_tolerance);
        result.normalDeviationDegrees=ComputeNormalPreservation(source,target);
        result.normalChecked=true;
        result.validationComplete=result.maxDistanceCertified;
        result.meetsRequirements=surfacePass&&result.hausdorffDistance<=Precision::DEFAULT_TOLERANCE&&
                                 result.normalDeviationDegrees<=Precision::ANGLE_TOLERANCE;
        if(result.maxDistanceCertified) result.message=result.meetsRequirements?
            "directed support bound, sampled bidirectional boundary union distance, trim interiors and oriented normals passed; adjacent-face G1 not checked":
            "face validation exceeds surface, sampled boundary or oriented-normal tolerance";
    } catch(const std::exception& e) { result.meetsRequirements=false; result.validationComplete=false; result.message=e.what(); }
      catch(...) { result.meetsRequirements=false; result.validationComplete=false; result.message="geometry kernel failed during face validation"; }
    return result;
}

double AccuracyValidator::ComputeHausdorffDistance(const TopoDS_Face& first,const TopoDS_Face& second) {
    const auto a=Edges(first),b=Edges(second);
    return std::max(DirectedBoundary(a,b),DirectedBoundary(b,a));
}

double AccuracyValidator::ComputeNormalPreservation(const TopoDS_Face& source,const TopoDS_Face& target) {
    const auto from=WorldSurface(source),to=WorldSurface(target);
    double u0,u1,v0,v1;
    BRepTools::UVBounds(source,u0,u1,v0,v1);
    RequireBounds(u0,u1,v0,v1);
    double maximum=0; int count=0;
    for(int i=0;i<10;++i) for(int j=0;j<10;++j) {
        const double u=u0+(u1-u0)*(i+0.5)/10.0,v=v0+(v1-v0)*(j+0.5)/10.0;
        BRepClass_FaceClassifier classifier(source,gp_Pnt2d(u,v),1e-9);
        if(classifier.State()==TopAbs_OUT) continue;
        if(classifier.State()!=TopAbs_IN&&classifier.State()!=TopAbs_ON) throw std::runtime_error("normal trim classification failed");
        const auto p=from->Value(u,v);
        if(!Finite(p)) throw std::runtime_error("non-finite normal source point");
        maximum=std::max(maximum,AngleDegrees(NormalAt(from,u,v,source.Orientation()),
            ProjectedNormal(to,p,target.Orientation()),false));
        ++count;
    }
    if(count==0) throw std::runtime_error("no valid normal samples in face trim");
    return maximum;
}

AccuracyResult AccuracyValidator::ValidateNormalsAlongEdge(
    const TopoDS_Face& first,const TopoDS_Face& second,const TopoDS_Edge& reference) {
    AccuracyResult result;
    if(first.IsNull()||second.IsNull()||reference.IsNull()) {
        result.message="null face or reference edge"; return result;
    }
    try {
        if(BRep_Tool::Degenerated(reference)) throw std::runtime_error("reference edge is degenerate");
        const auto a=WorldSurface(first),b=WorldSurface(second);
        const auto boundaryA=Edges(first),boundaryB=Edges(second);
        BRepAdaptor_Curve curve(reference);
        const double start=curve.FirstParameter(),end=curve.LastParameter();
        if(!std::isfinite(start)||!std::isfinite(end)||start>=end||
           std::max(std::abs(start),std::abs(end))>=1e99)
            throw std::runtime_error("reference edge requires finite parameter range");
        double maximum=0;
        constexpr double edgeTolerance=Precision::DEFAULT_TOLERANCE;
        for(int j=0;j<33;++j) {
            const auto point=curve.Value(start+(end-start)*(j+0.5)/33.0);
            if(!Finite(point)) throw std::runtime_error("non-finite reference edge point");
            for(const auto* boundary:{&boundaryA,&boundaryB}) {
                double distance=Infinity;
                for(const auto& edge:*boundary) distance=std::min(distance,PointToEdge(point,edge));
                if(!std::isfinite(distance)||distance>edgeTolerance)
                    throw std::runtime_error("reference edge is outside a trimmed face boundary");
            }
            for(const auto& support:{a,b}) {
                GeomAPI_ProjectPointOnSurf projection(point,support);
                if(!projection.IsDone()||projection.NbPoints()==0||
                   !std::isfinite(projection.LowerDistance())||projection.LowerDistance()>edgeTolerance)
                    throw std::runtime_error("reference edge has no support-surface correspondence");
            }
            maximum=std::max(maximum,AngleDegrees(ProjectedNormal(a,point,first.Orientation()),
                ProjectedNormal(b,point,second.Orientation()),false));
            ++result.sampleCount;
        }
        result.g1ContinuityError=maximum;
        result.adjacencyChecked=true;
        result.validationComplete=true;
        result.meetsRequirements=maximum<=Precision::ANGLE_TOLERANCE;
        result.message=result.meetsRequirements?
            "sampled reference-edge boundary correspondence and oriented-normal G1 check passed; caller establishes original adjacency":
            "reference-edge oriented-normal angle exceeds 0.1 degrees";
    } catch(const std::exception& e) { result.message=e.what(); ++result.failedSampleCount; }
      catch(...) { result.message="geometry kernel failed during reference-edge check"; ++result.failedSampleCount; }
    return result;
}

AccuracyResult AccuracyValidator::ValidateAdjacentFacesG1(const TopoDS_Face& first,const TopoDS_Face& second) {
    AccuracyResult result;
    if(first.IsNull()||second.IsNull()) { result.message="null adjacency face"; return result; }
    if(first.IsSame(second)) { result.message="adjacency requires two distinct faces"; return result; }
    try {
        TopTools_IndexedMapOfShape edgesA,edgesB;
        TopExp::MapShapes(first,TopAbs_EDGE,edgesA); TopExp::MapShapes(second,TopAbs_EDGE,edgesB);
        double maximum=0;
        for(int i=1;i<=edgesA.Extent();++i) {
            if(!edgesB.Contains(edgesA(i))) continue;
            const auto edgeResult=ValidateNormalsAlongEdge(first,second,TopoDS::Edge(edgesA(i)));
            result.sampleCount+=edgeResult.sampleCount;
            result.failedSampleCount+=edgeResult.failedSampleCount;
            if(!edgeResult.validationComplete) { result.message=edgeResult.message; return result; }
            maximum=std::max(maximum,edgeResult.g1ContinuityError);
        }
        if(result.sampleCount==0) throw std::runtime_error("faces have no actual shared topological edge");
        result.g1ContinuityError=maximum;
        result.adjacencyChecked=true;
        result.validationComplete=true;
        result.meetsRequirements=maximum<=Precision::ANGLE_TOLERANCE;
        result.message=result.meetsRequirements?"sampled actual-shared-edge oriented-normal G1 check passed":
            "shared-edge oriented-normal angle exceeds 0.1 degrees";
    } catch(const std::exception& e) { result.message=e.what(); ++result.failedSampleCount; }
      catch(...) { result.message="geometry kernel failed during adjacency check"; ++result.failedSampleCount; }
    return result;
}

PerformanceResult AccuracyValidator::ComparePerformance(
    const Handle(Geom_BSplineSurface)& source,const Handle(Geom_Surface)& target) {
    PerformanceResult result;
    if(source.IsNull()||target.IsNull()) { result.message="null performance surface"; return result; }
    std::vector<gp_Pnt> points;
    try {
        double u0,u1,v0,v1;
        source->Bounds(u0,u1,v0,v1);
        RequireBounds(u0,u1,v0,v1);
        for(int i=0;i<10;++i) for(int j=0;j<10;++j) {
            const auto p=source->Value(u0+(u1-u0)*(i+0.5)/10.0,v0+(v1-v0)*(j+0.5)/10.0);
            if(!Finite(p)) throw std::runtime_error("non-finite benchmark source sample");
            points.push_back(p.Translated(gp_Vec(0.13,-0.07,0.19)));
        }
    } catch(...) { result.message="benchmark source evaluation failed"; return result; }
    result.nurbsProjectionTime=BenchmarkProjection(source,points);
    result.quadricProjectionTime=BenchmarkProjection(target,points);
    result.projectionComplete=std::isfinite(result.nurbsProjectionTime)&&std::isfinite(result.quadricProjectionTime);
    result.nurbsIntersectionTime=BenchmarkIntersection(source);
    result.quadricIntersectionTime=BenchmarkIntersection(target);
    result.intersectionComplete=std::isfinite(result.nurbsIntersectionTime)&&std::isfinite(result.quadricIntersectionTime);
    result.message="operation completion reported per pair; support domains differ, so these timings alone do not establish a controlled speedup; Boolean operations not checked";
    return result;
}

PerformanceResult AccuracyValidator::ComparePerformance(const TopoDS_Face& source,const TopoDS_Face& target) {
    PerformanceResult result;
    if(source.IsNull()||target.IsNull()) { result.message="null performance face"; return result; }
    try {
        const auto from=WorldSurface(source),to=WorldSurface(target);
        const auto spline=BSplineSupport(from);
        if(!spline.IsNull()) result=ComparePerformance(spline,to);
        result.nurbsBooleanTime=BenchmarkBoolean(source);
        result.quadricBooleanTime=BenchmarkBoolean(target);
        result.booleanComplete=std::isfinite(result.nurbsBooleanTime)&&std::isfinite(result.quadricBooleanTime);
        result.message="operation completion reported separately for projection, intersection and Boolean pairs; timings alone do not establish equivalent-domain correctness or a controlled speedup";
    } catch(...) { result.message="geometry kernel failed during performance setup"; }
    return result;
}

double AccuracyValidator::BenchmarkProjection(const Handle(Geom_Surface)& surface,const std::vector<gp_Pnt>& points) {
    if(surface.IsNull()||points.empty()) return Infinity;
    const auto start=std::chrono::steady_clock::now();
    for(int iter=0;iter<m_perfIterations;++iter) for(const auto& p:points) {
        try {
            GeomAPI_ProjectPointOnSurf projection(p,surface);
            if(!projection.IsDone()||projection.NbPoints()==0||!std::isfinite(projection.LowerDistance())) return Infinity;
        } catch(...) { return Infinity; }
    }
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/m_perfIterations;
}

double AccuracyValidator::BenchmarkIntersection(const Handle(Geom_Surface)& surface) {
    if(surface.IsNull()) return Infinity;
    Handle(Geom_Surface) plane=new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1));
    const auto start=std::chrono::steady_clock::now();
    for(int iter=0;iter<m_perfIterations;++iter) {
        try {
            GeomAPI_IntSS intersection(surface,plane,Precision::DEFAULT_TOLERANCE);
            // This benchmark requires actual intersection curves. A reported
            // empty result for coincident/unsupported supports is not timed as
            // a successful useful intersection workload.
            if(!intersection.IsDone()||intersection.NbLines()==0) return Infinity;
            for(int i=1;i<=intersection.NbLines();++i) if(intersection.Line(i).IsNull()) return Infinity;
        } catch(...) { return Infinity; }
    }
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/m_perfIterations;
}

double AccuracyValidator::BenchmarkBoolean(const TopoDS_Face& face) {
    try {
        if(face.IsNull()||!BRepCheck_Analyzer(face).IsValid()) return Infinity;
        const auto plane=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),-100.0,100.0,-100.0,100.0).Face();
        const auto start=std::chrono::steady_clock::now();
        for(int iter=0;iter<m_perfIterations;++iter) {
            BRepAlgoAPI_Common common(face,plane);
            if(!common.IsDone()||common.Shape().IsNull()||!BRepCheck_Analyzer(common.Shape()).IsValid()) return Infinity;
            if(!TopExp_Explorer(common.Shape(),TopAbs_FACE).More()&&
               !TopExp_Explorer(common.Shape(),TopAbs_EDGE).More()&&
               !TopExp_Explorer(common.Shape(),TopAbs_VERTEX).More()) return Infinity;
        }
        return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/m_perfIterations;
    } catch(...) { return Infinity; }
}

double AccuracyValidator::GetTimeInMilliseconds() const {
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace BS2Quad
