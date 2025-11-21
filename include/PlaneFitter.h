/**
 * @file PlaneFitter.h
 * @brief 平面拟合器
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_PLANE_FITTER_H
#define BS2QUAD_PLANE_FITTER_H

#include "QuadricFitter.h"
#include <Geom_Plane.hxx>
#include <gp_Pln.hxx>

namespace BS2Quad {

/**
 * @brief 平面拟合器
 * 
 * 从NURBS曲面或采样点拟合平面参数（法向量、位置）
 */
class PlaneFitter : public QuadricFitter {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit PlaneFitter(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 析构函数
     */
    ~PlaneFitter() override;
    
    /**
     * @brief 从NURBS曲面拟合平面
     * @param surface NURBS B样条曲面
     * @param quadricSurface 输出：拟合得到的平面
     * @return 是否成功拟合
     */
    bool Fit(
        const Handle(Geom_BSplineSurface)& surface,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 从采样点拟合平面
     * @param points 采样点数组
     * @param quadricSurface 输出：拟合得到的平面
     * @return 是否成功拟合
     */
    bool FitFromPoints(
        const std::vector<gp_Pnt>& points,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 获取拟合的平面参数
     * @return 平面几何对象
     */
    const gp_Pln& GetPlane() const { return m_plane; }
    
    /**
     * @brief 获取法向量
     * @return 平面法向量
     */
    gp_Dir GetNormal() const { return m_plane.Axis().Direction(); }
    
    /**
     * @brief 获取平面上一点
     * @return 平面位置
     */
    gp_Pnt GetLocation() const { return m_plane.Location(); }

private:
    /**
     * @brief 使用SVD分解拟合平面
     * @param points 采样点数组
     * @param normal 输出：平面法向量
     * @param location 输出：平面上一点
     */
    void FitPlaneUsingSVD(
        const std::vector<gp_Pnt>& points,
        gp_Dir& normal,
        gp_Pnt& location
    );
    
    /**
     * @brief 计算点的质心
     * @param points 采样点数组
     * @return 质心
     */
    gp_Pnt ComputeCentroid(const std::vector<gp_Pnt>& points) const;
    
    /**
     * @brief 验证平面度
     * @param points 采样点数组
     * @param plane 平面
     * @return 平面度误差（最大偏差）
     */
    double ValidatePlanarity(
        const std::vector<gp_Pnt>& points,
        const gp_Pln& plane
    ) const;
    
    /**
     * @brief 计算点到平面的距离
     * @param point 点
     * @param plane 平面
     * @return 距离（带符号）
     */
    double DistanceToPlane(const gp_Pnt& point, const gp_Pln& plane) const;

private:
    gp_Pln m_plane;  ///< 拟合得到的平面参数
};

} // namespace BS2Quad

#endif // BS2QUAD_PLANE_FITTER_H

