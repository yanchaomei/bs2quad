/**
 * @file TorusFitter.cpp
 * @brief 圆环面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "TorusFitter.h"
#include <Geom_ToroidalSurface.hxx>
#include <gp_Vec.hxx>
#include <gp_XYZ.hxx>
#include <cmath>
#include <algorithm>

namespace BS2Quad {

// 构造函数
TorusFitter::TorusFitter(double tolerance)
    : QuadricFitter(tolerance)
    , m_torus(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 2.0, 0.5)
{
}

// 析构函数
TorusFitter::~TorusFitter() {
}

// 从NURBS曲面拟合圆环面
bool TorusFitter::Fit(
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

// 从采样点拟合圆环面
bool TorusFitter::FitFromPoints(
    const std::vector<gp_Pnt>& points,
    Handle(Geom_Surface)& quadricSurface
) {
    if (points.size() < 8) {
        return false;  // 至少需要8个点
    }
    
    // 估计圆环轴线
    gp_Ax1 axis;
    EstimateAxis(points, axis);
    
    // 估计主半径和副半径
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    EstimateRadii(points, axis, majorRadius, minorRadius);
    
    // 如果启用优化，进行参数优化
    if (m_optimize) {
        OptimizeParameters(points, axis, majorRadius, minorRadius);
    }
    
    // 创建圆环几何对象
    gp_Ax2 position(axis.Location(), axis.Direction());
    m_torus = gp_Torus(position, majorRadius, minorRadius);
    
    // 创建OpenCASCADE圆环面
    quadricSurface = new Geom_ToroidalSurface(m_torus);
    
    // 计算拟合误差
    m_fittingError = ComputeRMSError(points, quadricSurface);
    
    // 检查误差是否在容差范围内
    return m_fittingError <= m_tolerance;
}

// 估计圆环轴线
void TorusFitter::EstimateAxis(const std::vector<gp_Pnt>& points, gp_Ax1& axis) {
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
    
    // 幂法求最小特征向量（法向量）
    gp_XYZ direction(0, 0, 1);
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
    
    axis = gp_Ax1(gp_Pnt(centroid), gp_Dir(direction));
}

// 估计主半径和副半径
void TorusFitter::EstimateRadii(
    const std::vector<gp_Pnt>& points,
    const gp_Ax1& axis,
    double& majorRadius,
    double& minorRadius
) {
    // 计算每个点到轴线的距离
    std::vector<double> distancesToAxis;
    distancesToAxis.reserve(points.size());
    
    for (const auto& point : points) {
        gp_Vec v(axis.Location(), point);
        double t = v.Dot(gp_Vec(axis.Direction()));
        gp_Pnt proj = axis.Location().Translated(gp_Vec(axis.Direction()).Multiplied(t));
        double dist = point.Distance(proj);
        distancesToAxis.push_back(dist);
    }
    
    // 主半径是到轴线距离的平均值
    double sumDist = 0.0;
    for (double dist : distancesToAxis) {
        sumDist += dist;
    }
    majorRadius = sumDist / distancesToAxis.size();
    
    // 副半径是距离的标准差
    double sumSquaredDev = 0.0;
    for (double dist : distancesToAxis) {
        double dev = dist - majorRadius;
        sumSquaredDev += dev * dev;
    }
    minorRadius = std::sqrt(sumSquaredDev / distancesToAxis.size());
    
    // 确保半径为正且合理
    if (minorRadius < Precision::ZERO_TOLERANCE) {
        minorRadius = majorRadius * 0.1;  // 默认副半径为主半径的10%
    }
    
    if (minorRadius > majorRadius * 0.9) {
        minorRadius = majorRadius * 0.5;  // 限制副半径不超过主半径的50%
    }
}

// 优化圆环参数
void TorusFitter::OptimizeParameters(
    const std::vector<gp_Pnt>& points,
    gp_Ax1& axis,
    double& majorRadius,
    double& minorRadius
) {
    // 使用迭代最小二乘法优化
    for (int iter = 0; iter < 10; ++iter) {
        double oldMajor = majorRadius;
        double oldMinor = minorRadius;
        
        // 重新估计半径
        EstimateRadii(points, axis, majorRadius, minorRadius);
        
        // 检查收敛
        if (std::abs(majorRadius - oldMajor) < Precision::ZERO_TOLERANCE &&
            std::abs(minorRadius - oldMinor) < Precision::ZERO_TOLERANCE) {
            break;
        }
    }
}

// 计算点到圆环面的距离
double TorusFitter::DistanceToTorus(
    const gp_Pnt& point,
    const gp_Ax1& axis,
    double majorRadius,
    double minorRadius
) const {
    // 计算点到轴线的投影
    gp_Vec v(axis.Location(), point);
    double t = v.Dot(gp_Vec(axis.Direction()));
    gp_Pnt projOnAxis = axis.Location().Translated(gp_Vec(axis.Direction()).Multiplied(t));
    
    // 计算点到轴线的距离
    double distToAxis = point.Distance(projOnAxis);
    
    // 计算中心圆上最近点
    gp_Vec radialDir(projOnAxis, point);
    if (radialDir.Magnitude() < Precision::ZERO_TOLERANCE) {
        radialDir = gp_Vec(1, 0, 0);  // 默认方向
    } else {
        radialDir.Normalize();
    }
    
    gp_Pnt centerCirclePoint = projOnAxis.Translated(radialDir.Multiplied(majorRadius));
    
    // 点到圆环面的距离近似为点到中心圆最近点的距离与副半径的差
    double distToCenter = point.Distance(centerCirclePoint);
    return std::abs(distToCenter - minorRadius);
}

} // namespace BS2Quad

