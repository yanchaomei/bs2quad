/**
 * @file CylinderFitter.h
 * @brief 圆柱面拟合器
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_CYLINDER_FITTER_H
#define BS2QUAD_CYLINDER_FITTER_H

#include "QuadricFitter.h"
#include <Geom_CylindricalSurface.hxx>
#include <gp_Cylinder.hxx>

namespace BS2Quad {

/**
 * @brief 圆柱面拟合器
 * 
 * 从NURBS曲面或采样点拟合圆柱面参数（轴线、半径）
 */
class CylinderFitter : public QuadricFitter {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit CylinderFitter(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 析构函数
     */
    ~CylinderFitter() override;
    
    /**
     * @brief 从NURBS曲面拟合圆柱面
     * @param surface NURBS B样条曲面
     * @param quadricSurface 输出：拟合得到的圆柱面
     * @return 是否成功拟合
     */
    bool Fit(
        const Handle(Geom_BSplineSurface)& surface,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 从采样点拟合圆柱面
     * @param points 采样点数组
     * @param quadricSurface 输出：拟合得到的圆柱面
     * @return 是否成功拟合
     */
    bool FitFromPoints(
        const std::vector<gp_Pnt>& points,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 获取拟合的圆柱参数
     * @return 圆柱几何对象
     */
    const gp_Cylinder& GetCylinder() const { return m_cylinder; }
    
    /**
     * @brief 获取轴线
     * @return 圆柱轴线
     */
    gp_Ax1 GetAxis() const { return m_cylinder.Axis(); }
    
    /**
     * @brief 获取半径
     * @return 圆柱半径
     */
    double GetRadius() const { return m_cylinder.Radius(); }

private:
    /**
     * @brief 估计圆柱轴线方向（使用PCA）
     * @param points 采样点数组
     * @param axis 输出：估计的轴线
     */
    void EstimateAxisDirection(const std::vector<gp_Pnt>& points, gp_Ax1& axis);
    
    /**
     * @brief 优化圆柱参数（使用最小二乘法）
     * @param points 采样点数组
     * @param axis 输入输出：圆柱轴线
     * @param radius 输入输出：圆柱半径
     */
    void OptimizeParameters(
        const std::vector<gp_Pnt>& points,
        gp_Ax1& axis,
        double& radius
    );
    
    /**
     * @brief 计算点到轴线的距离
     * @param point 点
     * @param axis 轴线
     * @return 距离值
     */
    double DistanceToAxis(const gp_Pnt& point, const gp_Ax1& axis) const;
    
    /**
     * @brief 计算点在轴线上的投影
     * @param point 点
     * @param axis 轴线
     * @return 投影点
     */
    gp_Pnt ProjectToAxis(const gp_Pnt& point, const gp_Ax1& axis) const;

private:
    gp_Cylinder m_cylinder;  ///< 拟合得到的圆柱参数
};

} // namespace BS2Quad

#endif // BS2QUAD_CYLINDER_FITTER_H

