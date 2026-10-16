/**
 * @file ExceptionHandler.h
 * @brief 异常场景检测和处理模块
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_EXCEPTION_HANDLER_H
#define BS2QUAD_EXCEPTION_HANDLER_H

#include "Common.h"
#include <Geom_BSplineSurface.hxx>
#include <TopoDS_Face.hxx>
#include <string>
#include <vector>

namespace BS2Quad {

/**
 * @brief 异常信息结构
 */
struct ExceptionInfo {
    ErrorType errorType;      ///< 错误类型
    std::string message;      ///< 详细错误信息
    bool canSimplify;         ///< 是否可以简化
    
    /**
     * @brief 构造函数
     */
    ExceptionInfo()
        : errorType(ErrorType::SUCCESS),
          message(""),
          canSimplify(false) {}
    
    /**
     * @brief 判断是否成功
     */
    bool IsSuccess() const { return errorType == ErrorType::SUCCESS; }
};

/**
 * @brief 简化方案结构
 */
struct SimplificationResult {
    bool success;                              ///< 简化是否成功
    std::vector<Handle(Geom_Surface)> quadrics;  ///< 转换的二次曲面部分
    std::vector<Handle(Geom_Surface)> freeForms; ///< 保留的自由曲面部分
    std::string description;                   ///< 简化方案描述
    
    /**
     * @brief 构造函数
     */
    SimplificationResult()
        : success(false), description("") {}
};

/**
 * @brief 异常处理器
 * 
 * 负责检测和处理各种异常场景，提供局部简化方案
 */
class ExceptionHandler {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit ExceptionHandler(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 析构函数
     */
    ~ExceptionHandler();
    
    /**
     * @brief 检测NURBS曲面的异常情况
     * @param surface NURBS曲面
     * @return 异常信息
     */
    ExceptionInfo DetectExceptions(const Handle(Geom_BSplineSurface)& surface);
    
    /**
     * @brief 检测拓扑面的异常情况
     * @param face 拓扑面
     * @return 异常信息
     */
    ExceptionInfo DetectExceptions(const TopoDS_Face& face);
    
    /**
     * @brief 尝试简化曲面
     * @param surface NURBS曲面
     * @return 简化方案
     */
    SimplificationResult TrySimplify(const Handle(Geom_BSplineSurface)& surface);
    
    /**
     * @brief 生成错误报告
     * @param exceptionInfo 异常信息
     * @return 错误报告字符串
     */
    std::string GenerateErrorReport(const ExceptionInfo& exceptionInfo) const;
    
    /**
     * @brief 设置容差
     * @param tolerance 新的容差值
     */
    void SetTolerance(double tolerance) {
        if(!std::isfinite(tolerance)||tolerance<=0) throw std::invalid_argument("tolerance must be finite and positive");
        m_tolerance=tolerance;
    }
    
    /**
     * @brief 获取容差
     * @return 当前容差值
     */
    double GetTolerance() const { return m_tolerance; }

private:
    /**
     * @brief 检查数据是否损坏
     * @param surface NURBS曲面
     * @return 是否损坏
     */
    bool IsDataCorrupt(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 检查参数是否越界
     * @param surface NURBS曲面
     * @return 是否越界
     */
    bool IsOutOfRange(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 检查曲面是否退化
     * @param surface NURBS曲面
     * @return 是否退化
     */
    bool IsDegenerate(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 检查是否为非二次曲面
     * @param surface NURBS曲面
     * @return 是否非二次曲面
     */
    bool IsNonQuadric(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 检查控制点是否有效
     * @param surface NURBS曲面
     * @return 是否有效
     */
    bool ValidateControlPoints(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 检查权重是否有效
     * @param surface NURBS曲面
     * @return 是否有效
     */
    bool ValidateWeights(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 检查节点向量是否有效
     * @param surface NURBS曲面
     * @return 是否有效
     */
    bool ValidateKnotVector(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 计算曲面面积
     * @param surface NURBS曲面
     * @return 曲面面积
     */
    double ComputeSurfaceArea(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 尝试分割曲面
     * @param surface NURBS曲面
     * @param patches 输出：分割后的曲面片
     * @return 是否成功分割
     */
    bool TrySplitSurface(
        const Handle(Geom_BSplineSurface)& surface,
        std::vector<Handle(Geom_BSplineSurface)>& patches
    );
    
    /**
     * @brief 识别可转换区域
     * @param surface NURBS曲面
     * @param convertibleRegions 输出：可转换区域
     * @param freeFormRegions 输出：自由曲面区域
     * @return 是否成功识别
     */
    bool IdentifyConvertibleRegions(
        const Handle(Geom_BSplineSurface)& surface,
        std::vector<Handle(Geom_BSplineSurface)>& convertibleRegions,
        std::vector<Handle(Geom_BSplineSurface)>& freeFormRegions
    );

private:
    double m_tolerance;        ///< 几何误差容差
    double m_minArea;          ///< 最小有效面积
    int m_maxSplitDepth;       ///< 最大分割深度
};

} // namespace BS2Quad

#endif // BS2QUAD_EXCEPTION_HANDLER_H

