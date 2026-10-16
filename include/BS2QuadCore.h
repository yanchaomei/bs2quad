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
    ErrorType error = ErrorType::UNKNOWN_ERROR;
    double decisionTime = 0.0; ///< 候选识别和几何验收，不含拓扑重建或可选 CAD 基准
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

    /** Convert a face with its required tangent neighbors. Input topology must
     * share actual edges. Each preserved edge is checked on the reconstructed
     * support and the unchanged neighbor before the result is published. Sharp
     * design edges should not be supplied as required G1 neighbors. */
    ConversionResult Convert(const TopoDS_Face& nurbsFace,
        const std::vector<TopoDS_Face>& tangentNeighbors);
    
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
     * @brief 旧版兼容入口；当前恢复与证书优化固定启用。
     * 有限点拟合的可选优化由各 fitter 的 SetOptimize 控制。
     */
    [[deprecated("canonical recovery uses a fixed validated path; use fitter.SetOptimize for finite-point fitting")]]
    void SetEnableOptimization(bool) {}
    
    /**
     * @brief 获取版本信息
     * @return 版本字符串
     */
    static std::string GetVersion() { return "1.1.0"; }
    
    /**
     * @brief 获取统计信息
     * @return 统计信息字符串
     */
    std::string GetStatistics() const;

private:
    ConversionResult ConvertImpl(const Handle(Geom_BSplineSurface)& source,
        const TopoDS_Face* face);

private:
    double m_tolerance;                            ///< 几何误差容差
    bool m_enablePerfTest;                         ///< 是否启用性能测试
    
    std::unique_ptr<AccuracyValidator> m_validator;  ///< 精度验证器
    
    // 统计信息
    int m_totalConversions;                        ///< 总转换次数
    int m_successfulConversions;                   ///< 成功转换次数
    double m_totalConversionTime;                  ///< 总转换时间
};

} // namespace BS2Quad

#endif // BS2QUAD_CORE_H

