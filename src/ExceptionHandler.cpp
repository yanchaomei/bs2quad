/**
 * @file ExceptionHandler.cpp
 * @brief 异常场景检测和处理模块实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "ExceptionHandler.h"
#include "AnalyticRecovery.h"
#include "ImplicitCertificate.h"
#include <BRep_Tool.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <cmath>

namespace BS2Quad {

// 构造函数
ExceptionHandler::ExceptionHandler(double tolerance)
    : m_tolerance(tolerance)
    , m_minArea(1.0e-6)
    , m_maxSplitDepth(3)
{
    SetTolerance(tolerance);
}

// 析构函数
ExceptionHandler::~ExceptionHandler() {
}

// 检测NURBS曲面的异常情况
ExceptionInfo ExceptionHandler::DetectExceptions(
    const Handle(Geom_BSplineSurface)& surface
) {
    ExceptionInfo info;
    
    if (surface.IsNull()) {
        info.errorType = ErrorType::DATA_CORRUPT;
        info.message = "曲面为空";
        info.canSimplify = false;
        return info;
    }
    
    // 检查数据是否损坏
    if (IsDataCorrupt(surface)) {
        info.errorType = ErrorType::DATA_CORRUPT;
        info.message = "曲面数据损坏：控制点、权重或节点向量无效";
        info.canSimplify = false;
        return info;
    }
    
    // 检查参数是否越界
    if (IsOutOfRange(surface)) {
        info.errorType = ErrorType::OUT_OF_RANGE;
        info.message = "参数域越界";
        info.canSimplify = false;
        return info;
    }
    
    // 检查曲面是否退化
    if (IsDegenerate(surface)) {
        info.errorType = ErrorType::DEGENERATE_SURFACE;
        info.message = "曲面退化：面积接近零";
        info.canSimplify = false;
        return info;
    }
    
    // 检查是否为非二次曲面
    if (IsNonQuadric(surface)) {
        info.errorType = ErrorType::NON_QUADRIC;
        info.message = "复杂自由曲面，无法直接转换为二次曲面";
        info.canSimplify = true;  // 可以尝试简化
        return info;
    }
    
    // 没有检测到异常
    info.errorType = ErrorType::SUCCESS;
    info.message = "曲面正常";
    info.canSimplify = false;
    
    return info;
}

// 检测拓扑面的异常情况
ExceptionInfo ExceptionHandler::DetectExceptions(const TopoDS_Face& face) {
    ExceptionInfo info;
    
    if (face.IsNull()) {
        info.errorType = ErrorType::DATA_CORRUPT;
        info.message = "拓扑面为空";
        info.canSimplify = false;
        return info;
    }
    
    // 提取底层几何曲面
    Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
    
    // 尝试转换为B样条曲面
    Handle(Geom_BSplineSurface) bsplineSurface = 
        Handle(Geom_BSplineSurface)::DownCast(surface);
    
    if (bsplineSurface.IsNull()) {
        info.errorType = ErrorType::DATA_CORRUPT;
        info.message = "无法转换为B样条曲面";
        info.canSimplify = false;
        return info;
    }
    
    return DetectExceptions(bsplineSurface);
}

// 尝试简化曲面
SimplificationResult ExceptionHandler::TrySimplify(
    const Handle(Geom_BSplineSurface)& surface
) {
    SimplificationResult result;
    
    if (surface.IsNull()) {
        result.success = false;
        result.description = "输入曲面为空";
        return result;
    }
    
    const auto candidate=RecoverAnalytic(surface,m_tolerance);
    if(!candidate.surface.IsNull()) {
        const auto bound=CertifyImplicitDistance(surface,candidate.surface);
        if(bound.certified&&bound.upperBound<=m_tolerance) {
            result.quadrics.push_back(candidate.surface);result.success=true;
            result.description="whole surface converted with independent geometry bound; no subdivision performed";
            return result;
        }
    }
    result.freeForms.push_back(surface);
    result.description="no certified analytic conversion; original freeform retained; subdivision is not implemented";
    return result;
}

// 生成错误报告
std::string ExceptionHandler::GenerateErrorReport(
    const ExceptionInfo& exceptionInfo
) const {
    std::string report = "=== BS2Quad 错误报告 ===\n\n";
    
    report += "错误类型: " + ErrorTypeToString(exceptionInfo.errorType) + "\n";
    report += "详细信息: " + exceptionInfo.message + "\n";
    report += "可否简化: " + std::string(exceptionInfo.canSimplify ? "是" : "否") + "\n";
    
    if (exceptionInfo.canSimplify) {
        report += "\n建议: 尝试使用 TrySimplify() 方法对曲面进行分割和局部转换\n";
    }
    
    report += "\n=======================\n";
    
    return report;
}

// 检查数据是否损坏
bool ExceptionHandler::IsDataCorrupt(
    const Handle(Geom_BSplineSurface)& surface
) const {
    // 检查控制点
    if (!ValidateControlPoints(surface)) {
        return true;
    }
    
    // 检查权重
    if (!ValidateWeights(surface)) {
        return true;
    }
    
    // 检查节点向量
    if (!ValidateKnotVector(surface)) {
        return true;
    }
    
    return false;
}

// 检查参数是否越界
bool ExceptionHandler::IsOutOfRange(
    const Handle(Geom_BSplineSurface)& surface
) const {
    double uMin, uMax, vMin, vMax;
    surface->Bounds(uMin, uMax, vMin, vMax);
    
    // 检查参数域是否有效
    if (uMax <= uMin || vMax <= vMin) {
        return true;
    }
    
    // 检查参数域是否异常大
    if ((uMax - uMin) > 1.0e6 || (vMax - vMin) > 1.0e6) {
        return true;
    }
    
    return false;
}

// 检查曲面是否退化
bool ExceptionHandler::IsDegenerate(
    const Handle(Geom_BSplineSurface)& surface
) const {
    double area = ComputeSurfaceArea(surface);
    return area < m_minArea;
}

// 检查是否为非二次曲面
bool ExceptionHandler::IsNonQuadric(
    const Handle(Geom_BSplineSurface)& surface
) const {
    const auto candidate=RecoverAnalytic(surface,m_tolerance);
    if(candidate.surface.IsNull()) return true;
    const auto bound=CertifyImplicitDistance(surface,candidate.surface);
    return !bound.certified||bound.upperBound>m_tolerance;
}

// 检查控制点是否有效
bool ExceptionHandler::ValidateControlPoints(
    const Handle(Geom_BSplineSurface)& surface
) const {
    int numPolesU = surface->NbUPoles();
    int numPolesV = surface->NbVPoles();
    
    if (numPolesU < 2 || numPolesV < 2) {
        return false;
    }
    
    // 检查控制点坐标是否有效（不是NaN或Inf）
    for (int i = 1; i <= numPolesU; ++i) {
        for (int j = 1; j <= numPolesV; ++j) {
            gp_Pnt pole = surface->Pole(i, j);
            
            if (std::isnan(pole.X()) || std::isinf(pole.X()) ||
                std::isnan(pole.Y()) || std::isinf(pole.Y()) ||
                std::isnan(pole.Z()) || std::isinf(pole.Z())) {
                return false;
            }
        }
    }
    
    return true;
}

// 检查权重是否有效
bool ExceptionHandler::ValidateWeights(
    const Handle(Geom_BSplineSurface)& surface
) const {
    if (!surface->IsURational() && !surface->IsVRational()) {
        return true;  // 非有理曲面，无需检查权重
    }
    
    int numPolesU = surface->NbUPoles();
    int numPolesV = surface->NbVPoles();
    
    for (int i = 1; i <= numPolesU; ++i) {
        for (int j = 1; j <= numPolesV; ++j) {
            double weight = surface->Weight(i, j);
            
            if (std::isnan(weight) || std::isinf(weight) || weight <= 0.0) {
                return false;
            }
        }
    }
    
    return true;
}

// 检查节点向量是否有效
bool ExceptionHandler::ValidateKnotVector(
    const Handle(Geom_BSplineSurface)& surface
) const {
    // 检查U方向节点向量
    int numKnotsU = surface->NbUKnots();
    if (numKnotsU < 2) {
        return false;
    }
    
    for (int i = 1; i < numKnotsU; ++i) {
        double knot1 = surface->UKnot(i);
        double knot2 = surface->UKnot(i + 1);
        
        if (std::isnan(knot1) || std::isinf(knot1)) {
            return false;
        }
        
        if (knot2 < knot1) {
            return false;  // 节点向量必须非递减
        }
    }
    
    // 检查V方向节点向量
    int numKnotsV = surface->NbVKnots();
    if (numKnotsV < 2) {
        return false;
    }
    
    for (int i = 1; i < numKnotsV; ++i) {
        double knot1 = surface->VKnot(i);
        double knot2 = surface->VKnot(i + 1);
        
        if (std::isnan(knot1) || std::isinf(knot1)) {
            return false;
        }
        
        if (knot2 < knot1) {
            return false;
        }
    }
    
    return true;
}

// 计算曲面面积
double ExceptionHandler::ComputeSurfaceArea(
    const Handle(Geom_BSplineSurface)& surface
) const {
    // 简化实现：采样并估算面积
    double area = 0.0;
    
    double uMin, uMax, vMin, vMax;
    surface->Bounds(uMin, uMax, vMin, vMax);
    
    int samples = 20;
    double du = (uMax - uMin) / samples;
    double dv = (vMax - vMin) / samples;
    
    for (int i = 0; i < samples; ++i) {
        for (int j = 0; j < samples; ++j) {
            double u1 = uMin + i * du;
            double u2 = u1 + du;
            double v1 = vMin + j * dv;
            double v2 = v1 + dv;
            
            try {
                gp_Pnt p1 = surface->Value(u1, v1);
                gp_Pnt p2 = surface->Value(u2, v1);
                gp_Pnt p3 = surface->Value(u2, v2);
                gp_Pnt p4 = surface->Value(u1, v2);
                
                // 计算四边形面积（两个三角形）
                gp_Vec v12(p1, p2);
                gp_Vec v14(p1, p4);
                gp_Vec v23(p2, p3);
                
                double area1 = 0.5 * v12.Crossed(v14).Magnitude();
                double area2 = 0.5 * v14.Crossed(v23).Magnitude();
                
                area += area1 + area2;
            } catch (...) {
                // 忽略无效点
            }
        }
    }
    
    return area;
}

// 尝试分割曲面
bool ExceptionHandler::TrySplitSurface(
    const Handle(Geom_BSplineSurface)& surface,
    std::vector<Handle(Geom_BSplineSurface)>& patches
) {
    patches.clear();
    
    // 简化实现：不进行实际分割，只返回原曲面
    patches.push_back(surface);
    
    return true;
}

// 识别可转换区域
bool ExceptionHandler::IdentifyConvertibleRegions(
    const Handle(Geom_BSplineSurface)& surface,
    std::vector<Handle(Geom_BSplineSurface)>& convertibleRegions,
    std::vector<Handle(Geom_BSplineSurface)>& freeFormRegions
) {
    convertibleRegions.clear();
    freeFormRegions.clear();
    
    // 简化实现：根据曲面特征分类
    if (IsNonQuadric(surface)) {
        freeFormRegions.push_back(surface);
    } else {
        convertibleRegions.push_back(surface);
    }
    
    return true;
}

} // namespace BS2Quad

