/**
 * @file BS2QuadCore.h
 * @brief BS2Quad核心转换引擎
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_CORE_H
#define BS2QUAD_CORE_H

#include "Common.h"
#include "SurfaceAnalyzer.h"
#include "QuadricFitter.h"
#include "CylinderFitter.h"
#include "ConeFitter.h"
#include "SphereFitter.h"
#include "PlaneFitter.h"
#include "TorusFitter.h"
#include "AccuracyValidator.h"
#include "ExceptionHandler.h"

#include <Geom_BSplineSurface.hxx>
#include <Geom_Surface.hxx>
#include <TopoDS_Face.hxx>
#include <memory>

namespace BS2Quad {

/**
 * @brief 转换结果
 */
struct ConversionResult {
    bool success;                          ///< 转换是否成功
    QuadricType quadricType;               ///< 转换的二次曲面类型
    Handle(Geom_Surface) quadricSurface;   ///< 转换后的二次曲面
    TopoDS_Face quadricFace;               ///< 转换后的拓扑面（如果适用）
    AccuracyResult accuracyResult;         ///< 精度验证结果
    PerformanceResult performanceResult;   ///< 性能对比结果
    double conversionTime;                 ///< 转换耗时（毫秒）
    std::string message;                   ///< 详细信息
    
    /**
     * @brief 构造函数
     */
    ConversionResult()
        : success(false),
          quadricType(QuadricType::NONE),
          conversionTime(0.0),
          message("") {}
};

/**
 * @brief BS2Quad核心转换引擎
 * 
 * 整合所有模块，提供完整的样条面到二次曲面转换功能
 */
class BS2QuadCore {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit BS2QuadCore(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 析构函数
     */
    ~BS2QuadCore();
    
    /**
     * @brief 转换NURBS曲面到二次曲面
     * @param nurbsSurface NURBS B样条曲面
     * @return 转换结果
     */
    ConversionResult Convert(const Handle(Geom_BSplineSurface)& nurbsSurface);
    
    /**
     * @brief 转换拓扑面到二次曲面
     * @param nurbsFace NURBS拓扑面
     * @return 转换结果
     */
    ConversionResult Convert(const TopoDS_Face& nurbsFace);
    
    /**
     * @brief 批量转换
     * @param nurbsSurfaces NURBS曲面列表
     * @return 转换结果列表
     */
    std::vector<ConversionResult> ConvertBatch(
        const std::vector<Handle(Geom_BSplineSurface)>& nurbsSurfaces
    );
    
    /**
     * @brief 设置容差
     * @param tolerance 新的容差值
     */
    void SetTolerance(double tolerance);
    
    /**
     * @brief 获取容差
     * @return 当前容差值
     */
    double GetTolerance() const { return m_tolerance; }
    
    /**
     * @brief 设置是否启用性能对比
     * @param enable 是否启用
     */
    void SetEnablePerformanceTest(bool enable) { m_enablePerfTest = enable; }
    
    /**
     * @brief 设置是否启用优化
     * @param enable 是否启用
     */
    void SetEnableOptimization(bool enable) { m_enableOptimization = enable; }
    
    /**
     * @brief 获取版本信息
     * @return 版本字符串
     */
    static std::string GetVersion() { return "1.0.0"; }
    
    /**
     * @brief 获取统计信息
     * @return 统计信息字符串
     */
    std::string GetStatistics() const;

private:
    /**
     * @brief 执行曲面分析
     * @param surface NURBS曲面
     * @return 分析结果
     */
    SurfaceAnalysisResult AnalyzeSurface(const Handle(Geom_BSplineSurface)& surface);
    
    /**
     * @brief 执行曲面拟合
     * @param surface NURBS曲面
     * @param surfaceType 曲面类型
     * @return 拟合的二次曲面
     */
    Handle(Geom_Surface) FitQuadricSurface(
        const Handle(Geom_BSplineSurface)& surface,
        QuadricType surfaceType
    );
    
    /**
     * @brief 验证转换精度
     * @param nurbsSurface 原始NURBS曲面
     * @param quadricSurface 转换后的二次曲面
     * @return 精度验证结果
     */
    AccuracyResult ValidateConversion(
        const Handle(Geom_BSplineSurface)& nurbsSurface,
        const Handle(Geom_Surface)& quadricSurface
    );
    
    /**
     * @brief 处理异常情况
     * @param surface NURBS曲面
     * @param result 输出：转换结果
     * @return 是否成功处理
     */
    bool HandleException(
        const Handle(Geom_BSplineSurface)& surface,
        ConversionResult& result
    );
    
    /**
     * @brief 获取当前时间（毫秒）
     * @return 时间戳
     */
    double GetTimeInMilliseconds() const;

private:
    double m_tolerance;                            ///< 几何误差容差
    bool m_enablePerfTest;                         ///< 是否启用性能测试
    bool m_enableOptimization;                     ///< 是否启用优化
    
    std::unique_ptr<SurfaceAnalyzer> m_analyzer;   ///< 曲面分析器
    std::unique_ptr<CylinderFitter> m_cylinderFitter;  ///< 圆柱面拟合器
    std::unique_ptr<ConeFitter> m_coneFitter;      ///< 圆锥面拟合器
    std::unique_ptr<SphereFitter> m_sphereFitter;  ///< 球面拟合器
    std::unique_ptr<PlaneFitter> m_planeFitter;    ///< 平面拟合器
    std::unique_ptr<TorusFitter> m_torusFitter;    ///< 圆环面拟合器
    std::unique_ptr<AccuracyValidator> m_validator;  ///< 精度验证器
    std::unique_ptr<ExceptionHandler> m_exceptionHandler;  ///< 异常处理器
    
    // 统计信息
    int m_totalConversions;                        ///< 总转换次数
    int m_successfulConversions;                   ///< 成功转换次数
    double m_totalConversionTime;                  ///< 总转换时间
};

} // namespace BS2Quad

#endif // BS2QUAD_CORE_H

