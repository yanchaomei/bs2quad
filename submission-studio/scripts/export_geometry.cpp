// Read-only visualization helper. Samples evaluated BRep supports, not a certificate.
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <Geom_Surface.hxx>
#include <TopLoc_Location.hxx>
#include <fstream>
#include <iomanip>
#include <stdexcept>
int main(int argc,char** argv) {
 if(argc!=3)return 2;
 TopoDS_Shape shape;BRep_Builder b;if(!BRepTools::Read(shape,argv[1],b))return 3;
 TopExp_Explorer x(shape,TopAbs_FACE);if(!x.More())return 4;auto face=TopoDS::Face(x.Current());
 TopLoc_Location loc;auto surf=BRep_Tool::Surface(face,loc);double u0,u1,v0,v1;BRepTools::UVBounds(face,u0,u1,v0,v1);
 std::ofstream f(argv[2]);f<<std::setprecision(17)<<"{\"scope\":\"BRep support evaluation for visualization only\",\"rows\":31,\"cols\":61,\"points\":[";
 for(int j=0;j<31;++j)for(int i=0;i<61;++i){auto p=surf->Value(u0+(u1-u0)*i/60.0,v0+(v1-v0)*j/30.0).Transformed(loc.Transformation());if(i||j)f<<',';f<<'['<<p.X()<<','<<p.Y()<<','<<p.Z()<<']';}
 f<<"]}";f.flush();if(!f)return 5;return 0;
}
