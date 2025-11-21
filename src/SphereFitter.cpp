/**
 * @file SphereFitter.cpp
 * @brief 球面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "SphereFitter.h"
#include <Geom_SphericalSurface.hxx>
#include <gp_XYZ.hxx>
#include <cmath>

namespace BS2Quad {

// 构造函数
SphereFitter::SphereFitter(double tolerance)
    : QuadricFitter(tolerance)
    , m_sphere(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 1.0)
{
}

// 析构函数
SphereFitter::~SphereFitter() {
}

// 从NURBS曲面拟合球面
bool SphereFitter::Fit(
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

// 从采样点拟合球面
bool SphereFitter::FitFromPoints(
    const std::vector<gp_Pnt>& points,
    Handle(Geom_Surface)& quadricSurface
) {
    if (points.size() < 4) {
        return false;  // 至少需要4个点
    }
    
    // 估计球心
    gp_Pnt center = EstimateCenter(points);
    
    // 估计半径
    double radius = EstimateRadius(points, center);
    
    // 如果启用优化，进行参数优化
    if (m_optimize) {
        OptimizeParameters(points, center, radius);
    }
    
    // 创建球几何对象
    gp_Ax2 position(center, gp_Dir(0, 0, 1));
    m_sphere = gp_Sphere(position, radius);
    
    // 创建OpenCASCADE球面
    quadricSurface = new Geom_SphericalSurface(m_sphere);
    
    // 计算拟合误差
    m_fittingError = ComputeRMSError(points, quadricSurface);
    
    // 验证球面度
    double sphericity = ValidateSphericity(points, center, radius);
    
    // 检查误差是否在容差范围内
    return m_fittingError <= m_tolerance && sphericity <= m_tolerance;
}

// 估计球心（采样点质心）
gp_Pnt SphereFitter::EstimateCenter(const std::vector<gp_Pnt>& points) {
    gp_XYZ centroid(0, 0, 0);
    
    for (const auto& point : points) {
        centroid += point.XYZ();
    }
    
    centroid /= points.size();
    
    return gp_Pnt(centroid);
}

// 估计半径
double SphereFitter::EstimateRadius(
    const std::vector<gp_Pnt>& points,
    const gp_Pnt& center
) {
    double sumRadius = 0.0;
    
    for (const auto& point : points) {
        sumRadius += point.Distance(center);
    }
    
    return sumRadius / points.size();
}

// 优化球面参数（使用最小二乘法）
void SphereFitter::OptimizeParameters(
    const std::vector<gp_Pnt>& points,
    gp_Pnt& center,
    double& radius
) {
    // 使用Gauss-Newton迭代优化
    // 优化目标：最小化 sum((||pi - center|| - radius)^2)
    
    for (int iter = 0; iter < 20; ++iter) {
        // 计算梯度和Hessian矩阵（简化版）
        gp_XYZ gradCenter(0, 0, 0);
        double gradRadius = 0.0;
        double oldRadius = radius;
        gp_XYZ oldCenter = center.XYZ();
        
        // 计算残差和梯度
        for (const auto& point : points) {
            gp_Vec vec(center, point);
            double dist = vec.Magnitude();
            
            if (dist < Precision::ZERO_TOLERANCE) {
                continue;
            }
            
            double residual = dist - radius;
            
            // 梯度：对球心的偏导
            gradCenter += vec.XYZ() * (residual / dist);
            
            // 梯度：对半径的偏导
            gradRadius += -residual;
        }
        
        // 更新参数（简单梯度下降）
        double alpha = 0.1;  // 学习率
        center.SetXYZ(oldCenter - gradCenter * alpha / points.size());
        radius = oldRadius - gradRadius * alpha / points.size();
        
        // 检查收敛
        if (gradCenter.Modulus() < Precision::ZERO_TOLERANCE * 10 && 
            std::abs(gradRadius) < Precision::ZERO_TOLERANCE * 10) {
            break;
        }
    }
    
    // 确保半径为正
    if (radius < Precision::ZERO_TOLERANCE) {
        radius = EstimateRadius(points, center);
    }
}

// 验证球面度（检查标准差）
double SphereFitter::ValidateSphericity(
    const std::vector<gp_Pnt>& points,
    const gp_Pnt& center,
    double radius
) const {
    if (points.empty()) {
        return 0.0;
    }
    
    double sumSquaredDev = 0.0;
    
    for (const auto& point : points) {
        double dist = point.Distance(center);
        double deviation = dist - radius;
        sumSquaredDev += deviation * deviation;
    }
    
    return std::sqrt(sumSquaredDev / points.size());
}

} // namespace BS2Quad

