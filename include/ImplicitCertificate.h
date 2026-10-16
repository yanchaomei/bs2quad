#ifndef BS2QUAD_IMPLICIT_CERTIFICATE_H
#define BS2QUAD_IMPLICIT_CERTIFICATE_H

#include <Geom_BSplineSurface.hxx>
#include <Geom_Surface.hxx>
#include <limits>
#include <string>
#include <cstddef>

namespace BS2Quad {

struct ImplicitCertificateResult {
    bool certified = false;
    double upperBound = std::numeric_limits<double>::infinity();
    std::size_t patchCount = 0;
    std::string message;
};

/** Full parameter-domain, directed source-to-analytic-support distance bound.
 *
 * Positive-weight B-splines, including periodic and unclamped bases, are decomposed by interval
 * homogeneous knot insertion. Bernstein coefficients enclose the analytic
 * implicit residual on every rational Bezier patch. No sample maximum is used.
 * This does not certify trim boundaries, reverse coverage, normals, or G1.
 */
ImplicitCertificateResult CertifyImplicitDistance(
    const Handle(Geom_BSplineSurface)& source,
    const Handle(Geom_Surface)& analytic);

} // namespace BS2Quad
#endif
