#include <STEPControl_Writer.hxx>
#include <STEPControl_Controller.hxx>
#include <XSControl_WorkSession.hxx>
#include <XSAlgo.hxx>
#include <XSAlgo_AlgoContainer.hxx>
#include <Standard.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>
#include <iostream>
#include <string>
#include <cstdlib>

// Pure pinned OCCT7.7 API reproduction. No BS2Quad code or headers.
int main(int argc,char** argv) {
    const std::string mode=argc>1?argv[1]:"writes";
    const int count=argc>2?std::atoi(argv[2]):1;
    std::cout<<"PROBE mode="<<mode<<" count="<<count<<std::endl;
    if(mode=="no_step")return 0;
    if(mode=="controller"){STEPControl_Controller::Init();return 0;}
    if(mode=="init"){STEPControl_Writer writer;return 0;}
    const auto face=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),-2.0,3.0,-4.0,5.0).Face();
    for(int i=0;i<count;++i) {
        STEPControl_Writer writer;
        if(writer.Transfer(face,STEPControl_AsIs)!=IFSelect_RetDone)return 10;
        if(writer.Write("/work/evidence/occt-step-control/probe.step")!=IFSelect_RetDone)return 11;
        if(mode=="clear1"||mode=="clear7") {
            writer.WS()->ClearData(mode=="clear1"?1:7);
            writer.WS()->ClearContext();
        }
    }
    if(mode=="purge")std::cout<<"Purge="<<Standard::Purge()<<std::endl;
    if(mode=="nullalg")XSAlgo::SetAlgoContainer(Handle(XSAlgo_AlgoContainer)());
    std::cout<<"PROBE completed writes="<<count<<std::endl;
    return 0;
}
