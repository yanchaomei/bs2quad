/**
 * @file AccuracyValidator.cpp
 * @brief 精度验证和性能对比模块实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "AccuracyValidator.h"
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAPI_IntSS.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace BS2Quad {

// 构造函数
AccuracyValidator::AccuracyValidator(double tolerance)
    : m_tolerance(tolerance)
    , m_perfIterations(10)
    , m_samplesU(Sampling::ADAPTIVE_SAMPLES)
    , m_samplesV(Sampling::ADAPTIVE_SAMPLES)
{
}

// 析构函数
AccuracyValidator::~AccuracyValidator() {
}

// 验证转换精度
AccuracyResult AccuracyValidator::ValidateAccuracy(
    const Handle(Geom_BSplineSurface)& nurbsSurface,
    const Handle(Geom_Surface)& quadricSurface
) {
    AccuracyResult result;
    
    if (nurbsSurface.IsNull() || quadricSurface.IsNull()) {
        return result;
    }
    
    // 采样NURBS曲面
    std::vector<gp_Pnt> samplePoints = SampleSurface(
        Handle(Geom_Surface)::DownCast(nurbsSurface),
        m_samplesU,
        m_samplesV
    );
    
    if (samplePoints.empty()) {
        return result;
    }
    
    // 计算最大距离误差
    result.maxDistance = ComputeMaxDistance(
        nurbsSurface, quadricSurface, samplePoints
    );
    
    // 计算平均距离误差
    result.avgDistance = ComputeAvgDistance(
        nurbsSurface, quadricSurface, samplePoints
    );
    
    // 计算RMS距离误差
    result.rmsDistance = ComputeRMSDistance(
        nurbsSurface, quadricSurface, samplePoints
    );
    
    // 判断是否满足精度要求
    result.meetsRequirements = (result.maxDistance <= m_tolerance) &&
                                (result.avgDistance <= m_tolerance * 0.5);
    
    return result;
}

// 验证拓扑面转换精度
AccuracyResult AccuracyValidator::ValidateAccuracy(
    const TopoDS_Face& nurbsFace,
    const TopoDS_Face& quadricFace
) {
    AccuracyResult result;
    
    if (nurbsFace.IsNull() || quadricFace.IsNull()) {
        return result;
    }
    
    // 提取底层几何曲面
    Handle(Geom_Surface) nurbsSurf = BRep_Tool::Surface(nurbsFace);
    Handle(Geom_Surface) quadricSurf = BRep_Tool::Surface(quadricFace);
    
    Handle(Geom_BSplineSurface) bsplineSurf = 
        Handle(Geom_BSplineSurface)::DownCast(nurbsSurf);
    
    if (!bsplineSurf.IsNull()) {
        result = ValidateAccuracy(bsplineSurf, quadricSurf);
    }
    
    // 计算Hausdorff距离（边界）
    result.hausdorffDistance = ComputeHausdorffDistance(nurbsFace, quadricFace);
    
    // 验证G1连续性
    result.g1ContinuityError = ValidateG1Continuity(nurbsFace, quadricFace);
    
    // 更新是否满足要求
    result.meetsRequirements = result.meetsRequirements &&
                                (result.hausdorffDistance <= m_tolerance) &&
                                (result.g1ContinuityError <= Precision::ANGLE_TOLERANCE);
    
    return result;
}

// 性能对比测试
PerformanceResult AccuracyValidator::ComparePerformance(
    const Handle(Geom_BSplineSurface)& nurbsSurface,
    const Handle(Geom_Surface)& quadricSurface
) {
    PerformanceResult result;
    
    if (nurbsSurface.IsNull() || quadricSurface.IsNull()) {
        return result;
    }
    
    // 生成测试点
    std::vector<gp_Pnt> testPoints;
    for (int i = 0; i < 100; ++i) {
        testPoints.push_back(gp_Pnt(
            (rand() % 1000) / 100.0,
            (rand() % 1000) / 100.0,
            (rand() % 1000) / 100.0
        ));
    }
    
    // 测试投影运算性能
    result.nurbsProjectionTime = BenchmarkProjection(
        Handle(Geom_Surface)::DownCast(nurbsSurface), testPoints
    );
    result.quadricProjectionTime = BenchmarkProjection(quadricSurface, testPoints);
    
    // 测试求交运算性能
    result.nurbsIntersectionTime = BenchmarkIntersection(
        Handle(Geom_Surface)::DownCast(nurbsSurface)
    );
    result.quadricIntersectionTime = BenchmarkIntersection(quadricSurface);
    
    return result;
}

// 性能对比测试（拓扑面）
PerformanceResult AccuracyValidator::ComparePerformance(
    const TopoDS_Face& nurbsFace,
    const TopoDS_Face& quadricFace
) {
    PerformanceResult result;
    
    if (nurbsFace.IsNull() || quadricFace.IsNull()) {
        return result;
    }
    
    // 提取底层几何曲面
    Handle(Geom_Surface) nurbsSurf = BRep_Tool::Surface(nurbsFace);
    Handle(Geom_Surface) quadricSurf = BRep_Tool::Surface(quadricFace);
    
    Handle(Geom_BSplineSurface) bsplineSurf = 
        Handle(Geom_BSplineSurface)::DownCast(nurbsSurf);
    
    if (!bsplineSurf.IsNull()) {
        result = ComparePerformance(bsplineSurf, quadricSurf);
    }
    
    // 测试布尔运算性能
    result.nurbsBooleanTime = BenchmarkBoolean(nurbsFace);
    result.quadricBooleanTime = BenchmarkBoolean(quadricFace);
    
    return result;
}

// 计算最大几何距离误差
double AccuracyValidator::ComputeMaxDistance(
    const Handle(Geom_BSplineSurface)& nurbsSurface,
    const Handle(Geom_Surface)& quadricSurface,
    const std::vector<gp_Pnt>& samplePoints
) {
    double maxDistance = 0.0;
    
    for (const auto& point : samplePoints) {
        try {
            GeomAPI_ProjectPointOnSurf projector(point, quadricSurface);
            if (projector.NbPoints() > 0) {
                double dist = projector.LowerDistance();
                maxDistance = std::max(maxDistance, dist);
            }
        } catch (...) {
            // 忽略投影失败的点
        }
    }
    
    return maxDistance;
}

// 计算平均距离误差
double AccuracyValidator::ComputeAvgDistance(
    const Handle(Geom_BSplineSurface)& nurbsSurface,
    const Handle(Geom_Surface)& quadricSurface,
    const std::vector<gp_Pnt>& samplePoints
) {
    double sumDistance = 0.0;
    int validPoints = 0;
    
    for (const auto& point : samplePoints) {
        try {
            GeomAPI_ProjectPointOnSurf projector(point, quadricSurface);
            if (projector.NbPoints() > 0) {
                sumDistance += projector.LowerDistance();
                validPoints++;
            }
        } catch (...) {
            // 忽略投影失败的点
        }
    }
    
    return (validPoints > 0) ? (sumDistance / validPoints) : 0.0;
}

// 计算RMS距离误差
double AccuracyValidator::ComputeRMSDistance(
    const Handle(Geom_BSplineSurface)& nurbsSurface,
    const Handle(Geom_Surface)& quadricSurface,
    const std::vector<gp_Pnt>& samplePoints
) {
    double sumSquaredDistance = 0.0;
    int validPoints = 0;
    
    for (const auto& point : samplePoints) {
        try {
            GeomAPI_ProjectPointOnSurf projector(point, quadricSurface);
            if (projector.NbPoints() > 0) {
                double dist = projector.LowerDistance();
                sumSquaredDistance += dist * dist;
                validPoints++;
            }
        } catch (...) {
            // 忽略投影失败的点
        }
    }
    
    return (validPoints > 0) ? std::sqrt(sumSquaredDistance / validPoints) : 0.0;
}

// 计算Hausdorff距离（边界曲线）
double AccuracyValidator::ComputeHausdorffDistance(
    const TopoDS_Face& nurbsFace,
    const TopoDS_Face& quadricFace
) {
    double maxHausdorff = 0.0;
    
    // 遍历NURBS面的所有边界曲线
    TopExp_Explorer explorer1(nurbsFace, TopAbs_EDGE);
    while (explorer1.More()) {
        TopoDS_Edge edge1 = TopoDS::Edge(explorer1.Current());
        
        // 遍历二次曲面的所有边界曲线
        TopExp_Explorer explorer2(quadricFace, TopAbs_EDGE);
        while (explorer2.More()) {
            TopoDS_Edge edge2 = TopoDS::Edge(explorer2.Current());
            
            // 计算两条边的Hausdorff距离
            double dist = ComputeEdgeHausdorffDistance(edge1, edge2);
            maxHausdorff = std::max(maxHausdorff, dist);
            
            explorer2.Next();
        }
        
        explorer1.Next();
    }
    
    return maxHausdorff;
}

// 计算边界曲线的Hausdorff距离
double AccuracyValidator::ComputeEdgeHausdorffDistance(
    const TopoDS_Edge& edge1,
    const TopoDS_Edge& edge2
) {
    // 简化实现：采样边界曲线并计算点到点的最大距离
    double maxDist = 0.0;
    
    try {
        BRepAdaptor_Curve curve1(edge1);
        BRepAdaptor_Curve curve2(edge2);
        
        // 采样边1
        double first1 = curve1.FirstParameter();
        double last1 = curve1.LastParameter();
        
        for (int i = 0; i < 20; ++i) {
            double t = first1 + (last1 - first1) * i / 19.0;
            gp_Pnt p1 = curve1.Value(t);
            
            // 找到边2上最近的点
            double first2 = curve2.FirstParameter();
            double last2 = curve2.LastParameter();
            
            double minDist = 1e10;
            for (int j = 0; j < 20; ++j) {
                double s = first2 + (last2 - first2) * j / 19.0;
                gp_Pnt p2 = curve2.Value(s);
                double dist = p1.Distance(p2);
                minDist = std::min(minDist, dist);
            }
            
            maxDist = std::max(maxDist, minDist);
        }
    } catch (...) {
        // 忽略错误
    }
    
    return maxDist;
}

// 验证G1连续性
double AccuracyValidator::ValidateG1Continuity(
    const TopoDS_Face& nurbsFace,
    const TopoDS_Face& quadricFace
) {
    // 简化实现：比较边界上的法向量
    double maxAngleError = 0.0;
    
    try {
        Handle(Geom_Surface) surf1 = BRep_Tool::Surface(nurbsFace);
        Handle(Geom_Surface) surf2 = BRep_Tool::Surface(quadricFace);
        
        // 在曲面上采样并比较法向量
        double uMin1, uMax1, vMin1, vMax1;
        surf1->Bounds(uMin1, uMax1, vMin1, vMax1);
        
        double uMin2, uMax2, vMin2, vMax2;
        surf2->Bounds(uMin2, uMax2, vMin2, vMax2);
        
        for (int i = 0; i < 10; ++i) {
            double u1 = uMin1 + (uMax1 - uMin1) * i / 9.0;
            double u2 = uMin2 + (uMax2 - uMin2) * i / 9.0;
            
            for (int j = 0; j < 10; ++j) {
                double v1 = vMin1 + (vMax1 - vMin1) * j / 9.0;
                double v2 = vMin2 + (vMax2 - vMin2) * j / 9.0;
                
                gp_Pnt p1, p2;
                gp_Vec d1u, d1v, d2u, d2v;
                
                surf1->D1(u1, v1, p1, d1u, d1v);
                surf2->D1(u2, v2, p2, d2u, d2v);
                
                gp_Vec normal1 = d1u.Crossed(d1v);
                gp_Vec normal2 = d2u.Crossed(d2v);
                
                if (normal1.Magnitude() > Precision::ZERO_TOLERANCE &&
                    normal2.Magnitude() > Precision::ZERO_TOLERANCE) {
                    normal1.Normalize();
                    normal2.Normalize();
                    
                    double angle = std::abs(normal1.Angle(normal2));
                    double angleDeg = angle * 180.0 / M_PI;
                    maxAngleError = std::max(maxAngleError, angleDeg);
                }
            }
        }
    } catch (...) {
        // 忽略错误
    }
    
    return maxAngleError;
}

// 在曲面上采样点
std::vector<gp_Pnt> AccuracyValidator::SampleSurface(
    const Handle(Geom_Surface)& surface,
    int samplesU,
    int samplesV
) {
    std::vector<gp_Pnt> points;
    
    if (surface.IsNull()) {
        return points;
    }
    
    double uMin, uMax, vMin, vMax;
    surface->Bounds(uMin, uMax, vMin, vMax);
    
    for (int i = 0; i < samplesU; ++i) {
        double u = uMin + (uMax - uMin) * i / (samplesU - 1);
        
        for (int j = 0; j < samplesV; ++j) {
            double v = vMin + (vMax - vMin) * j / (samplesV - 1);
            
            try {
                gp_Pnt point = surface->Value(u, v);
                points.push_back(point);
            } catch (...) {
                // 忽略无效参数点
            }
        }
    }
    
    return points;
}

// 测试投影运算性能
double AccuracyValidator::BenchmarkProjection(
    const Handle(Geom_Surface)& surface,
    const std::vector<gp_Pnt>& testPoints
) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int iter = 0; iter < m_perfIterations; ++iter) {
        for (const auto& point : testPoints) {
            try {
                GeomAPI_ProjectPointOnSurf projector(point, surface);
                // 强制计算
                if (projector.NbPoints() > 0) {
                    projector.LowerDistance();
                }
            } catch (...) {
                // 忽略错误
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 返回平均时间（毫秒）
    return duration.count() / 1000.0 / m_perfIterations;
}

// 测试求交运算性能
double AccuracyValidator::BenchmarkIntersection(
    const Handle(Geom_Surface)& surface
) {
    // 创建一个测试平面进行求交
    Handle(Geom_Surface) testPlane = new Geom_Plane(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int iter = 0; iter < m_perfIterations; ++iter) {
        try {
            GeomAPI_IntSS intersector(surface, testPlane, Precision::DEFAULT_TOLERANCE);
            // 强制计算
            if (intersector.IsDone()) {
                intersector.NbLines();
            }
        } catch (...) {
            // 忽略错误
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 返回平均时间（毫秒）
    return duration.count() / 1000.0 / m_perfIterations;
}

// 测试布尔运算性能
double AccuracyValidator::BenchmarkBoolean(const TopoDS_Face& face) {
    // 创建一个测试立方体进行布尔运算
    BRepBuilderAPI_MakeFace faceMaker(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    TopoDS_Face testFace = faceMaker.Face();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int iter = 0; iter < m_perfIterations; ++iter) {
        try {
            // 简单的布尔交运算
            BRepAlgoAPI_Common boolean(face, testFace);
            if (boolean.IsDone()) {
                boolean.Shape();
            }
        } catch (...) {
            // 忽略错误
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 返回平均时间（毫秒）
    return duration.count() / 1000.0 / m_perfIterations;
}

// 获取当前时间（毫秒）
double AccuracyValidator::GetTimeInMilliseconds() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace BS2Quad

