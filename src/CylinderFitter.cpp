/**
 * @file CylinderFitter.cpp
 * @brief 圆柱面拟合器实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "CylinderFitter.h"
#include "AnalyticRecovery.h"
#include "ImplicitCertificate.h"
#include "PointFitting.h"
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
    quadricSurface.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto candidate=RecoverAnalytic(surface,m_tolerance,QuadricType::CYLINDER);
    if(candidate.surface.IsNull()) return false;
    const auto bound=CertifyImplicitDistance(surface,candidate.surface);
    if(!bound.certified||bound.upperBound>m_tolerance) return false;
    const auto analytic=Handle(Geom_CylindricalSurface)::DownCast(candidate.surface);
    if(analytic.IsNull()) return false;
    m_cylinder=analytic->Cylinder();
    m_fittingError=bound.upperBound;
    quadricSurface=candidate.surface;
    return true;
}

// 从采样点拟合圆柱面
bool CylinderFitter::FitFromPoints(const std::vector<gp_Pnt>& points,Handle(Geom_Surface)& output) {
    output.Nullify();
    m_fittingError=std::numeric_limits<double>::infinity();
    const auto fitted=FitFinitePoints(points,QuadricType::CYLINDER,m_tolerance,m_optimize);
    if(fitted.surface.IsNull())return false;
    const auto analytic=Handle(Geom_CylindricalSurface)::DownCast(fitted.surface);
    if(analytic.IsNull())return false;
    m_cylinder=analytic->Cylinder();
    m_fittingError=fitted.rmsResidual;
    output=fitted.surface;
    return true;
}
} // namespace BS2Quad
