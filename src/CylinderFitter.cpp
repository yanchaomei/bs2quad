/**
 * @file CylinderFitter.cpp
 * @brief 圆柱面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "CylinderFitter.h"
#include <Geom_CylindricalSurface.hxx>
#include <gp_Vec.hxx>
#include <gp_XYZ.hxx>
#include <cmath>
#include <algorithm>

namespace BS2Quad {

// 构造函数
CylinderFitter::CylinderFitter(double tolerance)
    : QuadricFitter(tolerance)
    , m_cylinder(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 1.0)
{
}

// 析构函数
CylinderFitter::~CylinderFitter() {
}

// 从NURBS曲面拟合圆柱面
bool CylinderFitter::Fit(
    const Handle(Geom_BSplineSurface)& surface,
    Handle(Geom_Surface)& quadricSurface
) {
    if (surface.IsNull()) {
        return false;
    }
    
    // 采样曲面
    std::vector<gp_Pnt> points;
    SampleSurface(surface, points);
    
    // 从采样点拟合
    return FitFromPoints(points, quadricSurface);
}

// 从采样点拟合圆柱面
bool CylinderFitter::FitFromPoints(
    const std::vector<gp_Pnt>& points,
    Handle(Geom_Surface)& quadricSurface
) {
    if (points.size() < 6) {
        return false;  // 至少需要6个点
    }
    
    // 估计圆柱轴线
    gp_Ax1 axis;
    EstimateAxisDirection(points, axis);
    
    // 计算半径
    double radius = 0.0;
    for (const auto& point : points) {
        radius += DistanceToAxis(point, axis);
    }
    radius /= points.size();
    
    // 如果启用优化，进行参数优化
    if (m_optimize) {
        OptimizeParameters(points, axis, radius);
    }
    
    // 创建圆柱几何对象
    gp_Ax2 position(axis.Location(), axis.Direction());
    m_cylinder = gp_Cylinder(position, radius);
    
    // 创建OpenCASCADE圆柱面
    quadricSurface = new Geom_CylindricalSurface(m_cylinder);
    
    // 计算拟合误差
    m_fittingError = ComputeRMSError(points, quadricSurface);
    
    // 检查误差是否在容差范围内
    return m_fittingError <= m_tolerance;
}

// 估计圆柱轴线方向（使用PCA）
void CylinderFitter::EstimateAxisDirection(
    const std::vector<gp_Pnt>& points,
    gp_Ax1& axis
) {
    // 计算点的质心
    gp_XYZ centroid(0, 0, 0);
    for (const auto& point : points) {
        centroid += point.XYZ();
    }
    centroid /= points.size();
    
    // 构建协方差矩阵
    double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    
    for (const auto& point : points) {
        gp_XYZ diff = point.XYZ() - centroid;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                cov[i][j] += diff.Coord(i+1) * diff.Coord(j+1);
            }
        }
    }
    
    // 简化的PCA：找最大特征值对应的特征向量
    // 这里使用幂法（Power Iteration）近似
    gp_XYZ direction(1, 0, 0);  // 初始向量
    
    for (int iter = 0; iter < 20; ++iter) {
        gp_XYZ newDir(0, 0, 0);
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                newDir.SetCoord(i+1, 
                    newDir.Coord(i+1) + cov[i][j] * direction.Coord(j+1));
            }
        }
        
        // 归一化
        double norm = newDir.Modulus();
        if (norm > Precision::ZERO_TOLERANCE) {
            direction = newDir / norm;
        }
    }
    
    // 设置轴线
    axis = gp_Ax1(gp_Pnt(centroid), gp_Dir(direction));
}

// 优化圆柱参数（使用最小二乘法）
void CylinderFitter::OptimizeParameters(
    const std::vector<gp_Pnt>& points,
    gp_Ax1& axis,
    double& radius
) {
    // 简化的优化：固定轴线方向，优化轴线位置和半径
    // 使用迭代方法
    
    for (int iter = 0; iter < 10; ++iter) {
        // 计算每个点到轴线的投影
        gp_XYZ axisPoint(0, 0, 0);
        double sumWeights = 0.0;
        
        for (const auto& point : points) {
            gp_Pnt proj = ProjectToAxis(point, axis);
            axisPoint += proj.XYZ();
            sumWeights += 1.0;
        }
        
        if (sumWeights > 0) {
            axisPoint /= sumWeights;
        }
        
        // 更新轴线位置
        axis.SetLocation(gp_Pnt(axisPoint));
        
        // 重新计算半径
        double newRadius = 0.0;
        for (const auto& point : points) {
            newRadius += DistanceToAxis(point, axis);
        }
        newRadius /= points.size();
        
        // 检查收敛
        if (std::abs(newRadius - radius) < Precision::ZERO_TOLERANCE) {
            break;
        }
        
        radius = newRadius;
    }
}

// 计算点到轴线的距离
double CylinderFitter::DistanceToAxis(const gp_Pnt& point, const gp_Ax1& axis) const {
    gp_Pnt proj = ProjectToAxis(point, axis);
    return point.Distance(proj);
}

// 计算点在轴线上的投影
gp_Pnt CylinderFitter::ProjectToAxis(const gp_Pnt& point, const gp_Ax1& axis) const {
    gp_Vec v(axis.Location(), point);
    gp_Dir dir = axis.Direction();
    
    // 计算投影长度
    double t = v.Dot(gp_Vec(dir));
    
    // 计算投影点
    gp_Pnt proj = axis.Location().Translated(gp_Vec(dir).Multiplied(t));
    return proj;
}

} // namespace BS2Quad

