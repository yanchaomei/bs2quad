/**
 * @file PlaneFitter.cpp
 * @brief 平面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "PlaneFitter.h"
#include "AnalyticRecovery.h"
#include "ImplicitCertificate.h"
#include "PointFitting.h"
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
    quadricSurface.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto candidate=RecoverAnalytic(surface,m_tolerance,QuadricType::PLANE);
    if(candidate.surface.IsNull()) return false;
    const auto bound=CertifyImplicitDistance(surface,candidate.surface);
    if(!bound.certified||bound.upperBound>m_tolerance) return false;
    const auto analytic=Handle(Geom_Plane)::DownCast(candidate.surface);
    if(analytic.IsNull()) return false;
    m_plane=analytic->Pln();
    m_fittingError=bound.upperBound;
    quadricSurface=candidate.surface;
    return true;
}

// 从采样点拟合平面
bool PlaneFitter::FitFromPoints(const std::vector<gp_Pnt>& points,Handle(Geom_Surface)& output) {
    output.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto fitted=FitFinitePoints(points,QuadricType::PLANE,m_tolerance,m_optimize);
    if(fitted.surface.IsNull())return false;
    const auto analytic=Handle(Geom_Plane)::DownCast(fitted.surface);
    if(analytic.IsNull())return false;
    m_plane=analytic->Pln();
    m_fittingError=fitted.rmsResidual;
    output=fitted.surface;
    return true;
}
} // namespace BS2Quad
