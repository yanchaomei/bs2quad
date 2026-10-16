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
     * @brief 仅拟合所给有限点，以最大点残差验收；不证明点间连续曲面。拟合平面
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
    gp_Pln m_plane;  ///< 拟合得到的平面参数
};

} // namespace BS2Quad

#endif // BS2QUAD_PLANE_FITTER_H

