/**
 * @file QuadricFitter.cpp
 * @brief 二次曲面参数拟合基类实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "QuadricFitter.h"
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <cmath>

namespace BS2Quad {

// 构造函数
QuadricFitter::QuadricFitter(double tolerance)
    : m_tolerance(tolerance)
    , m_fittingError(0.0)
    , m_optimize(true)
{
}

// 析构函数
QuadricFitter::~QuadricFitter() {
}

// 从NURBS曲面采样点
void QuadricFitter::SampleSurface(
    const Handle(Geom_BSplineSurface)& surface,
    std::vector<gp_Pnt>& points,
    int samplesU,
    int samplesV
) const {
    if (surface.IsNull()) {
        return;
    }
    
    // 获取参数域
    double uMin, uMax, vMin, vMax;
    surface->Bounds(uMin, uMax, vMin, vMax);
    
    // 清空点集
    points.clear();
    points.reserve(samplesU * samplesV);
    
    // 均匀采样
    for (int i = 0; i < samplesU; ++i) {
        double u = uMin + (uMax - uMin) * i / (samplesU - 1);
        
        for (int j = 0; j < samplesV; ++j) {
            double v = vMin + (vMax - vMin) * j / (samplesV - 1);
            
            try {
                gp_Pnt point = surface->Value(u, v);
                points.push_back(point);
            } catch (...) {
                // 忽略无效参数点
                continue;
            }
        }
    }
}

// 计算点到曲面的距离误差
double QuadricFitter::ComputeRMSError(
    const std::vector<gp_Pnt>& points,
    const Handle(Geom_Surface)& surface
) const {
    if (points.empty() || surface.IsNull()) {
        return 0.0;
    }
    
    double sumSquaredError = 0.0;
    int validPoints = 0;
    
    // 遍历所有点，计算到曲面的距离
    for (const auto& point : points) {
        try {
            // 使用OpenCASCADE的投影API
            GeomAPI_ProjectPointOnSurf projector(point, surface);
            
            if (projector.NbPoints() > 0) {
                double distance = projector.LowerDistance();
                sumSquaredError += distance * distance;
                validPoints++;
            }
        } catch (...) {
            // 忽略投影失败的点
            continue;
        }
    }
    
    // 计算RMS误差
    if (validPoints > 0) {
        return std::sqrt(sumSquaredError / validPoints);
    }
    
    return 0.0;
}

// 计算两点之间的距离
double QuadricFitter::Distance(const gp_Pnt& p1, const gp_Pnt& p2) const {
    double dx = p2.X() - p1.X();
    double dy = p2.Y() - p1.Y();
    double dz = p2.Z() - p1.Z();
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

} // namespace BS2Quad

