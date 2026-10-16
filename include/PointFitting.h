#ifndef BS2QUAD_POINT_FITTING_H
#define BS2QUAD_POINT_FITTING_H
#include "Common.h"
#include <Geom_Surface.hxx>
#include <limits>
namespace BS2Quad {
struct PointFitResult {
    Handle(Geom_Surface) surface;
    double maximumResidual=std::numeric_limits<double>::infinity();
    double rmsResidual=std::numeric_limits<double>::infinity();
};
// Fits only the supplied finite points. The maximum geometric point residual
// gates acceptance. This does not certify any surface between the points.
// Degenerate/ill-conditioned data and unconverged bounded searches are rejected.
// Practical minimum counts: plane3, sphere4, cylinder6, cone10, ring-torus12.
// Nonlinear refinement is bounded to six starts / 100 iterations per start;
// cone half-angles stay in [1e-4, pi/2-1e-4]. Radii are capped at
// 1e4 times cloud RMS scale. These are sufficient-fit searches, not complete
// primitive-recognition algorithms; informative partial patches can still fail.
PointFitResult FitFinitePoints(const std::vector<gp_Pnt>& points,QuadricType type,
                              double tolerance,bool refine=true);
}
#endif
