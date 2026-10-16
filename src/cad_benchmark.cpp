#include "CheckedOutput.h"
#include "BS2QuadCore.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <GeomConvert.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Plane.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ListOfShape.hxx>
#include <gp_Pln.hxx>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <limits>
#include <string>

using namespace BS2Quad;
namespace {
constexpr double Tol=1e-5;
constexpr double Inf=std::numeric_limits<double>::infinity();
struct Check { bool valid=false; double error=Inf,areaError=0,areaAllowance=0; std::string reason; };
bool Finite(const gp_Pnt& p) {return std::isfinite(p.X())&&std::isfinite(p.Y())&&std::isfinite(p.Z());}
Check CompareProjection(const std::vector<double>& a,const std::vector<double>& b) {
    Check c; c.valid=!a.empty()&&a.size()==b.size();c.error=0;
    for(std::size_t i=0;i<a.size()&&i<b.size();++i) {
        if(!std::isfinite(a[i])||!std::isfinite(b[i])||a[i]<0||b[i]<0) c.valid=false;
        c.error=std::max(c.error,std::abs(a[i]-b[i]));
    }
    c.valid=c.valid&&c.error<=Tol;
    if(!c.valid)c.reason="projection distances changed or incomplete";
    return c;
}
double EdgeDistance(const gp_Pnt& point,const TopoDS_Shape& edge) {
    BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(point).Vertex(),edge);
    if(!distance.IsDone()||distance.NbSolution()==0||!std::isfinite(distance.Value())||distance.Value()<0)
        throw std::runtime_error("trimmed-edge distance failed");
    return distance.Value();
}
double BoundedFaceDistance(const gp_Pnt& point,const TopoDS_Face& face) {
    if(!Finite(point))throw std::runtime_error("non-finite projection query");
    const auto support=BRep_Tool::Surface(face);
    double u0,u1,v0,v1;BRepTools::UVBounds(face,u0,u1,v0,v1);
    if(support.IsNull()||!std::isfinite(u0)||!std::isfinite(u1)||!std::isfinite(v0)||!std::isfinite(v1)||u0>=u1||v0>=v1)
        throw std::runtime_error("invalid bounded-face projection domain");
    double best=Inf;
    int edges=0;
    for(TopExp_Explorer edge(face,TopAbs_EDGE);edge.More();edge.Next()) {
        best=std::min(best,EdgeDistance(point,edge.Current()));++edges;
    }
    if(edges==0)throw std::runtime_error("bounded face has no trim edges");
    GeomAPI_ProjectPointOnSurf projection(point,support);
    if(!projection.IsDone()||projection.NbPoints()==0)throw std::runtime_error("support projection failed");
    for(int i=1;i<=projection.NbPoints();++i) {
        double u,v;projection.Parameters(i,u,v);
        const double distance=projection.Distance(i);
        if(!std::isfinite(u)||!std::isfinite(v)||!std::isfinite(distance)||distance<0)throw std::runtime_error("invalid support projection");
        if(support->IsUPeriodic())u+=std::round(((u0+u1)*.5-u)/support->UPeriod())*support->UPeriod();
        if(support->IsVPeriodic())v+=std::round(((v0+v1)*.5-v)/support->VPeriod())*support->VPeriod();
        if(u<u0-1e-9||u>u1+1e-9||v<v0-1e-9||v>v1+1e-9)continue;
        BRepClass_FaceClassifier classifier(face,gp_Pnt2d(u,v),1e-9);
        if(classifier.State()==TopAbs_IN||classifier.State()==TopAbs_ON)best=std::min(best,distance);
        else if(classifier.State()!=TopAbs_OUT)throw std::runtime_error("projection trim classification failed");
    }
    // Stationary support projections inside the trim, plus all complete
    // boundary-edge minima, cover the finite face. Periodic UV mapping matters:
    // OCCT7.7 BRepExtrema can overestimate distances for rebuilt negative-V
    // torus trims, even for points exactly on the surface and classified IN.
    if(!std::isfinite(best))throw std::runtime_error("no finite trimmed-face distance");
    return best;
}
double DistanceTo(const gp_Pnt& p,const TopoDS_Shape& shape) {
    if(!Finite(p))throw std::runtime_error("non-finite oracle point");
    double best=Inf;int faces=0;
    for(TopExp_Explorer e(shape,TopAbs_FACE);e.More();e.Next()) {
        best=std::min(best,BoundedFaceDistance(p,TopoDS::Face(e.Current())));++faces;
    }
    return faces?best:EdgeDistance(p,shape);
}
std::vector<gp_Pnt> ResultSamples(const TopoDS_Shape& shape,bool faces) {
    std::vector<gp_Pnt> points;
    for(TopExp_Explorer e(shape,TopAbs_EDGE);e.More();e.Next()) {
        const auto edge=TopoDS::Edge(e.Current());
        if(BRep_Tool::Degenerated(edge)) continue;
        BRepAdaptor_Curve curve(edge);
        const double lo=curve.FirstParameter(),hi=curve.LastParameter();
        if(!std::isfinite(lo)||!std::isfinite(hi)||lo>=hi) throw std::runtime_error("invalid result edge range");
        for(int i=0;i<33;++i)points.push_back(curve.Value(lo+(hi-lo)*i/32.0));
    }
    if(faces) for(TopExp_Explorer e(shape,TopAbs_FACE);e.More();e.Next()) {
        const auto face=TopoDS::Face(e.Current());
        const auto support=BRep_Tool::Surface(face);
        double u0,u1,v0,v1;BRepTools::UVBounds(face,u0,u1,v0,v1);
        if(support.IsNull()||!std::isfinite(u0)||!std::isfinite(u1)||!std::isfinite(v0)||!std::isfinite(v1)||u0>=u1||v0>=v1)
            throw std::runtime_error("invalid result face bounds");
        for(int i=0;i<5;++i)for(int j=0;j<5;++j) {
            const double u=u0+(u1-u0)*(i+.5)/5.0,v=v0+(v1-v0)*(j+.5)/5.0;
            BRepClass_FaceClassifier classifier(face,gp_Pnt2d(u,v),1e-9);
            if(classifier.State()==TopAbs_IN||classifier.State()==TopAbs_ON)points.push_back(support->Value(u,v));
            else if(classifier.State()!=TopAbs_OUT) throw std::runtime_error("result trim classification failed");
        }
    }
    if(points.empty())throw std::runtime_error("result has no representative geometry");
    return points;
}
Check CompareShapes(const TopoDS_Shape& a,const TopoDS_Shape& b,bool faces) {
    Check c;
    try {
        if(a.IsNull()||b.IsNull()||!BRepCheck_Analyzer(a).IsValid()||!BRepCheck_Analyzer(b).IsValid())
            throw std::runtime_error("operation produced null or invalid topology");
        const auto kind=faces?TopAbs_FACE:TopAbs_EDGE;
        int na=0,nb=0;
        for(TopExp_Explorer e(a,kind);e.More();e.Next())++na;
        for(TopExp_Explorer e(b,kind);e.More();e.Next())++nb;
        if(na==0||nb==0)throw std::runtime_error("empty operation result cannot count as a fast result");
        GProp_GProps pa,pb;
        if(faces){
            const double ea=BRepGProp::SurfaceProperties(a,pa,1e-10),eb=BRepGProp::SurfaceProperties(b,pb,1e-10);
            if(!std::isfinite(ea)||!std::isfinite(eb)||ea>1e-8||eb>1e-8)throw std::runtime_error("adaptive Boolean area integration did not converge");
        }
        else {BRepGProp::LinearProperties(a,pa);BRepGProp::LinearProperties(b,pb);}
        const double ma=pa.Mass(),mb=pb.Mass();
        if(!std::isfinite(ma)||!std::isfinite(mb)||ma<=Tol||mb<=Tol)throw std::runtime_error("operation result has no positive measure");
        if(faces) {
            GProp_GProps la,lb;BRepGProp::LinearProperties(a,la);BRepGProp::LinearProperties(b,lb);
            c.areaError=std::abs(ma-mb);
            c.areaAllowance=Tol*(1+la.Mass()+lb.Mass());
            if(!std::isfinite(c.areaAllowance)||c.areaError>c.areaAllowance)throw std::runtime_error("Boolean result areas differ");
        } else if(std::abs(ma-mb)>Tol*2*(na+nb))throw std::runtime_error("section result lengths differ");
        const auto sa=ResultSamples(a,faces),sb=ResultSamples(b,faces);
        c.error=0;
        for(const auto& p:sa)c.error=std::max(c.error,DistanceTo(p,b));
        for(const auto& p:sb)c.error=std::max(c.error,DistanceTo(p,a));
        c.valid=c.error<=Tol;
        if(!c.valid)c.reason="bidirectional sampled result geometry differs";
    } catch(const std::exception& e) {c.reason=e.what();}
      catch(...) {c.reason="geometry kernel failed while checking results";}
    return c;
}
std::vector<double> Project(const TopoDS_Face& face,const std::vector<gp_Pnt>& points) {
    std::vector<double> out;
    for(const auto& point:points)out.push_back(BoundedFaceDistance(point,face));
    return out;
}
bool ConvertedTorusProjectionRegression();
int SelfTest() {
    int failures=0;
    const auto expect=[&](bool condition,const char* name) {std::cout<<(condition?"PASS ":"FAIL ")<<name<<'\n';if(!condition)++failures;};
    expect(CompareProjection({2,3},{2,3}).valid,"equal bounded projection distances accepted");
    expect(!CompareProjection({2,3},{1,2}).valid,"changed projection distances rejected");
    TopoDS_Compound empty;BRep_Builder builder;builder.MakeCompound(empty);
    expect(!CompareShapes(empty,empty,false).valid,"empty section cannot count as fast");
    expect(!CompareShapes(empty,empty,true).valid,"empty Boolean cannot count as fast");
    const auto line=BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,0),gp_Pnt(1,0,0)).Edge();
    const auto shifted=BRepBuilderAPI_MakeEdge(gp_Pnt(0,.01,0),gp_Pnt(1,.01,0)).Edge();
    expect(CompareShapes(line,line,false).valid,"identical nonempty section accepted");
    expect(!CompareShapes(line,shifted,false).valid,"spatially changed section rejected");
    const auto face=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),-1.0,1.0,-1.0,1.0).Face();
    const auto larger=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),-2.0,2.0,-1.0,1.0).Face();
    expect(CompareShapes(face,face,true).valid,"identical nonempty Boolean accepted");
    expect(!CompareShapes(face,larger,true).valid,"changed Boolean area rejected");
    const auto bounded=Project(face,{gp_Pnt(2,0,0)});
    expect(bounded.size()==1&&std::abs(bounded[0]-1)<Tol,"projection respects finite trim instead of unbounded support");
    Handle(Geom_Surface) torus=new Geom_ToroidalSurface(gp_Ax3(),8,2);
    const auto torusFace=BRepBuilderAPI_MakeFace(torus,.2,2.1,-.6,1.1,1e-7).Face();
    gp_Pnt point;gp_Vec du,dv;torus->D1(.39,-.42,point,du,dv);
    auto normal=du.Crossed(dv);normal.Normalize();
    const auto torusDistance=Project(torusFace,{point.Translated(normal*.25)});
    expect(torusDistance.size()==1&&std::abs(torusDistance[0]-.25)<Tol,"negative-period torus trim retains the true interior projection");
    expect(ConvertedTorusProjectionRegression(),"converted torus face preserves known negative-period interior projection");
    return failures?1:0;
}

