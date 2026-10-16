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
     * @brief 仅拟合所给有限点，以最大点残差验收；不证明点间连续曲面。拟合圆柱面
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
    gp_Cylinder m_cylinder;  ///< 拟合得到的圆柱参数
};

} // namespace BS2Quad

#endif // BS2QUAD_CYLINDER_FITTER_H

