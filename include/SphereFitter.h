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
     * @brief 从采样点拟合球面
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
    /**
     * @brief 估计球心（采样点质心）
     * @param points 采样点数组
     * @return 估计的球心
     */
    gp_Pnt EstimateCenter(const std::vector<gp_Pnt>& points);
    
    /**
     * @brief 估计半径
     * @param points 采样点数组
     * @param center 球心
     * @return 估计的半径
     */
    double EstimateRadius(const std::vector<gp_Pnt>& points, const gp_Pnt& center);
    
    /**
     * @brief 优化球面参数（使用最小二乘法）
     * @param points 采样点数组
     * @param center 输入输出：球心
     * @param radius 输入输出：半径
     */
    void OptimizeParameters(
        const std::vector<gp_Pnt>& points,
        gp_Pnt& center,
        double& radius
    );
    
    /**
     * @brief 验证球面度（检查标准差）
     * @param points 采样点数组
     * @param center 球心
     * @param radius 半径
     * @return 标准差
     */
    double ValidateSphericity(
        const std::vector<gp_Pnt>& points,
        const gp_Pnt& center,
        double radius
    ) const;

private:
    gp_Sphere m_sphere;  ///< 拟合得到的球面参数
};

} // namespace BS2Quad

#endif // BS2QUAD_SPHERE_FITTER_H

