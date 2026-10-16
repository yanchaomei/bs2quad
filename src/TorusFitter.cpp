/**
 * @file TorusFitter.cpp
 * @brief 圆环面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "TorusFitter.h"
#include "AnalyticRecovery.h"
#include "ImplicitCertificate.h"
#include "PointFitting.h"
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
    quadricSurface.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto candidate=RecoverAnalytic(surface,m_tolerance,QuadricType::TORUS);
    if(candidate.surface.IsNull()) return false;
    const auto bound=CertifyImplicitDistance(surface,candidate.surface);
    if(!bound.certified||bound.upperBound>m_tolerance) return false;
    const auto analytic=Handle(Geom_ToroidalSurface)::DownCast(candidate.surface);
    if(analytic.IsNull()) return false;
    m_torus=analytic->Torus();
    m_fittingError=bound.upperBound;
    quadricSurface=candidate.surface;
    return true;
}

// 从采样点拟合圆环面
bool TorusFitter::FitFromPoints(const std::vector<gp_Pnt>& points,Handle(Geom_Surface)& output) {
    output.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto fitted=FitFinitePoints(points,QuadricType::TORUS,m_tolerance,m_optimize);
    if(fitted.surface.IsNull())return false;
    const auto analytic=Handle(Geom_ToroidalSurface)::DownCast(fitted.surface);
    if(analytic.IsNull())return false;
    m_torus=analytic->Torus();
    m_fittingError=fitted.rmsResidual;
    output=fitted.surface;
    return true;
}
} // namespace BS2Quad