struct Fixture {
    std::string name;
    Handle(Geom_BSplineSurface) source;
    TopoDS_Face before,after,sectionPlane;
    TopoDS_Shape clipBox;
    std::vector<gp_Pnt> queries;
    std::vector<double> expectedDistances;
    int poles=0;
};
void Densify(const Handle(Geom_BSplineSurface)& s,bool u) {
    while((u?s->NbUPoles():s->NbVPoles())<10) {
        const int count=u?s->NbUKnots():s->NbVKnots();
        double gap=-1,middle=0;
        for(int i=1;i<count;++i) {
            const double a=u?s->UKnot(i):s->VKnot(i),b=u?s->UKnot(i+1):s->VKnot(i+1);
            if(b-a>gap){gap=b-a;middle=a+(b-a)*.5;}
        }
        if(gap<=0)throw std::runtime_error("cannot densify exact source");
        if(u)s->InsertUKnot(middle,1,1e-12);else s->InsertVKnot(middle,1,1e-12);
    }
}
Fixture MakeFixture(int index) {
    Fixture f;
    const gp_Ax3 frame;
    Handle(Geom_Surface) analytic;
    std::array<double,4> bounds;
    switch(index) {
    case 0:f.name="plane";analytic=new Geom_Plane(frame);bounds={{-4,5,-4,5}};break;
    case 1:f.name="cylinder";analytic=new Geom_CylindricalSurface(frame,5);bounds={{.2,2.1,-3,5}};break;
    case 2:f.name="sphere";analytic=new Geom_SphericalSurface(frame,5);bounds={{.2,2.1,-.65,.75}};break;
    case 3:f.name="cone";analytic=new Geom_ConicalSurface(frame,.35,4);bounds={{.2,2.1,1,7}};break;
    default:f.name="torus";analytic=new Geom_ToroidalSurface(frame,8,2);bounds={{.2,2.1,-.6,1.1}};break;
    }
    f.source=GeomConvert::SurfaceToBSplineSurface(new Geom_RectangularTrimmedSurface(analytic,bounds[0],bounds[1],bounds[2],bounds[3]));
    Densify(f.source,true);Densify(f.source,false);
    f.poles=f.source->NbUPoles()*f.source->NbVPoles();
    if(f.poles<50||f.poles>200)throw std::runtime_error("fixture is outside 50-200 control-pole contract");
    f.before=BRepBuilderAPI_MakeFace(f.source,1e-7).Face();
    const auto converted=BS2QuadCore().Convert(f.before);
    if(!converted.success||converted.quadricFace.IsNull())throw std::runtime_error(f.name+" conversion failed: "+converted.message);
    f.after=converted.quadricFace;
    if(!BRepCheck_Analyzer(f.before).IsValid()||!BRepCheck_Analyzer(f.after).IsValid())throw std::runtime_error("benchmark input face is invalid");
    double u0,u1,v0,v1;f.source->Bounds(u0,u1,v0,v1);
    gp_Pnt center;gp_Vec du,dv;
    f.source->D1((u0+u1)*.5,(v0+v1)*.5,center,du,dv);
    if(!Finite(center)||du.Magnitude()<1e-10)throw std::runtime_error("invalid fixture interior");
    f.sectionPlane=BRepBuilderAPI_MakeFace(gp_Pln(center,gp_Dir(du)),-40.0,40.0,-40.0,40.0).Face();
    f.clipBox=BRepPrimAPI_MakeBox(gp_Pnt(center.X()-40,center.Y()-40,center.Z()-40),40,80,80).Shape();
    for(int i=0;i<5;++i)for(int j=0;j<5;++j) {
        gp_Pnt p;gp_Vec a,b;
        f.source->D1(u0+(u1-u0)*(i+.5)/5.0,v0+(v1-v0)*(j+.5)/5.0,p,a,b);
        auto n=a.Crossed(b);
        if(!Finite(p)||n.Magnitude()<1e-10)throw std::runtime_error("invalid query-source normal");
        n.Normalize();
        const double offset=.25+.05*((i+2*j)%5);
        f.queries.push_back(p.Translated(n*offset));
        // All five exact analytic fixtures use offsets below their local
        // reach, so their interior normal footpoint has known distance offset.
        f.expectedDistances.push_back(offset);
    }
    return f;
}
bool ConvertedTorusProjectionRegression() {
    const auto fixture=MakeFixture(4);
    const auto before=Project(fixture.before,{fixture.queries.front()});
    const auto after=Project(fixture.after,{fixture.queries.front()});
    return before.size()==1&&after.size()==1&&std::abs(before[0]-.25)<Tol&&std::abs(after[0]-.25)<Tol;
}
TopoDS_Shape Section(const TopoDS_Face& face,const TopoDS_Face& operand) {
    BRepAlgoAPI_Section operation(face,operand,false);
    operation.SetRunParallel(false);operation.Approximation(true);operation.Build();
    if(!operation.IsDone())throw std::runtime_error("finite-face Section failed");
    return operation.Shape();
}
TopoDS_Shape Common(const TopoDS_Face& face,const TopoDS_Shape& operand) {
    BRepAlgoAPI_Common operation;
    TopTools_ListOfShape arguments,tools;arguments.Append(face);tools.Append(operand);
    operation.SetArguments(arguments);operation.SetTools(tools);operation.SetRunParallel(false);operation.Build();
    if(!operation.IsDone())throw std::runtime_error("face/box Common failed");
    return operation.Shape();
}
template<class T> struct Timed { T value;double ms=Inf; };
template<class T,class Function> Timed<T> Measure(Function work) {
    const auto start=std::chrono::steady_clock::now();
    T value=work();
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    if(!std::isfinite(ms)||ms<=0)throw std::runtime_error("invalid operation timing");
    return {std::move(value),ms};
}
std::string Quote(const std::string& value,bool csv=false) {
    std::string out="\"";
    for(char c:value) {
        if(c=='"')out+=csv?"\"\"":"\\\"";
        else if(c=='\\')out+=csv?"\\":"\\\\";
        else if(c=='\n'||c=='\r')out+=' ';
        else out+=c;
    }
    return out+'"';
}
void Number(std::ostream& out,double x){if(std::isfinite(x))out<<std::setprecision(17)<<x;else out<<"null";}
double Median(std::vector<double> v) {if(v.empty())return Inf;std::sort(v.begin(),v.end());return v[v.size()/2];}
struct Summary {std::string name,operation,message;int poles=0;bool valid=true,pass=false;double before=Inf,after=Inf,gain=-Inf,maxError=0;};
Summary RunOperation(const Fixture& f,const std::string& operation,int iterations,std::ostream& raw,const std::filesystem::path& directory) {
    Summary summary;summary.name=f.name;summary.operation=operation;summary.poles=f.poles;
    std::vector<double> before,after;
    for(int repeat=-1;repeat<iterations;++repeat) {
        double oldMs=Inf,newMs=Inf;Check check;
        try {
            if(operation=="projection") {
                Timed<std::vector<double>> a,b;
                if(repeat%2==0){a=Measure<std::vector<double>>([&]{return Project(f.before,f.queries);});b=Measure<std::vector<double>>([&]{return Project(f.after,f.queries);});}
                else {b=Measure<std::vector<double>>([&]{return Project(f.after,f.queries);});a=Measure<std::vector<double>>([&]{return Project(f.before,f.queries);});}
                oldMs=a.ms;newMs=b.ms;check=CompareProjection(a.value,b.value);
                const auto truthBefore=CompareProjection(a.value,f.expectedDistances),truthAfter=CompareProjection(b.value,f.expectedDistances);
                check.valid=check.valid&&truthBefore.valid&&truthAfter.valid;
                check.error=std::max({check.error,truthBefore.error,truthAfter.error});
                if(!check.valid)check.reason="finite-face projection disagrees with pair or known interior normal-offset distance";
            } else {
                const auto work=[&](const TopoDS_Face& face){return operation=="intersection"?Section(face,f.sectionPlane):Common(face,f.clipBox);};
                Timed<TopoDS_Shape> a,b;
                if(repeat%2==0){a=Measure<TopoDS_Shape>([&]{return work(f.before);});b=Measure<TopoDS_Shape>([&]{return work(f.after);});}
                else {b=Measure<TopoDS_Shape>([&]{return work(f.after);});a=Measure<TopoDS_Shape>([&]{return work(f.before);});}
                oldMs=a.ms;newMs=b.ms;check=CompareShapes(a.value,b.value,operation=="boolean");
                // Preserve representative result receipts outside timed work.
                if(repeat==0) {
                    const auto beforePath=directory/(f.name+"_"+operation+"_before_result.brep");
                    const auto afterPath=directory/(f.name+"_"+operation+"_after_result.brep");
                    if(a.value.IsNull()||b.value.IsNull()||!BRepTools::Write(a.value,beforePath.string().c_str())||
                       !BRepTools::Write(b.value,afterPath.string().c_str()))throw std::runtime_error("cannot preserve operation-result BRep receipts");
                }
            }
        } catch(const std::exception& e){check.reason=e.what();}
          catch(...){check.reason="geometry kernel failed during operation";}
        const double gain=check.valid?1-newMs/oldMs:-Inf;
        const bool pass=check.valid&&gain>=.5;
        raw<<f.name<<','<<operation<<','<<(repeat<0?"warmup":"measured")<<','<<repeat<<','<<f.poles<<','
           <<std::setprecision(17)<<oldMs<<','<<newMs<<','<<(check.valid?1:0)<<','<<check.error<<','
           <<check.areaError<<','<<check.areaAllowance<<','<<gain<<",0.5,"<<(pass?1:0)<<','<<Quote(check.reason,true)<<'\n';
        raw.flush();
        summary.valid=summary.valid&&check.valid;
        if(!check.valid)summary.message=check.reason;
        summary.maxError=std::max(summary.maxError,check.error);
        if(repeat>=0&&check.valid){before.push_back(oldMs);after.push_back(newMs);}
    }
    if(summary.valid&&static_cast<int>(before.size())==iterations) {
        summary.before=Median(before);summary.after=Median(after);summary.gain=1-summary.after/summary.before;
        summary.pass=summary.gain>=.5;
        summary.message=summary.pass?"valid measurements; median reduction meets 50% target":"valid measurements; median reduction below 50% target";
    }
    return summary;
}
int Integer(const std::string& text) {
    if(text!="5"&&text!="7")throw std::invalid_argument("--iterations must be 5 or 7");
    return std::stoi(text);
}
} // namespace
int main(int argc,char** argv) {
    try {
        std::string directory="data/cad-benchmark",selectedCase;int iterations=5;bool pilot=false;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--self-test")return SelfTest();
            if(arg=="--pilot"){pilot=true;continue;}
            if(arg=="--help"){std::cout<<"bs2quad_cad_benchmark --output-dir PATH --iterations 5|7 [--pilot | --case FAMILY] | --self-test\nExit0 means valid completed measurements; performance target pass is explicit in summary.json.\n";return 0;}
            if(arg!="--output-dir"&&arg!="--iterations"&&arg!="--case")throw std::invalid_argument("unknown option: "+arg);
            if(++i>=argc)throw std::invalid_argument("missing value for "+arg);
            if(arg=="--output-dir")directory=argv[i];else if(arg=="--case")selectedCase=argv[i];else iterations=Integer(argv[i]);
        }
        const std::array<std::string,5> names={{"plane","cylinder","sphere","cone","torus"}};
        if(!selectedCase.empty()&&std::find(names.begin(),names.end(),selectedCase)==names.end())throw std::invalid_argument("unknown --case");
        if(pilot&&!selectedCase.empty())throw std::invalid_argument("choose --pilot or --case");
        std::filesystem::create_directories(directory);
        std::ofstream raw(std::filesystem::path(directory)/"raw.csv");
        if(!raw)throw std::runtime_error("cannot open raw.csv");
        raw<<"case,operation,phase,repeat,poles,before_ms,after_ms,validation_ok,max_sampled_geometry_error_mm,area_difference_mm2,area_allowance_mm2,gain_fraction,target_gain_fraction,target_pass,message\n";
        std::vector<Summary> summaries;
        for(int i=0;i<(pilot?1:5);++i) {
            if(!selectedCase.empty()&&names[i]!=selectedCase)continue;
            try {
                const auto fixture=MakeFixture(i);
                const std::array<std::pair<std::string,TopoDS_Shape>,4> receipts={{{"before",fixture.before},{"after",fixture.after},
                    {"section_operand",fixture.sectionPlane},{"clipping_box",fixture.clipBox}}};
                for(const auto& receipt:receipts) {
                    const auto path=std::filesystem::path(directory)/(fixture.name+"_"+receipt.first+".brep");
                    if(!BRepTools::Write(receipt.second,path.string().c_str()))throw std::runtime_error("cannot preserve input BRep receipt");
                }
                const auto beforeDistances=Project(fixture.before,fixture.queries),afterDistances=Project(fixture.after,fixture.queries);
                std::ofstream queryFile(std::filesystem::path(directory)/(fixture.name+"_queries.csv"));
                if(!queryFile)throw std::runtime_error("cannot preserve query receipts");
                queryFile<<"query,x_mm,y_mm,z_mm,before_distance_mm,after_distance_mm,known_normal_offset_mm\n"<<std::setprecision(17);
                for(std::size_t q=0;q<fixture.queries.size();++q) {
                    const auto& point=fixture.queries[q];
                    queryFile<<q<<','<<point.X()<<','<<point.Y()<<','<<point.Z()<<','<<beforeDistances[q]<<','<<afterDistances[q]<<','<<fixture.expectedDistances[q]<<'\n';
                }
                FinishOutput(queryFile,directory+"/"+fixture.name+"_queries.csv");
                for(const std::string operation:{"projection","intersection","boolean"}) {
                    auto result=RunOperation(fixture,operation,iterations,raw,directory);
                    std::cout<<result.name<<' '<<operation<<" valid="<<result.valid<<" gain="<<result.gain<<" target_pass="<<result.pass<<'\n';
                    summaries.push_back(result);
                }
            } catch(const std::exception& e) {
                Summary result;result.name=std::array<std::string,5>{{"plane","cylinder","sphere","cone","torus"}}[i];result.operation="setup";result.valid=false;result.maxError=Inf;result.message=e.what();summaries.push_back(result);
                raw<<result.name<<",setup,setup,-1,0,inf,inf,0,inf,0,0,-inf,0.5,0,"<<Quote(result.message,true)<<'\n';raw.flush();
            }
        }
        if(summaries.empty()) throw std::runtime_error("no benchmark operations were measured");
        bool valid=true,pass=true;for(const auto& s:summaries){valid&=s.valid;pass&=s.pass;}
        std::ofstream json(std::filesystem::path(directory)/"summary.json");
        if(!json||!raw)throw std::runtime_error("cannot write benchmark outputs");
        json<<"{\"backend\":\"OpenCASCADE\",\"geometry_tolerance_mm\":1e-5,\"target_reduction\":0.5,\"warmups\":1,\"measured_repeats\":"<<iterations
            <<",\"all_measurements_valid\":"<<(valid?"true":"false")<<",\"all_operation_targets_pass\":"<<(pass?"true":"false")
            <<",\"scope\":\"One deterministic 100-pole partial finite face per analytic family; same finite faces and query/operand geometry before and after conversion. Projection:25 fixed points; complete trim-edge minima plus periodic-UV-mapped support projections classified inside the finite face (same algorithm before/after), checked against each other and known interior normal-offset distances. Section:finite plane through known interior point. Common:same clipping box. Timings exclude result checking. Pair order alternates. Inputs, operands and first measured Section/Common results are preserved as BRep files outside timed work. 33 edge samples and 25 interior face samples per result compare geometry bidirectionally; Boolean areas use adaptive integration Eps1e-10; area allowance is tolerance*(1+both boundary lengths). Results are sampled correctness checks, not global operation-result certificates. Medians exclude warmup; this workload is not a universal 50% guarantee.\",\"results\":[";
        for(std::size_t i=0;i<summaries.size();++i) {
            const auto& s=summaries[i];if(i)json<<',';
            json<<"{\"case\":"<<Quote(s.name)<<",\"operation\":"<<Quote(s.operation)<<",\"poles\":"<<s.poles<<",\"valid\":"<<(s.valid?"true":"false")<<",\"median_before_ms\":";Number(json,s.before);
            json<<",\"median_after_ms\":";Number(json,s.after);json<<",\"reduction_fraction\":";Number(json,s.gain);
            json<<",\"max_sampled_geometry_error_mm\":";Number(json,s.maxError);json<<",\"target_pass\":"<<(s.pass?"true":"false")<<",\"message\":"<<Quote(s.message)<<'}';
        }
        json<<"]}\n";FinishOutput(raw,directory+"/raw.csv");FinishOutput(json,directory+"/summary.json");
        return valid?0:1;
    } catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 2;}
      catch(...){std::cerr<<"ERROR: geometry kernel exception\n";return 2;}
}
