#include "CheckedOutput.h"
#include "BS2QuadCore.h"
#include "TestDataGenerator.h"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepTools.hxx>
#include <Standard_Version.hxx>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <gp_Trsf.hxx>

using namespace BS2Quad;
namespace {
std::string CSV(const std::string& text) {std::string out="\"";for(char c:text) {if(c=='"') out+='"';out+=c;}return out+'"';}
double Quantile(std::vector<double> data,double p) {if(data.empty()) return 0;std::sort(data.begin(),data.end());return data[static_cast<std::size_t>(std::ceil(p*data.size()))-1];}
int PositiveInt(const std::string& text) {
    if(text.empty()||text.find_first_not_of("0123456789")!=std::string::npos) throw std::invalid_argument("invalid integer");
    const auto value=std::stoll(text);if(value<1||value>100000) throw std::invalid_argument("count must be in [1,100000]");return static_cast<int>(value);
}
}
int main(int argc,char** argv) {
    try {
        int count=1000;std::string directory="evidence/run";bool saveInputs=true;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--no-save-inputs") {saveInputs=false;continue;}
            if(i+1>=argc) throw std::invalid_argument("missing option value");
            if(arg=="--count") count=PositiveInt(argv[++i]);
            else if(arg=="--output-dir") directory=argv[++i];
            else throw std::invalid_argument("unknown option: "+arg);
        }
        std::filesystem::create_directories(directory);
        if(saveInputs) std::filesystem::create_directories(directory+"/inputs");
        std::ofstream rows(directory+"/cases.csv"),manifest(directory+"/inputs.csv");
        if(!rows||!manifest) throw std::runtime_error("cannot open evidence outputs");
        rows<<"id,expected_type,actual_type,control_points,expected_accept,accepted,repeat_consistent,decision_ms,total_ms,sampled_max_mm,sampled_mean_mm,directed_bound_mm,certificate_available,error,message\n";
        manifest<<"id,family,radius,height,requested_u_poles,requested_v_poles,rotation_rad,translation_x_mm,translation_y_mm,translation_z_mm,input_file\n";
        TestDataGenerator generator;generator.SetRandomSeed(42);BS2QuadCore core;
        int truePositive=0,trueNegative=0,falsePositive=0,falseNegative=0,inconsistent=0,outsideControlRange=0;
        int minPoles=std::numeric_limits<int>::max(),maxPoles=0;
        double maximumBound=0;std::vector<double> decisions,totals;
        const QuadricType types[]={QuadricType::PLANE,QuadricType::CYLINDER,QuadricType::CONE,QuadricType::SPHERE,QuadricType::TORUS,QuadricType::NONE};
        for(int i=0;i<count;++i) {
            // Include exact lower/upper limits and a near-square high-density
            // grid, instead of testing only the middle of the 50--200 range.
            const int grids[][2]={{10,5},{10,10},{14,14},{20,10},{10,20},{15,13},{12,16},{11,17}};
            const int family=i%6,grid=(i/6)%8,nu=grids[grid][0],nv=grids[grid][1];
            const double radius=2.0+(i%47)*0.13,height=6.0+(i%31)*0.17;
            Handle(Geom_BSplineSurface) source;
            switch(family) {
            case 0:source=generator.GeneratePlaneSurface(radius*2,height,nu,nv);break;
            case 1:source=generator.GenerateCylinderSurface(radius,height,nu,nv);break;
            case 2:source=generator.GenerateConeSurface(radius,radius*0.4,height,nu,nv);break;
            case 3:source=generator.GenerateSphereSurface(radius,nu,nv);break;
            case 4:source=generator.GenerateTorusSurface(radius*2.5,radius*0.35,nu,nv);break;
            default:source=generator.GenerateFreeFormSurface(nu,nv);break;
            }
            const double angle=(i%53)*0.037;
            const double x=(i%29)*1.3,y=-(i%37)*0.7,z=(i%19)*0.41;
            gp_Trsf rotation,translation;rotation.SetRotation(gp_Ax1(gp_Pnt(),gp_Dir(1,2,3)),angle);translation.SetTranslation(gp_Vec(x,y,z));
            source->Transform(rotation);source->Transform(translation);
            const int poles=source->NbUPoles()*source->NbVPoles();
            minPoles=std::min(minPoles,poles);maxPoles=std::max(maxPoles,poles);
            outsideControlRange+=poles<50||poles>200;
            const std::string inputFile=saveInputs?"inputs/case_"+std::to_string(i)+".brep":"not_saved";
            if(saveInputs) {
                BRepBuilderAPI_MakeFace face(source,1e-7);
                if(!face.IsDone()||!BRepTools::Write(face.Face(),(directory+"/"+inputFile).c_str())) throw std::runtime_error("cannot save input "+std::to_string(i));
            }
            manifest<<std::setprecision(17)<<i<<','<<QuadricTypeToString(types[family])<<','<<radius<<','<<height<<','<<nu<<','<<nv<<','<<angle<<','<<x<<','<<y<<','<<z<<','<<inputFile<<'\n';
            const auto first=core.Convert(source),again=core.Convert(source);
            const bool repeat=first.success==again.success&&first.quadricType==again.quadricType&&first.error==again.error&&
                first.accuracyResult.certifiedMaxDistance==again.accuracyResult.certifiedMaxDistance&&first.accuracyResult.maxDistance==again.accuracyResult.maxDistance;
            if(!repeat) ++inconsistent;
            const bool expected=family<5;
            if(expected&&first.success&&first.quadricType==types[family]) ++truePositive;
            else if(expected) ++falseNegative;
            else if(first.success) ++falsePositive;
            else ++trueNegative;
            if(first.success) maximumBound=std::max(maximumBound,first.accuracyResult.certifiedMaxDistance);
            decisions.push_back(first.decisionTime);totals.push_back(first.conversionTime);
            rows<<std::setprecision(17)<<i<<','<<QuadricTypeToString(types[family])<<','<<QuadricTypeToString(first.quadricType)<<','<<poles<<','<<expected<<','<<first.success<<','<<repeat<<','<<first.decisionTime<<','<<first.conversionTime<<','<<first.accuracyResult.maxDistance<<','<<first.accuracyResult.avgDistance<<','<<first.accuracyResult.certifiedMaxDistance<<','<<first.accuracyResult.maxDistanceCertified<<','<<ErrorTypeToString(first.error)<<','<<CSV(first.message)<<'\n';
            if((i+1)%100==0) {rows.flush();manifest.flush();std::cout<<"processed="<<i+1<<'/'<<count<<std::endl;}
        }
        FinishOutput(rows,directory+"/cases.csv");
        FinishOutput(manifest,directory+"/inputs.csv");
        std::ofstream summary(directory+"/summary.json");
        summary<<std::setprecision(17)<<"{\n\"schema\":1,\"backend\":\"OpenCASCADE\",\"backend_version\":\""<<OCC_VERSION_COMPLETE
            <<"\",\"sgk_1_7_verified\":false,\"scope\":\"synthetic directed support conversion, not a complete face/adjacency benchmark\",\n"
            <<"\"seed\":42,\"count\":"<<count<<",\"true_positive\":"<<truePositive<<",\"true_negative\":"<<trueNegative
            <<",\"false_positive\":"<<falsePositive<<",\"false_negative\":"<<falseNegative<<",\"inconsistent\":"<<inconsistent
            <<",\"outside_50_200_control_points\":"<<outsideControlRange<<",\"maximum_accepted_directed_bound_mm\":"<<maximumBound
            <<",\"control_points_min\":"<<minPoles<<",\"control_points_max\":"<<maxPoles
            <<",\"decision_mean_ms\":"<<std::accumulate(decisions.begin(),decisions.end(),0.0)/count
            <<",\"decision_p50_ms\":"<<Quantile(decisions,0.50)<<",\"decision_p95_ms\":"<<Quantile(decisions,0.95)
            <<",\"decision_p99_ms\":"<<Quantile(decisions,0.99)<<",\"decision_max_ms\":"<<*std::max_element(decisions.begin(),decisions.end())
            <<",\"decision_10ms_pass\":"<<(*std::max_element(decisions.begin(),decisions.end())<=10?"true":"false")
            <<",\"cad_operations_50_percent_verified\":false,\"inputs_saved\":"<<(saveInputs?"true":"false")<<"\n}\n";
        FinishOutput(summary,directory+"/summary.json");
        std::cout<<"TP="<<truePositive<<" TN="<<trueNegative<<" FP="<<falsePositive<<" FN="<<falseNegative<<" inconsistent="<<inconsistent<<"\n";
        return falsePositive||falseNegative||inconsistent||outsideControlRange?1:0;
    } catch(const Standard_Failure& failure) {
        std::cerr<<"KERNEL_FAILURE: "<<(failure.GetMessageString()?failure.GetMessageString():"OCCT exception")<<"\n";return 2;
    } catch(const std::exception& failure) {std::cerr<<"EVIDENCE_FAILURE: "<<failure.what()<<"\n";return 2;}
}
