/**
 * @file SphereFitter.cpp
 * @brief 球面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "SphereFitter.h"
#include "AnalyticRecovery.h"
#include "ImplicitCertificate.h"
#include "PointFitting.h"
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
    quadricSurface.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto candidate=RecoverAnalytic(surface,m_tolerance,QuadricType::SPHERE);
    if(candidate.surface.IsNull()) return false;
    const auto bound=CertifyImplicitDistance(surface,candidate.surface);
    if(!bound.certified||bound.upperBound>m_tolerance) return false;
    const auto analytic=Handle(Geom_SphericalSurface)::DownCast(candidate.surface);
    if(analytic.IsNull()) return false;
    m_sphere=analytic->Sphere();
    m_fittingError=bound.upperBound;
    quadricSurface=candidate.surface;
    return true;
}

// 从采样点拟合球面
bool SphereFitter::FitFromPoints(const std::vector<gp_Pnt>& points,Handle(Geom_Surface)& output) {
    output.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto fitted=FitFinitePoints(points,QuadricType::SPHERE,m_tolerance,m_optimize);
    if(fitted.surface.IsNull())return false;
    const auto analytic=Handle(Geom_SphericalSurface)::DownCast(fitted.surface);
    if(analytic.IsNull())return false;
    m_sphere=analytic->Sphere();
    m_fittingError=fitted.rmsResidual;
    output=fitted.surface;
    return true;
}
} // namespace BS2Quad
