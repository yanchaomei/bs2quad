/**
 * @file ConeFitter.cpp
 * @brief 圆锥面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "ConeFitter.h"
#include "AnalyticRecovery.h"
#include "ImplicitCertificate.h"
#include "PointFitting.h"
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
    quadricSurface.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto candidate=RecoverAnalytic(surface,m_tolerance,QuadricType::CONE);
    if(candidate.surface.IsNull()) return false;
    const auto bound=CertifyImplicitDistance(surface,candidate.surface);
    if(!bound.certified||bound.upperBound>m_tolerance) return false;
    const auto analytic=Handle(Geom_ConicalSurface)::DownCast(candidate.surface);
    if(analytic.IsNull()) return false;
    m_cone=analytic->Cone();
    m_fittingError=bound.upperBound;
    quadricSurface=candidate.surface;
    return true;
}

// 从采样点拟合圆锥面
bool ConeFitter::FitFromPoints(const std::vector<gp_Pnt>& points,Handle(Geom_Surface)& output) {
    output.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto fitted=FitFinitePoints(points,QuadricType::CONE,m_tolerance,m_optimize);
    if(fitted.surface.IsNull())return false;
    const auto analytic=Handle(Geom_ConicalSurface)::DownCast(fitted.surface);
    if(analytic.IsNull())return false;
    m_cone=analytic->Cone();
    m_fittingError=fitted.rmsResidual;
    output=fitted.surface;
    return true;
}
} // namespace BS2Quad
