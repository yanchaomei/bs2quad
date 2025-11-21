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
#include <cmath>
#include <random>
#include <fstream>
#include <sys/stat.h>

namespace BS2Quad {

// 构造函数
TestDataGenerator::TestDataGenerator()
    : m_randomSeed(42)
{
    std::srand(m_randomSeed);
}

// 析构函数
TestDataGenerator::~TestDataGenerator() {
}

// 生成标准圆柱面NURBS
Handle(Geom_BSplineSurface) TestDataGenerator::GenerateCylinderSurface(
    double radius,
    double height,
    int numPointsU,
    int numPointsV
) {
    // 创建控制点网格
    TColgp_Array2OfPnt controlPoints(1, numPointsU, 1, numPointsV);
    TColStd_Array2OfReal weights(1, numPointsU, 1, numPointsV);
    
    // 生成圆柱面控制点
    for (int i = 1; i <= numPointsU; ++i) {
        double angle = 2.0 * M_PI * (i - 1) / (numPointsU - 1);
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        
        for (int j = 1; j <= numPointsV; ++j) {
            double z = height * (j - 1) / (numPointsV - 1);
            controlPoints.SetValue(i, j, gp_Pnt(x, y, z));
            weights.SetValue(i, j, 1.0);
        }
    }
    
    // 生成节点向量
    int degreeU = 2;
    int degreeV = 1;
    
    TColStd_Array1OfReal uKnots = GenerateUniformKnots(numPointsU, degreeU);
    TColStd_Array1OfReal vKnots = GenerateUniformKnots(numPointsV, degreeV);
    
    // 生成重复度
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

// 生成标准圆锥面NURBS
Handle(Geom_BSplineSurface) TestDataGenerator::GenerateConeSurface(
    double radius1,
    double radius2,
    double height,
    int numPointsU,
    int numPointsV
) {
    // 创建控制点网格
    TColgp_Array2OfPnt controlPoints(1, numPointsU, 1, numPointsV);
    TColStd_Array2OfReal weights(1, numPointsU, 1, numPointsV);
    
    // 生成圆锥面控制点
    for (int i = 1; i <= numPointsU; ++i) {
        double angle = 2.0 * M_PI * (i - 1) / (numPointsU - 1);
        
        for (int j = 1; j <= numPointsV; ++j) {
            double t = static_cast<double>(j - 1) / (numPointsV - 1);
            double z = height * t;
            double r = radius1 + (radius2 - radius1) * t;
            double x = r * std::cos(angle);
            double y = r * std::sin(angle);
            
            controlPoints.SetValue(i, j, gp_Pnt(x, y, z));
            weights.SetValue(i, j, 1.0);
        }
    }
    
    // 生成节点向量
    int degreeU = 2;
    int degreeV = 1;
    
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

// 生成标准球面NURBS
Handle(Geom_BSplineSurface) TestDataGenerator::GenerateSphereSurface(
    double radius,
    int numPointsU,
    int numPointsV
) {
    // 创建控制点网格
    TColgp_Array2OfPnt controlPoints(1, numPointsU, 1, numPointsV);
    TColStd_Array2OfReal weights(1, numPointsU, 1, numPointsV);
    
    // 生成球面控制点
    for (int i = 1; i <= numPointsU; ++i) {
        double phi = 2.0 * M_PI * (i - 1) / (numPointsU - 1);  // 经度
        
        for (int j = 1; j <= numPointsV; ++j) {
            double theta = M_PI * (j - 1) / (numPointsV - 1);  // 纬度
            double x = radius * std::sin(theta) * std::cos(phi);
            double y = radius * std::sin(theta) * std::sin(phi);
            double z = radius * std::cos(theta);
            
            controlPoints.SetValue(i, j, gp_Pnt(x, y, z));
            weights.SetValue(i, j, 1.0);
        }
    }
    
    // 生成节点向量
    int degreeU = 2;
    int degreeV = 2;
    
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

// 生成标准平面NURBS
Handle(Geom_BSplineSurface) TestDataGenerator::GeneratePlaneSurface(
    double width,
    double length,
    int numPointsU,
    int numPointsV
) {
    // 创建控制点网格
    TColgp_Array2OfPnt controlPoints(1, numPointsU, 1, numPointsV);
    TColStd_Array2OfReal weights(1, numPointsU, 1, numPointsV);
    
    // 生成平面控制点
    for (int i = 1; i <= numPointsU; ++i) {
        double x = width * (i - 1) / (numPointsU - 1);
        
        for (int j = 1; j <= numPointsV; ++j) {
            double y = length * (j - 1) / (numPointsV - 1);
            controlPoints.SetValue(i, j, gp_Pnt(x, y, 0.0));
            weights.SetValue(i, j, 1.0);
        }
    }
    
    // 生成节点向量
    int degreeU = 1;
    int degreeV = 1;
    
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

// 生成标准圆环面NURBS
Handle(Geom_BSplineSurface) TestDataGenerator::GenerateTorusSurface(
    double majorRadius,
    double minorRadius,
    int numPointsU,
    int numPointsV
) {
    // 创建控制点网格
    TColgp_Array2OfPnt controlPoints(1, numPointsU, 1, numPointsV);
    TColStd_Array2OfReal weights(1, numPointsU, 1, numPointsV);
    
    // 生成圆环面控制点
    for (int i = 1; i <= numPointsU; ++i) {
        double u = 2.0 * M_PI * (i - 1) / (numPointsU - 1);
        
        for (int j = 1; j <= numPointsV; ++j) {
            double v = 2.0 * M_PI * (j - 1) / (numPointsV - 1);
            
            double x = (majorRadius + minorRadius * std::cos(v)) * std::cos(u);
            double y = (majorRadius + minorRadius * std::cos(v)) * std::sin(u);
            double z = minorRadius * std::sin(v);
            
            controlPoints.SetValue(i, j, gp_Pnt(x, y, z));
            weights.SetValue(i, j, 1.0);
        }
    }
    
    // 生成节点向量
    int degreeU = 2;
    int degreeV = 2;
    
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

// 生成带噪声的曲面
Handle(Geom_BSplineSurface) TestDataGenerator::AddNoiseToSurface(
    const Handle(Geom_BSplineSurface)& surface,
    double noiseLevel
) {
    if (surface.IsNull() || noiseLevel <= 0.0) {
        return surface;
    }
    
    // 复制控制点
    int numPolesU = surface->NbUPoles();
    int numPolesV = surface->NbVPoles();
    
    TColgp_Array2OfPnt newControlPoints(1, numPolesU, 1, numPolesV);
    TColStd_Array2OfReal newWeights(1, numPolesU, 1, numPolesV);
    
    // 添加噪声
    for (int i = 1; i <= numPolesU; ++i) {
        for (int j = 1; j <= numPolesV; ++j) {
            gp_Pnt pole = surface->Pole(i, j);
            gp_Vec noise = GenerateRandomNoise(noiseLevel);
            
            newControlPoints.SetValue(i, j, pole.Translated(noise));
            newWeights.SetValue(i, j, surface->Weight(i, j));
        }
    }
    
    // 复制节点向量
    TColStd_Array1OfReal uKnots(1, surface->NbUKnots());
    TColStd_Array1OfReal vKnots(1, surface->NbVKnots());
    TColStd_Array1OfInteger uMults(1, surface->NbUKnots());
    TColStd_Array1OfInteger vMults(1, surface->NbVKnots());
    
    surface->UKnots(uKnots);
    surface->VKnots(vKnots);
    surface->UMultiplicities(uMults);
    surface->VMultiplicities(vMults);
    
    // 创建新的B样条曲面
    Handle(Geom_BSplineSurface) noisySurface = new Geom_BSplineSurface(
        newControlPoints, newWeights, uKnots, vKnots, uMults, vMults,
        surface->UDegree(), surface->VDegree()
    );
    
    return noisySurface;
}

// 生成复杂自由曲面（负样本）
Handle(Geom_BSplineSurface) TestDataGenerator::GenerateFreeFormSurface(
    int numPointsU,
    int numPointsV
) {
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
    std::vector<Handle(Geom_BSplineSurface)> surfaces;
    
    // 创建输出目录
    mkdir(outputDir.c_str(), 0755);
    
    int cylinderCount = count / 5;
    int coneCount = count / 5;
    int sphereCount = count / 5;
    int planeCount = count / 5;
    int freeFormCount = count - (cylinderCount + coneCount + sphereCount + planeCount);
    
    // 生成圆柱面
    for (int i = 0; i < cylinderCount; ++i) {
        double radius = 1.0 + (std::rand() % 100) / 50.0;
        double height = 5.0 + (std::rand() % 100) / 20.0;
        int points = 8 + std::rand() % 12;
        
        Handle(Geom_BSplineSurface) surf = GenerateCylinderSurface(
            radius, height, points, points
        );
        surfaces.push_back(surf);
        
        std::string filename = outputDir + "/cylinder_" + std::to_string(i) + ".step";
        ExportToSTEP(surf, filename);
    }
    
    // 生成圆锥面
    for (int i = 0; i < coneCount; ++i) {
        double radius1 = 1.0 + (std::rand() % 100) / 50.0;
        double radius2 = 0.1 + (std::rand() % 50) / 50.0;
        double height = 5.0 + (std::rand() % 100) / 20.0;
        int points = 8 + std::rand() % 12;
        
        Handle(Geom_BSplineSurface) surf = GenerateConeSurface(
            radius1, radius2, height, points, points
        );
        surfaces.push_back(surf);
        
        std::string filename = outputDir + "/cone_" + std::to_string(i) + ".step";
        ExportToSTEP(surf, filename);
    }
    
    // 生成球面
    for (int i = 0; i < sphereCount; ++i) {
        double radius = 1.0 + (std::rand() % 100) / 50.0;
        int points = 8 + std::rand() % 12;
        
        Handle(Geom_BSplineSurface) surf = GenerateSphereSurface(
            radius, points, points
        );
        surfaces.push_back(surf);
        
        std::string filename = outputDir + "/sphere_" + std::to_string(i) + ".step";
        ExportToSTEP(surf, filename);
    }
    
    // 生成平面
    for (int i = 0; i < planeCount; ++i) {
        double width = 5.0 + (std::rand() % 100) / 20.0;
        double length = 5.0 + (std::rand() % 100) / 20.0;
        int points = 4 + std::rand() % 6;
        
        Handle(Geom_BSplineSurface) surf = GeneratePlaneSurface(
            width, length, points, points
        );
        surfaces.push_back(surf);
        
        std::string filename = outputDir + "/plane_" + std::to_string(i) + ".step";
        ExportToSTEP(surf, filename);
    }
    
    // 生成自由曲面
    for (int i = 0; i < freeFormCount; ++i) {
        int points = 10 + std::rand() % 10;
        
        Handle(Geom_BSplineSurface) surf = GenerateFreeFormSurface(points, points);
        surfaces.push_back(surf);
        
        std::string filename = outputDir + "/freeform_" + std::to_string(i) + ".step";
        ExportToSTEP(surf, filename);
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
    std::srand(seed);
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
    double x = ((std::rand() % 2000) - 1000) / 1000.0 * magnitude;
    double y = ((std::rand() % 2000) - 1000) / 1000.0 * magnitude;
    double z = ((std::rand() % 2000) - 1000) / 1000.0 * magnitude;
    
    return gp_Vec(x, y, z);
}

} // namespace BS2Quad

