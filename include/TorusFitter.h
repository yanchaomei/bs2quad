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
     * @brief 从采样点拟合圆环面
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
    /**
     * @brief 估计圆环轴线
     * @param points 采样点数组
     * @param axis 输出：估计的轴线
     */
    void EstimateAxis(const std::vector<gp_Pnt>& points, gp_Ax1& axis);
    
    /**
     * @brief 估计主半径和副半径
     * @param points 采样点数组
     * @param axis 圆环轴线
     * @param majorRadius 输出：主半径
     * @param minorRadius 输出：副半径
     */
    void EstimateRadii(
        const std::vector<gp_Pnt>& points,
        const gp_Ax1& axis,
        double& majorRadius,
        double& minorRadius
    );
    
    /**
     * @brief 优化圆环参数
     * @param points 采样点数组
     * @param axis 输入输出：圆环轴线
     * @param majorRadius 输入输出：主半径
     * @param minorRadius 输入输出：副半径
     */
    void OptimizeParameters(
        const std::vector<gp_Pnt>& points,
        gp_Ax1& axis,
        double& majorRadius,
        double& minorRadius
    );
    
    /**
     * @brief 计算点到圆环面的距离
     * @param point 点
     * @param axis 圆环轴线
     * @param majorRadius 主半径
     * @param minorRadius 副半径
     * @return 距离值
     */
    double DistanceToTorus(
        const gp_Pnt& point,
        const gp_Ax1& axis,
        double majorRadius,
        double minorRadius
    ) const;

private:
    gp_Torus m_torus;  ///< 拟合得到的圆环参数
};

} // namespace BS2Quad

#endif // BS2QUAD_TORUS_FITTER_H

