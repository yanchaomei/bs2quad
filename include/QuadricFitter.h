/**
 * @file QuadricFitter.h
 * @brief 二次曲面参数拟合基类
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_QUADRIC_FITTER_H
#define BS2QUAD_QUADRIC_FITTER_H

#include "Common.h"
#include <Geom_BSplineSurface.hxx>
#include <Geom_Surface.hxx>
#include <TopoDS_Face.hxx>
#include <vector>

namespace BS2Quad {

/**
 * @brief 二次曲面拟合器基类
 * 
 * 提供从NURBS曲面拟合二次曲面参数的通用接口
 */
class QuadricFitter {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit QuadricFitter(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 虚析构函数
     */
    virtual ~QuadricFitter();
    
    /**
     * @brief 从NURBS曲面拟合二次曲面
     * @param surface NURBS B样条曲面
     * @param quadricSurface 输出：拟合得到的二次曲面
     * @return 是否成功拟合
     */
    virtual bool Fit(
        const Handle(Geom_BSplineSurface)& surface,
        Handle(Geom_Surface)& quadricSurface
    ) = 0;
    
    /**
     * @brief 从采样点拟合二次曲面
     * @param points 采样点数组
     * @param quadricSurface 输出：拟合得到的二次曲面
     * @return 是否成功拟合
     */
    virtual bool FitFromPoints(
        const std::vector<gp_Pnt>& points,
        Handle(Geom_Surface)& quadricSurface
    ) = 0;
    
    /**
     * @brief 获取拟合误差
     * @return 拟合的RMS误差
     */
    double GetFittingError() const { return m_fittingError; }
    
    /**
     * @brief 设置容差
     * @param tolerance 新的容差值
     */
    void SetTolerance(double tolerance) { m_tolerance = tolerance; }
    
    /**
     * @brief 获取容差
     * @return 当前容差值
     */
    double GetTolerance() const { return m_tolerance; }
    
    /**
     * @brief 设置是否优化参数
     * @param optimize 是否进行非线性优化
     */
    void SetOptimize(bool optimize) { m_optimize = optimize; }

protected:
    /**
     * @brief 从NURBS曲面采样点
     * @param surface NURBS曲面
     * @param points 输出：采样点数组
     * @param samplesU U方向采样数
     * @param samplesV V方向采样数
     */
    void SampleSurface(
        const Handle(Geom_BSplineSurface)& surface,
        std::vector<gp_Pnt>& points,
        int samplesU = Sampling::ADAPTIVE_SAMPLES,
        int samplesV = Sampling::ADAPTIVE_SAMPLES
    ) const;
    
    /**
     * @brief 计算点到曲面的距离误差
     * @param points 点集
     * @param surface 曲面
     * @return RMS距离误差
     */
    double ComputeRMSError(
        const std::vector<gp_Pnt>& points,
        const Handle(Geom_Surface)& surface
    ) const;
    
    /**
     * @brief 计算两点之间的距离
     * @param p1 点1
     * @param p2 点2
     * @return 欧氏距离
     */
    double Distance(const gp_Pnt& p1, const gp_Pnt& p2) const;

protected:
    double m_tolerance;      ///< 几何误差容差
    double m_fittingError;   ///< 拟合误差
    bool m_optimize;         ///< 是否进行优化
};

} // namespace BS2Quad

#endif // BS2QUAD_QUADRIC_FITTER_H

