/**
 * @file TestDataGenerator.cpp
 * @brief 测试数据生成器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "TestDataGenerator.h"
#include <Geom_BSplineSurface.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array2OfReal.hxx>
#include <STEPControl_Writer.hxx>
#include <IGESControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <BRep_Tool.hxx>
#include <cmath>
#include <GeomConvert.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <Geom_Plane.hxx>
#include <filesystem>
#include <random>
#include <fstream>
#include <sys/stat.h>

namespace BS2Quad {

namespace {
void ValidateGrid(int u, int v) {
    if (u < 2 || v < 2 || u > 256 || v > 256)
        throw std::invalid_argument("control grid must be in [2,256] per direction");
}
void Positive(double value, const char* name) {
    if (!std::isfinite(value) || value <= 0.0) throw std::invalid_argument(name);
}
// Inserting a knot preserves the rational surface exactly. Requested counts
// below the canonical representation minimum are raised to that minimum.
Handle(Geom_BSplineSurface) Refine(const Handle(Geom_Surface)& analytic,
    double u0, double u1, double v0, double v1, int nu, int nv) {
    ValidateGrid(nu,nv);
    auto result=GeomConvert::SurfaceToBSplineSurface(
        new Geom_RectangularTrimmedSurface(analytic,u0,u1,v0,v1));
    // Canonical clamped fixtures make the full-domain proof independent of a
    // periodic seam convention. The analytic residual is checked in tests.
    if(result->IsUPeriodic()) result->SetUNotPeriodic();
    if(result->IsVPeriodic()) result->SetVNotPeriodic();
    result->Segment(u0,u1,v0,v1);
    for (int direction=0; direction<2; ++direction) {
        const int target=direction==0?nu:nv;
        while ((direction==0?result->NbUPoles():result->NbVPoles()) < target) {
            const int count=direction==0?result->NbUKnots():result->NbVKnots();
            double largest=-1.0, middle=0.0;
            for (int i=1;i<count;++i) {
                const double lo=direction==0?result->UKnot(i):result->VKnot(i);
                const double hi=direction==0?result->UKnot(i+1):result->VKnot(i+1);
                if (hi-lo>largest) { largest=hi-lo; middle=(lo+hi)*0.5; }
            }
            if (direction==0) result->InsertUKnot(middle,1,1e-12);
            else result->InsertVKnot(middle,1,1e-12);
        }
    }
    return result;
}
}

TestDataGenerator::TestDataGenerator() : m_rng(42), m_randomSeed(42) {}
TestDataGenerator::~TestDataGenerator() = default;

Handle(Geom_BSplineSurface) TestDataGenerator::GenerateCylinderSurface(
    double radius,double height,int nu,int nv) {
    Positive(radius,"radius must be finite and positive");
    Positive(height,"height must be finite and positive");
    return Refine(new Geom_CylindricalSurface(gp_Ax3(),radius),
        0.0,2.0*M_PI,0.0,height,nu,nv);
}

Handle(Geom_BSplineSurface) TestDataGenerator::GenerateConeSurface(
    double r1,double r2,double height,int nu,int nv) {
    Positive(r1,"base radius must be finite and positive");
    Positive(height,"height must be finite and positive");
    if (!std::isfinite(r2)||r2<0.0) throw std::invalid_argument("top radius must be finite and nonnegative");
    if (r1==r2) return GenerateCylinderSurface(r1,height,nu,nv);
    const double angle=std::atan2(r2-r1,height);
    return Refine(new Geom_ConicalSurface(gp_Ax3(),angle,r1),
        0.0,2.0*M_PI,0.0,std::hypot(height,r2-r1),nu,nv);
}

Handle(Geom_BSplineSurface) TestDataGenerator::GenerateSphereSurface(
    double radius,int nu,int nv) {
    Positive(radius,"radius must be finite and positive");
    return Refine(new Geom_SphericalSurface(gp_Ax3(),radius),
        0.0,2.0*M_PI,-M_PI/2.0,M_PI/2.0,nu,nv);
}

Handle(Geom_BSplineSurface) TestDataGenerator::GeneratePlaneSurface(
    double width,double length,int nu,int nv) {
    Positive(width,"width must be finite and positive");
    Positive(length,"length must be finite and positive");
    return Refine(new Geom_Plane(gp_Ax3()),0.0,width,0.0,length,nu,nv);
}

Handle(Geom_BSplineSurface) TestDataGenerator::GenerateTorusSurface(
    double majorRadius,double minorRadius,int nu,int nv) {
    Positive(majorRadius,"major radius must be finite and positive");
    Positive(minorRadius,"minor radius must be finite and positive");
    if (minorRadius>=majorRadius) throw std::invalid_argument("ring torus requires major radius > minor radius");
    return Refine(new Geom_ToroidalSurface(gp_Ax3(),majorRadius,minorRadius),
        0.0,2.0*M_PI,0.0,2.0*M_PI,nu,nv);
}

Handle(Geom_BSplineSurface) TestDataGenerator::AddNoiseToSurface(
    const Handle(Geom_BSplineSurface)& surface,double amplitude) {
    if (surface.IsNull()) return {};
    if (!std::isfinite(amplitude)||amplitude<0.0) throw std::invalid_argument("noise amplitude must be finite and nonnegative (mm)");
    auto copy=Handle(Geom_BSplineSurface)::DownCast(surface->Copy());
    for (int i=1;i<=copy->NbUPoles();++i)
        for (int j=1;j<=copy->NbVPoles();++j)
            copy->SetPole(i,j,copy->Pole(i,j).Translated(GenerateRandomNoise(amplitude)));
    return copy;
}

// 生成复杂自由曲面（负样本）
Handle(Geom_BSplineSurface) TestDataGenerator::GenerateFreeFormSurface(
    int numPointsU,
    int numPointsV
) {
    ValidateGrid(numPointsU,numPointsV);
    if (numPointsU<4||numPointsV<4) throw std::invalid_argument("cubic freeform needs at least 4 poles per direction");
    // 创建控制点网格（随机波浪形）
    TColgp_Array2OfPnt controlPoints(1, numPointsU, 1, numPointsV);
    TColStd_Array2OfReal weights(1, numPointsU, 1, numPointsV);
    
    for (int i = 1; i <= numPointsU; ++i) {
        double x = 10.0 * (i - 1) / (numPointsU - 1);
        
        for (int j = 1; j <= numPointsV; ++j) {
            double y = 10.0 * (j - 1) / (numPointsV - 1);
            
            // 复杂的Z坐标（多个正弦波叠加）
            double z = std::sin(x) * std::cos(y) + 
                       0.5 * std::sin(2 * x) * std::sin(2 * y) +
                       0.3 * std::cos(3 * x + 2 * y);
            
            controlPoints.SetValue(i, j, gp_Pnt(x, y, z));
            weights.SetValue(i, j, 1.0);
        }
    }
    
    // 生成节点向量（高阶）
    int degreeU = 3;
    int degreeV = 3;
    
    TColStd_Array1OfReal uKnots = GenerateUniformKnots(numPointsU, degreeU);
    TColStd_Array1OfReal vKnots = GenerateUniformKnots(numPointsV, degreeV);
    
    TColStd_Array1OfInteger uMults(1, uKnots.Length());
    TColStd_Array1OfInteger vMults(1, vKnots.Length());
    
    for (int i = 1; i <= uKnots.Length(); ++i) {
        uMults.SetValue(i, (i == 1 || i == uKnots.Length()) ? (degreeU + 1) : 1);
    }
    
    for (int i = 1; i <= vKnots.Length(); ++i) {
        vMults.SetValue(i, (i == 1 || i == vKnots.Length()) ? (degreeV + 1) : 1);
    }
    
    // 创建B样条曲面
    Handle(Geom_BSplineSurface) surface = new Geom_BSplineSurface(
        controlPoints, weights, uKnots, vKnots, uMults, vMults,
        degreeU, degreeV
    );
    
    return surface;
}

// 批量生成测试数据
std::vector<Handle(Geom_BSplineSurface)> TestDataGenerator::GenerateBatchTestData(
    int count,
    const std::string& outputDir
) {
    if(count<0||count>100000) throw std::invalid_argument("batch count must be in [0,100000]");
    std::filesystem::create_directories(outputDir);
    std::vector<Handle(Geom_BSplineSurface)> surfaces;
    surfaces.reserve(count);
    const char* names[]={"plane","cylinder","cone","sphere","torus","freeform"};
    for(int i=0;i<count;++i) {
        const int u=9+static_cast<int>(m_rng()%5),v=6+static_cast<int>(m_rng()%8);
        const double r=2.0+(m_rng()%100)*0.05,h=5.0+(m_rng()%100)*0.1;
        Handle(Geom_BSplineSurface) surface;
        switch(i%6) {
        case 0:surface=GeneratePlaneSurface(2*r,h,u,v);break;
        case 1:surface=GenerateCylinderSurface(r,h,u,v);break;
        case 2:surface=GenerateConeSurface(r,r*0.4,h,u,v);break;
        case 3:surface=GenerateSphereSurface(r,u,v);break;
        case 4:surface=GenerateTorusSurface(r*2.5,r*0.35,u,v);break;
        default:surface=GenerateFreeFormSurface(u,v);break;
        }
        const std::string filename=outputDir+"/"+names[i%6]+"_"+std::to_string(i)+".step";
        if(!ExportToSTEP(surface,filename)) throw std::runtime_error("STEP export failed: "+filename);
        surfaces.push_back(surface);
    }
    return surfaces;
}

// 导出曲面到STEP文件
bool TestDataGenerator::ExportToSTEP(
    const Handle(Geom_BSplineSurface)& surface,
    const std::string& filename
) {
    if (surface.IsNull()) {
        return false;
    }
    
    try {
        // 创建拓扑面
        BRepBuilderAPI_MakeFace faceMaker(surface, Precision::DEFAULT_TOLERANCE);
        if (!faceMaker.IsDone()) {
            return false;
        }
        
        TopoDS_Face face = faceMaker.Face();
        
        // 导出STEP
        STEPControl_Writer writer;
        writer.Transfer(face, STEPControl_AsIs);
        IFSelect_ReturnStatus status = writer.Write(filename.c_str());
        
        return status == IFSelect_RetDone;
    } catch (...) {
        return false;
    }
}

// 导出曲面到IGES文件
bool TestDataGenerator::ExportToIGES(
    const Handle(Geom_BSplineSurface)& surface,
    const std::string& filename
) {
    if (surface.IsNull()) {
        return false;
    }
    
    try {
        // 创建拓扑面
        BRepBuilderAPI_MakeFace faceMaker(surface, Precision::DEFAULT_TOLERANCE);
        if (!faceMaker.IsDone()) {
            return false;
        }
        
        TopoDS_Face face = faceMaker.Face();
        
        // 导出IGES
        IGESControl_Writer writer;
        writer.AddShape(face);
        Standard_Boolean status = writer.Write(filename.c_str());
        
        return status;
    } catch (...) {
        return false;
    }
}

// 从STEP文件导入曲面
Handle(Geom_BSplineSurface) TestDataGenerator::ImportFromSTEP(
    const std::string& filename
) {
    try {
        STEPControl_Reader reader;
        IFSelect_ReturnStatus status = reader.ReadFile(filename.c_str());
        
        if (status != IFSelect_RetDone) {
            return Handle(Geom_BSplineSurface)();
        }
        
        reader.TransferRoots();
        TopoDS_Shape shape = reader.OneShape();
        
        // 提取第一个面
        TopExp_Explorer explorer(shape, TopAbs_FACE);
        if (explorer.More()) {
            TopoDS_Face face = TopoDS::Face(explorer.Current());
            Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
            
            return Handle(Geom_BSplineSurface)::DownCast(surface);
        }
    } catch (...) {
        // 忽略错误
    }
    
    return Handle(Geom_BSplineSurface)();
}

// 设置随机种子
void TestDataGenerator::SetRandomSeed(unsigned int seed) {
    m_randomSeed = seed;
    m_rng.seed(seed);
}

// 生成均匀节点向量
TColStd_Array1OfReal TestDataGenerator::GenerateUniformKnots(
    int numControlPoints,
    int degree
) {
    int numKnots = numControlPoints - degree + 1;
    TColStd_Array1OfReal knots(1, numKnots);
    
    for (int i = 1; i <= numKnots; ++i) {
        knots.SetValue(i, static_cast<double>(i - 1) / (numKnots - 1));
    }
    
    return knots;
}

// 生成随机噪声
gp_Vec TestDataGenerator::GenerateRandomNoise(double magnitude) {
    std::uniform_real_distribution<double> distribution(-magnitude,magnitude);
    double x=distribution(m_rng), y=distribution(m_rng), z=distribution(m_rng);
    
    return gp_Vec(x, y, z);
}

} // namespace BS2Quad

