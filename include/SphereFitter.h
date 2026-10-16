/**
 * @file SphereFitter.h
 * @brief 球面拟合器
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_SPHERE_FITTER_H
#define BS2QUAD_SPHERE_FITTER_H

#include "QuadricFitter.h"
#include <Geom_SphericalSurface.hxx>
#include <gp_Sphere.hxx>

namespace BS2Quad {

/**
 * @brief 球面拟合器
 * 
 * 从NURBS曲面或采样点拟合球面参数（球心、半径）
 */
class SphereFitter : public QuadricFitter {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit SphereFitter(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 析构函数
     */
    ~SphereFitter() override;
    
    /**
     * @brief 从NURBS曲面拟合球面
     * @param surface NURBS B样条曲面
     * @param quadricSurface 输出：拟合得到的球面
     * @return 是否成功拟合
     */
    bool Fit(
        const Handle(Geom_BSplineSurface)& surface,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 仅拟合所给有限点，以最大点残差验收；不证明点间连续曲面。拟合球面
     * @param points 采样点数组
     * @param quadricSurface 输出：拟合得到的球面
     * @return 是否成功拟合
     */
    bool FitFromPoints(
        const std::vector<gp_Pnt>& points,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 获取拟合的球面参数
     * @return 球几何对象
     */
    const gp_Sphere& GetSphere() const { return m_sphere; }
    
    /**
     * @brief 获取球心
     * @return 球心坐标
     */
    gp_Pnt GetCenter() const { return m_sphere.Location(); }
    
    /**
     * @brief 获取半径
     * @return 球半径
     */
    double GetRadius() const { return m_sphere.Radius(); }

private:
    gp_Sphere m_sphere;  ///< 拟合得到的球面参数
};

} // namespace BS2Quad

#endif // BS2QUAD_SPHERE_FITTER_H

