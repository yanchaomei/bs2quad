#include "CheckedOutput.h"
#include "BS2QuadCore.h"
#include "TestDataGenerator.h"
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRep_Tool.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>

using namespace BS2Quad;
namespace {
void Usage() {
    std::cout<<"BS2Quad "<<BS2QuadCore::GetVersion()<<" (OpenCASCADE; SGK 1.7 not integrated)\n"
      <<"  --demo [--tolerance 1e-5] [--perf]\n"
      <<"  --generate N [--output-dir data/generated] [--seed 42]\n"
      <<"  --input face.step|face.brep --output converted.step|converted.brep [--report result.json]\n"
      <<"File conversion accepts exactly one trimmed NURBS face. Multi-face model replacement requires explicit neighbor context.\n";
}
std::string Quote(const std::string& text) {
    std::string out="\"";
    for(unsigned char c:text) {
        if(c=='"'||c=='\\') {out+='\\';out+=static_cast<char>(c);}
        else if(c=='\n') out+="\\n";
        else if(c=='\r') out+="\\r";
        else if(c=='\t') out+="\\t";
        else if(c<32) out+='?';
        else out+=static_cast<char>(c);
    }
    return out+'"';
}
void Number(std::ostream& out,double value) {if(std::isfinite(value)) out<<std::setprecision(17)<<value;else out<<"null";}
void Report(std::ostream& out,const ConversionResult& r) {
    out<<"{\"backend\":\"OpenCASCADE\",\"sgk_1_7_verified\":false,\"success\":"<<(r.success?"true":"false")
       <<",\"type\":"<<Quote(QuadricTypeToString(r.quadricType))<<",\"error\":"<<Quote(ErrorTypeToString(r.error))
       <<",\"message\":"<<Quote(r.message)<<",\"decision_ms\":";Number(out,r.decisionTime);
    out<<",\"conversion_ms\":";Number(out,r.conversionTime);
    out<<",\"directed_support_bound_mm\":";Number(out,r.accuracyResult.certifiedMaxDistance);
    out<<",\"sampled_max_mm\":";Number(out,r.accuracyResult.maxDistance);
    out<<",\"sampled_boundary_hausdorff_mm\":";Number(out,r.accuracyResult.hausdorffDistance);
    out<<",\"normal_deviation_degrees\":";Number(out,r.accuracyResult.normalDeviationDegrees);
    out<<",\"adjacency_checked\":"<<(r.accuracyResult.adjacencyChecked?"true":"false")<<"}\n";
}
int Integer(const std::string& text,int lo,int hi) {
    if(text.empty()||text.find_first_not_of("0123456789")!=std::string::npos) throw std::invalid_argument("expected a nonnegative integer: "+text);
    std::size_t end=0;const auto value=std::stoll(text,&end);
    if(end!=text.size()||value<lo||value>hi) throw std::invalid_argument("integer outside allowed range: "+text);
    return static_cast<int>(value);
}
TopoDS_Face ReadFace(const std::string& filename) {
    if(!std::filesystem::is_regular_file(filename)) throw std::runtime_error("INPUT_READ_FAILED: file does not exist: "+filename);
    TopoDS_Shape shape;
    const auto extension=std::filesystem::path(filename).extension().string();
    if(extension==".brep") {
        BRep_Builder builder;
        if(!BRepTools::Read(shape,filename.c_str(),builder)) throw std::runtime_error("INPUT_CORRUPT: failed to parse BRep: "+filename);
    } else if(extension==".step"||extension==".stp") {
        STEPControl_Reader reader;
        if(reader.ReadFile(filename.c_str())!=IFSelect_RetDone||reader.TransferRoots()<1) throw std::runtime_error("INPUT_CORRUPT: failed to parse STEP: "+filename);
        shape=reader.OneShape();
    } else throw std::invalid_argument("unsupported input extension (use .step, .stp or .brep)");
    if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid()) throw std::runtime_error("INPUT_CORRUPT: invalid topology: "+filename);
    TopoDS_Face face;int count=0;
    for(TopExp_Explorer e(shape,TopAbs_FACE);e.More();e.Next()) {face=TopoDS::Face(e.Current());++count;}
    if(count!=1) throw std::runtime_error("INPUT_SCOPE: expected exactly one face; received "+std::to_string(count));
    return face;
}
void WriteFace(const TopoDS_Face& face,const std::string& filename) {
    const auto extension=std::filesystem::path(filename).extension().string();
    if(extension==".brep") {
        if(!BRepTools::Write(face,filename.c_str())) throw std::runtime_error("OUTPUT_WRITE_FAILED: "+filename);
    } else if(extension==".step"||extension==".stp") {
        STEPControl_Writer writer;
        if(writer.Transfer(face,STEPControl_AsIs)!=IFSelect_RetDone||writer.Write(filename.c_str())!=IFSelect_RetDone) throw std::runtime_error("OUTPUT_WRITE_FAILED: "+filename);
    } else throw std::invalid_argument("unsupported output extension (use .step, .stp or .brep)");
}
}
int main(int argc,char** argv) {
    try {
        bool demo=argc==1,perf=false;int generate=0,seed=42;double tolerance=1e-5;
        std::string input,output,report,directory="data/generated";
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            const auto value=[&]() {if(i+1>=argc) throw std::invalid_argument("missing value for "+arg);return std::string(argv[++i]);};
            if(arg=="--help"||arg=="-h") {Usage();return 0;}
            else if(arg=="--demo"||arg=="-d") demo=true;
            else if(arg=="--perf"||arg=="-p") perf=true;
            else if(arg=="--generate"||arg=="-g") generate=Integer(value(),1,100000);
            else if(arg=="--seed") seed=Integer(value(),0,std::numeric_limits<int>::max());
            else if(arg=="--tolerance"||arg=="-t") {
                const auto text=value();std::size_t end=0;tolerance=std::stod(text,&end);
                if(end!=text.size()||!std::isfinite(tolerance)||tolerance<=0) throw std::invalid_argument("tolerance must be finite and positive (mm)");
            } else if(arg=="--input") input=value();
            else if(arg=="--output") output=value();
            else if(arg=="--report") report=value();
            else if(arg=="--output-dir") directory=value();
            else throw std::invalid_argument("unknown option: "+arg);
        }
        if(static_cast<int>(demo)+(generate>0)+!input.empty()!=1) throw std::invalid_argument("choose exactly one of --demo, --generate, --input");
        TestDataGenerator generator;generator.SetRandomSeed(static_cast<unsigned>(seed));
        if(generate>0) {
            const auto sources=generator.GenerateBatchTestData(generate,directory);
            std::cout<<"generated="<<sources.size()<<"; directory="<<directory<<"; seed="<<seed<<"\n";return 0;
        }
        BS2QuadCore core(tolerance);core.SetEnablePerformanceTest(perf);
        if(!input.empty()) {
            if(output.empty()) throw std::invalid_argument("--input requires --output");
            const auto sameFile=[](const std::string& a,const std::string& b) {
                return std::filesystem::weakly_canonical(std::filesystem::absolute(a))==std::filesystem::weakly_canonical(std::filesystem::absolute(b))||
                    (std::filesystem::exists(a)&&std::filesystem::exists(b)&&std::filesystem::equivalent(a,b));
            };
            if(sameFile(input,output)||(!report.empty()&&(sameFile(input,report)||sameFile(output,report))))
                throw std::invalid_argument("input, output and report must refer to distinct files");
            const auto face=ReadFace(input);
            const auto result=core.Convert(face);
            if(result.success) WriteFace(result.quadricFace,output);
            Report(std::cout,result);
            if(!report.empty()) {std::ofstream file(report);if(!file) throw std::runtime_error("cannot open report: "+report);Report(file,result);FinishOutput(file,report);}
            return result.success?0:1;
        }
        const std::vector<Handle(Geom_BSplineSurface)> sources={
            generator.GeneratePlaneSurface(8,9,10,10),generator.GenerateCylinderSurface(5,12,10,10),
            generator.GenerateConeSurface(5,2,12,10,10),generator.GenerateSphereSurface(5,10,10),
            generator.GenerateTorusSurface(8,2,10,10),generator.GenerateFreeFormSurface(10,10)};
        bool valid=true;
        for(std::size_t i=0;i<sources.size();++i) {const auto result=core.Convert(sources[i]);Report(std::cout,result);valid&=result.success==(i<5);}
        std::cout<<core.GetStatistics();return valid?0:1;
    } catch(const Standard_Failure& failure) {
        std::cerr<<"KERNEL_FAILURE: "<<(failure.GetMessageString()?failure.GetMessageString():"OCCT exception")<<"\n";return 2;
    } catch(const std::exception& failure) {std::cerr<<"ERROR: "<<failure.what()<<"\n";return 2;}
}
