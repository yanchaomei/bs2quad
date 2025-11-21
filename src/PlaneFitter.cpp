/**
 * @file PlaneFitter.cpp
 * @brief 平面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "PlaneFitter.h"
#include <Geom_Plane.hxx>
#include <gp_XYZ.hxx>
#include <cmath>
#include <algorithm>

namespace BS2Quad {

// 构造函数
PlaneFitter::PlaneFitter(double tolerance)
    : QuadricFitter(tolerance)
    , m_plane(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1))
{
}

// 析构函数
PlaneFitter::~PlaneFitter() {
}

// 从NURBS曲面拟合平面
bool PlaneFitter::Fit(
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

// 从采样点拟合平面
bool PlaneFitter::FitFromPoints(
    const std::vector<gp_Pnt>& points,
    Handle(Geom_Surface)& quadricSurface
) {
    if (points.size() < 3) {
        return false;  // 至少需要3个点
    }
    
    // 使用SVD分解拟合平面
    gp_Dir normal;
    gp_Pnt location;
    FitPlaneUsingSVD(points, normal, location);
    
    // 创建平面几何对象
    m_plane = gp_Pln(location, normal);
    
    // 创建OpenCASCADE平面
    quadricSurface = new Geom_Plane(m_plane);
    
    // 验证平面度
    double planarity = ValidatePlanarity(points, m_plane);
    
    // 计算拟合误差
    m_fittingError = planarity;
    
    // 检查误差是否在容差范围内
    return m_fittingError <= m_tolerance;
}

// 使用SVD分解拟合平面
void PlaneFitter::FitPlaneUsingSVD(
    const std::vector<gp_Pnt>& points,
    gp_Dir& normal,
    gp_Pnt& location
) {
    // 计算质心
    location = ComputeCentroid(points);
    
    // 构建协方差矩阵
    double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    
    for (const auto& point : points) {
        gp_XYZ diff = point.XYZ() - location.XYZ();
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                cov[i][j] += diff.Coord(i+1) * diff.Coord(j+1);
            }
        }
    }
    
    // 简化的SVD：找最小特征值对应的特征向量（法向量）
    // 使用逆幂法（Inverse Power Iteration）
    gp_XYZ normalVec(0, 0, 1);  // 初始向量
    
    // 逆幂法迭代
    for (int iter = 0; iter < 20; ++iter) {
        gp_XYZ newVec(0, 0, 0);
        
        // 简化求解：Cov * v = lambda * v
        // 找最小特征值，即最小方差方向（法向量）
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                newVec.SetCoord(i+1, 
                    newVec.Coord(i+1) + cov[i][j] * normalVec.Coord(j+1));
            }
        }
        
        // 归一化
        double norm = newVec.Modulus();
        if (norm > Precision::ZERO_TOLERANCE) {
            normalVec = newVec / norm;
        }
    }
    
    // 对于平面拟合，我们实际上需要最小方差方向
    // 简化方法：计算各方向的方差，选择最小的
    double variance[3] = {0, 0, 0};
    for (const auto& point : points) {
        gp_XYZ diff = point.XYZ() - location.XYZ();
        for (int i = 0; i < 3; ++i) {
            variance[i] += diff.Coord(i+1) * diff.Coord(i+1);
        }
    }
    
    // 找最小方差的坐标轴作为近似法向量
    int minIdx = 0;
    for (int i = 1; i < 3; ++i) {
        if (variance[i] < variance[minIdx]) {
            minIdx = i;
        }
    }
    
    // 设置法向量
    if (minIdx == 0) {
        normalVec.SetCoord(1.0, 0.0, 0.0);
    } else if (minIdx == 1) {
        normalVec.SetCoord(0.0, 1.0, 0.0);
    } else {
        normalVec.SetCoord(0.0, 0.0, 1.0);
    }
    
    // 精化法向量：使用所有点的平均法向
    gp_XYZ sumNormal(0, 0, 0);
    int count = 0;
    
    // 使用最小二乘法拟合平面：Ax + By + Cz + D = 0
    // 通过中心化的点来计算
    double A = 0, B = 0, C = 0;
    for (size_t i = 0; i < points.size() - 2; ++i) {
        gp_Vec v1(location, points[i]);
        gp_Vec v2(location, points[i+1]);
        gp_Vec cross = v1.Crossed(v2);
        if (cross.Magnitude() > Precision::ZERO_TOLERANCE) {
            sumNormal += cross.XYZ().Normalized();
            count++;
        }
    }
    
    if (count > 0) {
        normalVec = sumNormal / count;
        double norm = normalVec.Modulus();
        if (norm > Precision::ZERO_TOLERANCE) {
            normalVec /= norm;
        }
    }
    
    normal = gp_Dir(normalVec);
}

// 计算点的质心
gp_Pnt PlaneFitter::ComputeCentroid(const std::vector<gp_Pnt>& points) const {
    gp_XYZ centroid(0, 0, 0);
    
    for (const auto& point : points) {
        centroid += point.XYZ();
    }
    
    centroid /= points.size();
    
    return gp_Pnt(centroid);
}

// 验证平面度
double PlaneFitter::ValidatePlanarity(
    const std::vector<gp_Pnt>& points,
    const gp_Pln& plane
) const {
    if (points.empty()) {
        return 0.0;
    }
    
    double maxDeviation = 0.0;
    double sumSquaredDev = 0.0;
    
    for (const auto& point : points) {
        double dist = std::abs(DistanceToPlane(point, plane));
        maxDeviation = std::max(maxDeviation, dist);
        sumSquaredDev += dist * dist;
    }
    
    // 返回RMS偏差
    return std::sqrt(sumSquaredDev / points.size());
}

// 计算点到平面的距离
double PlaneFitter::DistanceToPlane(const gp_Pnt& point, const gp_Pln& plane) const {
    gp_Vec v(plane.Location(), point);
    gp_Dir normal = plane.Axis().Direction();
    
    // 计算点到平面的有向距离
    return v.Dot(gp_Vec(normal));
}

} // namespace BS2Quad

