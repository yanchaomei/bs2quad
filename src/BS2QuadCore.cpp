/**
 * @file BS2QuadCore.cpp
 * @brief BS2Quad核心转换引擎实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "BS2QuadCore.h"
#include <BRep_Tool.hxx>
#include <chrono>
#include <sstream>

namespace BS2Quad {

// 构造函数
BS2QuadCore::BS2QuadCore(double tolerance)
    : m_tolerance(tolerance)
    , m_enablePerfTest(false)
    , m_enableOptimization(true)
    , m_totalConversions(0)
    , m_successfulConversions(0)
    , m_totalConversionTime(0.0)
{
    // 初始化所有模块
    m_analyzer = std::make_unique<SurfaceAnalyzer>(tolerance);
    m_cylinderFitter = std::make_unique<CylinderFitter>(tolerance);
    m_coneFitter = std::make_unique<ConeFitter>(tolerance);
    m_sphereFitter = std::make_unique<SphereFitter>(tolerance);
    m_planeFitter = std::make_unique<PlaneFitter>(tolerance);
    m_torusFitter = std::make_unique<TorusFitter>(tolerance);
    m_validator = std::make_unique<AccuracyValidator>(tolerance);
    m_exceptionHandler = std::make_unique<ExceptionHandler>(tolerance);
    
    // 配置拟合器
    m_cylinderFitter->SetOptimize(m_enableOptimization);
    m_coneFitter->SetOptimize(m_enableOptimization);
    m_sphereFitter->SetOptimize(m_enableOptimization);
    m_torusFitter->SetOptimize(m_enableOptimization);
}

// 析构函数
BS2QuadCore::~BS2QuadCore() {
}

// 转换NURBS曲面到二次曲面
ConversionResult BS2QuadCore::Convert(
    const Handle(Geom_BSplineSurface)& nurbsSurface
) {
    ConversionResult result;
    
    double startTime = GetTimeInMilliseconds();
    
    m_totalConversions++;
    
    if (nurbsSurface.IsNull()) {
        result.message = "输入曲面为空";
        result.conversionTime = GetTimeInMilliseconds() - startTime;
        return result;
    }
    
    // 步骤1：分析曲面
    SurfaceAnalysisResult analysisResult = AnalyzeSurface(nurbsSurface);
    
    if (!analysisResult.IsSuccess()) {
        // 尝试异常处理
        if (!HandleException(nurbsSurface, result)) {
            result.message = analysisResult.message;
            result.conversionTime = GetTimeInMilliseconds() - startTime;
            return result;
        }
        // 异常处理成功，返回结果
        result.conversionTime = GetTimeInMilliseconds() - startTime;
        return result;
    }
    
    // 步骤2：拟合二次曲面
    Handle(Geom_Surface) quadricSurface = FitQuadricSurface(
        nurbsSurface,
        analysisResult.type
    );
    
    if (quadricSurface.IsNull()) {
        result.message = "拟合二次曲面失败";
        result.conversionTime = GetTimeInMilliseconds() - startTime;
        return result;
    }
    
    // 步骤3：验证转换精度
    AccuracyResult accuracyResult = ValidateConversion(
        nurbsSurface,
        quadricSurface
    );
    
    // 步骤4：性能测试（如果启用）
    PerformanceResult performanceResult;
    if (m_enablePerfTest) {
        performanceResult = m_validator->ComparePerformance(
            nurbsSurface,
            quadricSurface
        );
    }
    
    // 填充结果
    result.success = accuracyResult.meetsRequirements;
    result.quadricType = analysisResult.type;
    result.quadricSurface = quadricSurface;
    result.accuracyResult = accuracyResult;
    result.performanceResult = performanceResult;
    result.conversionTime = GetTimeInMilliseconds() - startTime;
    
    if (result.success) {
        result.message = "转换成功：" + QuadricTypeToString(analysisResult.type) +
                         "，置信度=" + std::to_string(analysisResult.confidence) +
                         "，最大误差=" + std::to_string(accuracyResult.maxDistance) + "mm";
        m_successfulConversions++;
    } else {
        result.message = "转换精度不满足要求：最大误差=" + 
                         std::to_string(accuracyResult.maxDistance) + "mm";
    }
    
    m_totalConversionTime += result.conversionTime;
    
    return result;
}

// 转换拓扑面到二次曲面
ConversionResult BS2QuadCore::Convert(const TopoDS_Face& nurbsFace) {
    ConversionResult result;
    
    if (nurbsFace.IsNull()) {
        result.message = "输入拓扑面为空";
        return result;
    }
    
    // 提取底层几何曲面
    Handle(Geom_Surface) surface = BRep_Tool::Surface(nurbsFace);
    
    // 尝试转换为B样条曲面
    Handle(Geom_BSplineSurface) bsplineSurface = 
        Handle(Geom_BSplineSurface)::DownCast(surface);
    
    if (bsplineSurface.IsNull()) {
        result.message = "无法转换为B样条曲面";
        return result;
    }
    
    return Convert(bsplineSurface);
}

// 批量转换
std::vector<ConversionResult> BS2QuadCore::ConvertBatch(
    const std::vector<Handle(Geom_BSplineSurface)>& nurbsSurfaces
) {
    std::vector<ConversionResult> results;
    results.reserve(nurbsSurfaces.size());
    
    for (const auto& surface : nurbsSurfaces) {
        results.push_back(Convert(surface));
    }
    
    return results;
}

// 设置容差
void BS2QuadCore::SetTolerance(double tolerance) {
    m_tolerance = tolerance;
    
    // 更新所有模块的容差
    m_analyzer->SetTolerance(tolerance);
    m_cylinderFitter->SetTolerance(tolerance);
    m_coneFitter->SetTolerance(tolerance);
    m_sphereFitter->SetTolerance(tolerance);
    m_planeFitter->SetTolerance(tolerance);
    m_torusFitter->SetTolerance(tolerance);
    m_validator->SetTolerance(tolerance);
    m_exceptionHandler->SetTolerance(tolerance);
}

// 获取统计信息
std::string BS2QuadCore::GetStatistics() const {
    std::ostringstream oss;
    
    oss << "=== BS2Quad 统计信息 ===\n";
    oss << "版本: " << GetVersion() << "\n";
    oss << "总转换次数: " << m_totalConversions << "\n";
    oss << "成功转换次数: " << m_successfulConversions << "\n";
    oss << "成功率: ";
    
    if (m_totalConversions > 0) {
        double successRate = 100.0 * m_successfulConversions / m_totalConversions;
        oss << successRate << "%\n";
    } else {
        oss << "N/A\n";
    }
    
    oss << "总转换时间: " << m_totalConversionTime << " ms\n";
    
    if (m_totalConversions > 0) {
        double avgTime = m_totalConversionTime / m_totalConversions;
        oss << "平均转换时间: " << avgTime << " ms\n";
    }
    
    oss << "=======================\n";
    
    return oss.str();
}

// 执行曲面分析
SurfaceAnalysisResult BS2QuadCore::AnalyzeSurface(
    const Handle(Geom_BSplineSurface)& surface
) {
    return m_analyzer->Analyze(surface);
}

// 执行曲面拟合
Handle(Geom_Surface) BS2QuadCore::FitQuadricSurface(
    const Handle(Geom_BSplineSurface)& surface,
    QuadricType surfaceType
) {
    Handle(Geom_Surface) quadricSurface;
    
    switch (surfaceType) {
        case QuadricType::PLANE:
            m_planeFitter->Fit(surface, quadricSurface);
            break;
            
        case QuadricType::CYLINDER:
            m_cylinderFitter->Fit(surface, quadricSurface);
            break;
            
        case QuadricType::CONE:
            m_coneFitter->Fit(surface, quadricSurface);
            break;
            
        case QuadricType::SPHERE:
            m_sphereFitter->Fit(surface, quadricSurface);
            break;
            
        case QuadricType::TORUS:
            m_torusFitter->Fit(surface, quadricSurface);
            break;
            
        default:
            break;
    }
    
    return quadricSurface;
}

// 验证转换精度
AccuracyResult BS2QuadCore::ValidateConversion(
    const Handle(Geom_BSplineSurface)& nurbsSurface,
    const Handle(Geom_Surface)& quadricSurface
) {
    return m_validator->ValidateAccuracy(nurbsSurface, quadricSurface);
}

// 处理异常情况
bool BS2QuadCore::HandleException(
    const Handle(Geom_BSplineSurface)& surface,
    ConversionResult& result
) {
    // 检测异常
    ExceptionInfo exceptionInfo = m_exceptionHandler->DetectExceptions(surface);
    
    // 生成错误报告
    result.message = m_exceptionHandler->GenerateErrorReport(exceptionInfo);
    
    // 如果可以简化，尝试简化
    if (exceptionInfo.canSimplify) {
        SimplificationResult simplification = m_exceptionHandler->TrySimplify(surface);
        
        if (simplification.success) {
            result.message += "\n简化成功：" + simplification.description;
            return true;
        }
    }
    
    return false;
}

// 获取当前时间（毫秒）
double BS2QuadCore::GetTimeInMilliseconds() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace BS2Quad

