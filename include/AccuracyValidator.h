#ifndef BS2QUAD_ACCURACY_VALIDATOR_H
#define BS2QUAD_ACCURACY_VALIDATOR_H

#include "Common.h"
#include <Geom_Surface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <vector>

namespace BS2Quad {

class AccuracyValidator {
public:
    explicit AccuracyValidator(double tolerance = Precision::DEFAULT_TOLERANCE);
    ~AccuracyValidator();

    // Sample statistics are diagnostic. Acceptance additionally requires an
    // independent directed full-source-domain bound to the target support.
    // No boundary, normal, or adjacent-face G1 claim is made by this overload.
    AccuracyResult ValidateAccuracy(const Handle(Geom_BSplineSurface)& nurbsSurface,
                                    const Handle(Geom_Surface)& quadricSurface);

    // Uses finite trimmed source UV bounds and world-space geometry. Boundary
    // union distance and orientation-aware normal preservation are sampled
    // checks; their values are not certified continuous Hausdorff/G1 bounds.
    AccuracyResult ValidateAccuracy(const TopoDS_Face& nurbsFace,
                                    const TopoDS_Face& quadricFace);

    // Separate adjacency check: faces must share an actual topological edge.
    // Evaluates tangent planes at the same edge points, with 0.1 degree gate.
    // Face orientation is respected. Opposite normals fail the specified
    // normal-angle criterion even though their unoriented tangent planes agree.
    AccuracyResult ValidateAdjacentFacesG1(const TopoDS_Face& first,
                                          const TopoDS_Face& second);

    // Caller establishes original topological adjacency before rebuilding a
    // face. This checks the supplied reference edge lies on both trimmed face
    // boundaries and compares corresponding world-space normals there.
    AccuracyResult ValidateNormalsAlongEdge(const TopoDS_Face& first,
                                            const TopoDS_Face& second,
                                            const TopoDS_Edge& referenceSharedEdge);

    PerformanceResult ComparePerformance(const Handle(Geom_BSplineSurface)& nurbsSurface,
                                         const Handle(Geom_Surface)& quadricSurface);
    PerformanceResult ComparePerformance(const TopoDS_Face& nurbsFace,
                                         const TopoDS_Face& quadricFace);

    void SetTolerance(double tolerance);
    double GetTolerance() const { return m_tolerance; }
    void SetPerformanceIterations(int iterations);

private:
    double ComputeHausdorffDistance(const TopoDS_Face& first,const TopoDS_Face& second);
    double ComputeNormalPreservation(const TopoDS_Face& source,const TopoDS_Face& target);
    double BenchmarkProjection(const Handle(Geom_Surface)& surface,const std::vector<gp_Pnt>& points);
    double BenchmarkIntersection(const Handle(Geom_Surface)& surface);
    double BenchmarkBoolean(const TopoDS_Face& face);
    double GetTimeInMilliseconds() const;
    double m_tolerance;
    int m_perfIterations;
    int m_samplesU;
    int m_samplesV;
};
} // namespace BS2Quad
#endif
