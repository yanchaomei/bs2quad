/**
 * @file ConeFitter.h
 * @brief 圆锥面拟合器
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_CONE_FITTER_H
#define BS2QUAD_CONE_FITTER_H

#include "QuadricFitter.h"
#include <Geom_ConicalSurface.hxx>
#include <gp_Cone.hxx>

namespace BS2Quad {

/**
 * @brief 圆锥面拟合器
 * 
 * 从NURBS曲面或采样点拟合圆锥面参数（轴线、顶点、半锥角）
 */
class ConeFitter : public QuadricFitter {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit ConeFitter(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 析构函数
     */
    ~ConeFitter() override;
    
    /**
     * @brief 从NURBS曲面拟合圆锥面
     * @param surface NURBS B样条曲面
     * @param quadricSurface 输出：拟合得到的圆锥面
     * @return 是否成功拟合
     */
    bool Fit(
        const Handle(Geom_BSplineSurface)& surface,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 从采样点拟合圆锥面
     * @param points 采样点数组
     * @param quadricSurface 输出：拟合得到的圆锥面
     * @return 是否成功拟合
     */
    bool FitFromPoints(
        const std::vector<gp_Pnt>& points,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 获取拟合的圆锥参数
     * @return 圆锥几何对象
     */
    const gp_Cone& GetCone() const { return m_cone; }
    
    /**
     * @brief 获取轴线
     * @return 圆锥轴线
     */
    gp_Ax1 GetAxis() const { return m_cone.Axis(); }
    
    /**
     * @brief 获取半锥角
     * @return 半锥角（弧度）
     */
    double GetSemiAngle() const { return m_cone.SemiAngle(); }
    
    /**
     * @brief 获取顶点
     * @return 圆锥顶点
     */
    gp_Pnt GetApex() const { return m_cone.Apex(); }

private:
    /**
     * @brief 估计圆锥轴线和顶点
     * @param points 采样点数组
     * @param axis 输出：估计的轴线
     * @param apex 输出：估计的顶点
     */
    void EstimateAxisAndApex(
        const std::vector<gp_Pnt>& points,
        gp_Ax1& axis,
        gp_Pnt& apex
    );
    
    /**
     * @brief 估计半锥角
     * @param points 采样点数组
     * @param axis 圆锥轴线
     * @param apex 圆锥顶点
     * @return 半锥角（弧度）
     */
    double EstimateSemiAngle(
        const std::vector<gp_Pnt>& points,
        const gp_Ax1& axis,
        const gp_Pnt& apex
    );
    
    /**
     * @brief 优化圆锥参数
     * @param points 采样点数组
     * @param axis 输入输出：圆锥轴线
     * @param apex 输入输出：圆锥顶点
     * @param semiAngle 输入输出：半锥角
     */
    void OptimizeParameters(
        const std::vector<gp_Pnt>& points,
        gp_Ax1& axis,
        gp_Pnt& apex,
        double& semiAngle
    );
    
    /**
     * @brief 计算点到圆锥面的距离
     * @param point 点
     * @param axis 圆锥轴线
     * @param apex 圆锥顶点
     * @param semiAngle 半锥角
     * @return 距离值
     */
    double DistanceToCone(
        const gp_Pnt& point,
        const gp_Ax1& axis,
        const gp_Pnt& apex,
        double semiAngle
    ) const;

private:
    gp_Cone m_cone;  ///< 拟合得到的圆锥参数
};

} // namespace BS2Quad

#endif // BS2QUAD_CONE_FITTER_H

