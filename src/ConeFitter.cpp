/**
 * @file ConeFitter.cpp
 * @brief 圆锥面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "ConeFitter.h"
#include <Geom_ConicalSurface.hxx>
#include <gp_Vec.hxx>
#include <gp_XYZ.hxx>
#include <cmath>
#include <algorithm>

namespace BS2Quad {

// 构造函数
ConeFitter::ConeFitter(double tolerance)
    : QuadricFitter(tolerance)
    , m_cone(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), M_PI/4, 1.0)
{
}

// 析构函数
ConeFitter::~ConeFitter() {
}

// 从NURBS曲面拟合圆锥面
bool ConeFitter::Fit(
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

// 从采样点拟合圆锥面
bool ConeFitter::FitFromPoints(
    const std::vector<gp_Pnt>& points,
    Handle(Geom_Surface)& quadricSurface
) {
    if (points.size() < 5) {
        return false;  // 至少需要5个点
    }
    
    // 估计圆锥轴线和顶点
    gp_Ax1 axis;
    gp_Pnt apex;
    EstimateAxisAndApex(points, axis, apex);
    
    // 估计半锥角
    double semiAngle = EstimateSemiAngle(points, axis, apex);
    
    // 如果启用优化，进行参数优化
    if (m_optimize) {
        OptimizeParameters(points, axis, apex, semiAngle);
    }
    
    // 创建圆锥几何对象
    gp_Ax2 position(apex, axis.Direction());
    m_cone = gp_Cone(position, semiAngle, 0.0);  // 0.0是参考半径
    
    // 创建OpenCASCADE圆锥面
    quadricSurface = new Geom_ConicalSurface(m_cone);
    
    // 计算拟合误差
    m_fittingError = ComputeRMSError(points, quadricSurface);
    
    // 检查误差是否在容差范围内
    return m_fittingError <= m_tolerance;
}

// 估计圆锥轴线和顶点
void ConeFitter::EstimateAxisAndApex(
    const std::vector<gp_Pnt>& points,
    gp_Ax1& axis,
    gp_Pnt& apex
) {
    // 计算点的质心
    gp_XYZ centroid(0, 0, 0);
    for (const auto& point : points) {
        centroid += point.XYZ();
    }
    centroid /= points.size();
    
    // 使用PCA估计主方向作为轴线方向
    double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    
    for (const auto& point : points) {
        gp_XYZ diff = point.XYZ() - centroid;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                cov[i][j] += diff.Coord(i+1) * diff.Coord(j+1);
            }
        }
    }
    
    // 幂法求最大特征向量
    gp_XYZ direction(1, 0, 0);
    for (int iter = 0; iter < 20; ++iter) {
        gp_XYZ newDir(0, 0, 0);
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                newDir.SetCoord(i+1, 
                    newDir.Coord(i+1) + cov[i][j] * direction.Coord(j+1));
            }
        }
        double norm = newDir.Modulus();
        if (norm > Precision::ZERO_TOLERANCE) {
            direction = newDir / norm;
        }
    }
    
    // 估计顶点：找到投影到轴线上距离最集中的点
    // 简化方法：使用质心作为初始顶点位置
    apex = gp_Pnt(centroid);
    axis = gp_Ax1(apex, gp_Dir(direction));
    
    // 精化顶点位置：找到使所有点到圆锥面距离最小的顶点
    // 使用点到轴线的距离和沿轴线的距离的关系
    double sumT = 0.0;
    double sumR = 0.0;
    int count = 0;
    
    for (const auto& point : points) {
        gp_Vec v(apex, point);
        double t = v.Dot(gp_Vec(axis.Direction()));
        gp_Pnt proj = apex.Translated(gp_Vec(axis.Direction()).Multiplied(t));
        double r = point.Distance(proj);
        
        if (std::abs(r) > Precision::ZERO_TOLERANCE) {
            sumT += t;
            sumR += r;
            count++;
        }
    }
    
    // 更新顶点位置
    if (count > 0) {
        double avgT = sumT / count;
        apex = apex.Translated(gp_Vec(axis.Direction()).Multiplied(-avgT * 0.5));
        axis.SetLocation(apex);
    }
}

// 估计半锥角
double ConeFitter::EstimateSemiAngle(
    const std::vector<gp_Pnt>& points,
    const gp_Ax1& axis,
    const gp_Pnt& apex
) {
    std::vector<double> angles;
    angles.reserve(points.size());
    
    for (const auto& point : points) {
        gp_Vec v(apex, point);
        double dist = v.Magnitude();
        
        if (dist < Precision::ZERO_TOLERANCE) {
            continue;
        }
        
        // 计算点到轴线的距离
        double t = v.Dot(gp_Vec(axis.Direction()));
        gp_Pnt proj = apex.Translated(gp_Vec(axis.Direction()).Multiplied(t));
        double r = point.Distance(proj);
        
        // 计算半锥角：tan(angle) = r / |t|
        if (std::abs(t) > Precision::ZERO_TOLERANCE) {
            double angle = std::atan2(r, std::abs(t));
            angles.push_back(angle);
        }
    }
    
    if (angles.empty()) {
        return M_PI / 4;  // 默认45度
    }
    
    // 计算平均半锥角
    double sumAngle = 0.0;
    for (double angle : angles) {
        sumAngle += angle;
    }
    
    return sumAngle / angles.size();
}

// 优化圆锥参数
void ConeFitter::OptimizeParameters(
    const std::vector<gp_Pnt>& points,
    gp_Ax1& axis,
    gp_Pnt& apex,
    double& semiAngle
) {
    // 使用迭代最小二乘法优化
    for (int iter = 0; iter < 10; ++iter) {
        // 重新估计半锥角
        double newSemiAngle = EstimateSemiAngle(points, axis, apex);
        
        // 检查收敛
        if (std::abs(newSemiAngle - semiAngle) < Precision::ANGLE_TOLERANCE_RAD) {
            break;
        }
        
        semiAngle = newSemiAngle;
    }
}

// 计算点到圆锥面的距离
double ConeFitter::DistanceToCone(
    const gp_Pnt& point,
    const gp_Ax1& axis,
    const gp_Pnt& apex,
    double semiAngle
) const {
    gp_Vec v(apex, point);
    double t = v.Dot(gp_Vec(axis.Direction()));
    
    // 计算点在轴线上的投影
    gp_Pnt proj = apex.Translated(gp_Vec(axis.Direction()).Multiplied(t));
    double r = point.Distance(proj);
    
    // 圆锥面上对应点的半径：r_cone = |t| * tan(semiAngle)
    double r_cone = std::abs(t) * std::tan(semiAngle);
    
    // 距离误差
    return std::abs(r - r_cone);
}

} // namespace BS2Quad

