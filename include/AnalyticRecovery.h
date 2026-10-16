#ifndef BS2QUAD_ANALYTIC_RECOVERY_H
#define BS2QUAD_ANALYTIC_RECOVERY_H

#include "Common.h"
#include <Geom_BSplineSurface.hxx>
#include <Geom_Surface.hxx>

namespace BS2Quad {
struct AnalyticCandidate {
    Handle(Geom_Surface) surface;
    QuadricType type = QuadricType::NONE;
    ErrorType error = ErrorType::NON_QUADRIC;
    double kernelGap = std::numeric_limits<double>::infinity();
    std::string message;
};

// Candidate generation uses OCCT's canonical recovery. The kernel's reported
// gap is diagnostic; independent validation owns acceptance.
AnalyticCandidate RecoverAnalytic(const Handle(Geom_BSplineSurface)& source,
    double tolerance, QuadricType target = QuadricType::NONE);
ErrorType CheckNurbsInput(const Handle(Geom_BSplineSurface)& source,
    std::string& message);
}
#endif
