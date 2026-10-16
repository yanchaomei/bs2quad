/**
 * @file TorusFitter.h
 * @brief 圆环面拟合器
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_TORUS_FITTER_H
#define BS2QUAD_TORUS_FITTER_H

#include "QuadricFitter.h"
#include <Geom_ToroidalSurface.hxx>
#include <gp_Torus.hxx>

namespace BS2Quad {

/**
 * @brief 圆环面拟合器
 * 
 * 从NURBS曲面或采样点拟合圆环面参数（轴线、主半径、副半径）
 */
class TorusFitter : public QuadricFitter {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit TorusFitter(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 析构函数
     */
    ~TorusFitter() override;
    
    /**
     * @brief 从NURBS曲面拟合圆环面
     * @param surface NURBS B样条曲面
     * @param quadricSurface 输出：拟合得到的圆环面
     * @return 是否成功拟合
     */
    bool Fit(
        const Handle(Geom_BSplineSurface)& surface,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 仅拟合所给有限点，以最大点残差验收；不证明点间连续曲面。拟合圆环面
     * @param points 采样点数组
     * @param quadricSurface 输出：拟合得到的圆环面
     * @return 是否成功拟合
     */
    bool FitFromPoints(
        const std::vector<gp_Pnt>& points,
        Handle(Geom_Surface)& quadricSurface
    ) override;
    
    /**
     * @brief 获取拟合的圆环面参数
     * @return 圆环几何对象
     */
    const gp_Torus& GetTorus() const { return m_torus; }
    
    /**
     * @brief 获取主半径
     * @return 主半径（中心圆半径）
     */
    double GetMajorRadius() const { return m_torus.MajorRadius(); }
    
    /**
     * @brief 获取副半径
     * @return 副半径（管道半径）
     */
    double GetMinorRadius() const { return m_torus.MinorRadius(); }
    
    /**
     * @brief 获取轴线
     * @return 圆环轴线
     */
    gp_Ax1 GetAxis() const { return m_torus.Axis(); }

private:
    gp_Torus m_torus;  ///< 拟合得到的圆环参数
};

} // namespace BS2Quad

#endif // BS2QUAD_TORUS_FITTER_H

