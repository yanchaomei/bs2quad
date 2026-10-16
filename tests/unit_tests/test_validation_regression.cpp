#include "AccuracyValidator.h"
#include "BS2QuadCore.h"
#include <BRep_Builder.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <ShapeFix_Edge.hxx>
#include <TopExp_Explorer.hxx>
#include "ImplicitCertificate.h"
#include <gtest/gtest.h>
#include <GeomConvert.hxx>
#include <Geom_Plane.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <gp_Trsf.hxx>
#include <TopLoc_Location.hxx>
#include <array>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <gp_Pln.hxx>
#include <TopoDS.hxx>
#include <limits>
#include <TColgp_Array2OfPnt.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <stdexcept>

using namespace BS2Quad;

namespace {
Handle(Geom_BSplineSurface) PlanarPatch(double z = 0.0) {
    Handle(Geom_Surface) plane = new Geom_Plane(gp_Pnt(0, 0, z), gp_Dir(0, 0, 1));
    return GeomConvert::SurfaceToBSplineSurface(
        new Geom_RectangularTrimmedSurface(plane, -2.0, 3.0, -4.0, 5.0));
}
}

TEST(ValidationRegression, RejectsNonPositiveOrNonFiniteToleranceWithoutChangingState) {
    EXPECT_THROW(AccuracyValidator(0.0), std::invalid_argument);
    EXPECT_THROW(AccuracyValidator(-1.0), std::invalid_argument);
    EXPECT_THROW(AccuracyValidator(std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_THROW(AccuracyValidator(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    AccuracyValidator validator;
    EXPECT_THROW(validator.SetTolerance(0.0), std::invalid_argument);
    EXPECT_THROW(validator.SetTolerance(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    EXPECT_DOUBLE_EQ(validator.GetTolerance(), Precision::DEFAULT_TOLERANCE);
}

TEST(ValidationRegression, IdenticalTrimmedFaceHasZeroBoundaryUnionDistance) {
    auto source = PlanarPatch();
    TopoDS_Face face = BRepBuilderAPI_MakeFace(source, 1e-7).Face();
    const auto result = AccuracyValidator().ValidateAccuracy(face, face);
    EXPECT_TRUE(result.meetsRequirements);
    EXPECT_NEAR(result.hausdorffDistance, 0.0, 1e-10);
}

TEST(ValidationRegression, MissingInputsCannotBecomeZeroErrorOrSuccessfulChecks) {
    const auto result = AccuracyValidator().ValidateAccuracy(
        Handle(Geom_BSplineSurface)(), Handle(Geom_Surface)());
    EXPECT_FALSE(result.meetsRequirements);
    EXPECT_FALSE(result.validationComplete);
    EXPECT_FALSE(result.surfaceChecked);
    EXPECT_FALSE(result.boundaryChecked);
    EXPECT_FALSE(result.normalChecked);
    EXPECT_FALSE(result.adjacencyChecked);
    EXPECT_FALSE(result.maxDistanceCertified);
    EXPECT_TRUE(std::isinf(result.maxDistance));
    EXPECT_FALSE(result.message.empty());
}

TEST(ValidationRegression, SurfaceOnlyResultDoesNotClaimBoundaryOrAdjacencyChecks) {
    Handle(Geom_Surface) plane = new Geom_Plane(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    const auto result = AccuracyValidator().ValidateAccuracy(PlanarPatch(), plane);
    EXPECT_TRUE(result.meetsRequirements) << result.message;
    EXPECT_TRUE(result.validationComplete);
    EXPECT_TRUE(result.surfaceChecked);
    EXPECT_TRUE(result.maxDistanceCertified);
    EXPECT_LE(result.certifiedMaxDistance, Precision::DEFAULT_TOLERANCE);
    EXPECT_GT(result.sampleCount, 0u);
    EXPECT_EQ(result.failedSampleCount, 0u);
    EXPECT_FALSE(result.boundaryChecked);
    EXPECT_FALSE(result.normalChecked);
    EXPECT_FALSE(result.adjacencyChecked);
    EXPECT_TRUE(std::isinf(result.hausdorffDistance));
    EXPECT_TRUE(std::isinf(result.g1ContinuityError));
}

TEST(ValidationRegression, OffsetWithinToleranceDoesNotFailAnInventedMeanErrorThreshold) {
    Handle(Geom_Surface) plane = new Geom_Plane(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    const auto result = AccuracyValidator(1e-3).ValidateAccuracy(PlanarPatch(7.5e-4), plane);
    EXPECT_TRUE(result.meetsRequirements) << result.message;
    EXPECT_NEAR(result.maxDistance, 7.5e-4, 1e-12);
    EXPECT_NEAR(result.avgDistance, 7.5e-4, 1e-12);
    EXPECT_NEAR(result.rmsDistance, 7.5e-4, 1e-12);
}

TEST(ValidationRegression, FullDomainCertificateRejectsNarrowBumpBetweenAllSamplingColumns) {
    // A 0.01 mm tent is entirely between consecutive columns of the 50x50
    // validation grid. A zero sample maximum cannot certify this surface.
    TColgp_Array2OfPnt poles(1, 5, 1, 2);
    const double xs[] = {0.0, 0.4001, 0.40015, 0.4002, 1.0};
    for (int i = 1; i <= 5; ++i) {
        for (int j = 1; j <= 2; ++j) {
            poles(i,j) = gp_Pnt(xs[i-1], j-1, i == 3 ? 0.01 : 0.0);
        }
    }
    TColStd_Array1OfReal uk(1, 5), vk(1, 2);
    TColStd_Array1OfInteger um(1, 5), vm(1, 2);
    for (int i = 1; i <= 5; ++i) { uk(i)=xs[i-1]; um(i)=(i==1||i==5)?2:1; }
    vk(1)=0; vk(2)=1; vm(1)=2; vm(2)=2;
    Handle(Geom_BSplineSurface) source = new Geom_BSplineSurface(poles,uk,vk,um,vm,1,1);
    Handle(Geom_Surface) plane = new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1));
    const auto result = AccuracyValidator().ValidateAccuracy(source,plane);
    EXPECT_FALSE(result.meetsRequirements);
    ASSERT_TRUE(result.maxDistanceCertified) << result.message;
    EXPECT_GE(result.certifiedMaxDistance,0.01);
    EXPECT_NEAR(source->Value(0.40015,0.5).Z(),0.01,1e-12);
}

TEST(ValidationRegression, FaceOrientationReversalIsNotNormalPreservation) {
    TopoDS_Face source = BRepBuilderAPI_MakeFace(PlanarPatch(),1e-7).Face();
    TopoDS_Face reversed = source;
    reversed.Reverse();
    const auto result = AccuracyValidator().ValidateAccuracy(source,reversed);
    EXPECT_FALSE(result.meetsRequirements);
    EXPECT_TRUE(result.normalChecked) << result.message;
    EXPECT_NEAR(result.normalDeviationDegrees,180.0,1e-8);
    EXPECT_FALSE(result.adjacencyChecked);
    EXPECT_TRUE(std::isinf(result.g1ContinuityError));
}

namespace {
std::pair<TopoDS_Face,TopoDS_Face> AdjacentPlanarFaces(TopoDS_Edge& shared,bool crease) {
    const gp_Pnt a(0,0,0),b(0,1,0),c(-1,1,0),d(-1,0,0);
    shared=BRepBuilderAPI_MakeEdge(a,b).Edge();
    BRepBuilderAPI_MakeWire left;
    left.Add(shared);
    left.Add(BRepBuilderAPI_MakeEdge(b,c).Edge());
    left.Add(BRepBuilderAPI_MakeEdge(c,d).Edge());
    left.Add(BRepBuilderAPI_MakeEdge(d,a).Edge());
    const gp_Pnt e=crease?gp_Pnt(0,0,1):gp_Pnt(1,0,0);
    const gp_Pnt f=crease?gp_Pnt(0,1,1):gp_Pnt(1,1,0);
    BRepBuilderAPI_MakeWire right;
    right.Add(TopoDS::Edge(shared.Reversed()));
    right.Add(BRepBuilderAPI_MakeEdge(a,e).Edge());
    right.Add(BRepBuilderAPI_MakeEdge(e,f).Edge());
    right.Add(BRepBuilderAPI_MakeEdge(f,b).Edge());
    return {BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),left.Wire()).Face(),
            BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),crease?gp_Dir(1,0,0):gp_Dir(0,0,1)),right.Wire()).Face()};
}
}

TEST(ValidationRegression, CoplanarActualNeighborsPassSeparateSharedEdgeG1Check) {
    TopoDS_Edge edge;
    const auto faces=AdjacentPlanarFaces(edge,false);
    const auto result=AccuracyValidator().ValidateAdjacentFacesG1(faces.first,faces.second);
    EXPECT_TRUE(result.meetsRequirements) << result.message;
    EXPECT_TRUE(result.adjacencyChecked);
    EXPECT_FALSE(result.normalChecked);
    EXPECT_NEAR(result.g1ContinuityError,0.0,1e-10);
    const auto reference=AccuracyValidator().ValidateNormalsAlongEdge(faces.first,faces.second,edge);
    EXPECT_TRUE(reference.meetsRequirements) << reference.message;
}

TEST(ValidationRegression, ActualSharedEdgeCreaseIsRejectedAtNinetyDegrees) {
    TopoDS_Edge edge;
    const auto faces=AdjacentPlanarFaces(edge,true);
    const auto result=AccuracyValidator().ValidateNormalsAlongEdge(faces.first,faces.second,edge);
    EXPECT_FALSE(result.meetsRequirements);
    EXPECT_TRUE(result.adjacencyChecked) << result.message;
    EXPECT_NEAR(result.g1ContinuityError,90.0,1e-8);
}

TEST(ValidationRegression, ReferenceEdgeMustLieOnBothTrimmedBoundaries) {
    TopoDS_Edge edge;
    const auto faces=AdjacentPlanarFaces(edge,false);
    const auto far=BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,100),gp_Pnt(0,1,100)).Edge();
    const auto result=AccuracyValidator().ValidateNormalsAlongEdge(faces.first,faces.second,far);
    EXPECT_FALSE(result.meetsRequirements);
    EXPECT_FALSE(result.validationComplete);
    EXPECT_FALSE(result.adjacencyChecked);
    EXPECT_TRUE(std::isinf(result.g1ContinuityError));
}

TEST(ValidationRegression, PerformanceReportsOnlyCompletedOperationPairs) {
    AccuracyValidator validator;
    validator.SetPerformanceIterations(1);
    Handle(Geom_Surface) plane=new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1));
    const auto result=validator.ComparePerformance(PlanarPatch(),plane);
    EXPECT_TRUE(result.projectionComplete) << result.message;
    EXPECT_TRUE(std::isfinite(result.nurbsProjectionTime));
    EXPECT_TRUE(std::isfinite(result.quadricProjectionTime));
    EXPECT_GT(result.nurbsProjectionTime,0.0);
    EXPECT_GT(result.quadricProjectionTime,0.0);
    EXPECT_FALSE(result.booleanComplete);
    EXPECT_TRUE(std::isnan(result.GetBooleanSpeedup()));
    const auto missing=validator.ComparePerformance(Handle(Geom_BSplineSurface)(),plane);
    EXPECT_FALSE(missing.projectionComplete);
    EXPECT_FALSE(missing.intersectionComplete);
    EXPECT_TRUE(std::isinf(missing.nurbsProjectionTime));
    EXPECT_TRUE(std::isnan(missing.GetProjectionSpeedup()));
}

TEST(ValidationRegression, IndependentFullDomainBoundsCoverAllFiveAnalyticFamilies) {
    const gp_Ax3 frame(gp_Pnt(123,-24,15),gp_Dir(1,2,3));
    const std::vector<Handle(Geom_Surface)> surfaces={
        new Geom_Plane(frame),new Geom_CylindricalSurface(frame,7.0),
        new Geom_SphericalSurface(frame,7.0),new Geom_ConicalSurface(frame,0.4,5.0),
        new Geom_ToroidalSurface(frame,15.0,3.0)};
    const std::array<std::array<double,4>,5> trims={{{-.4,.5,-.3,.6},{.2,1.4,-2,3},
        {.2,1.4,-.8,.7},{.2,1.4,0,5},{.2,1.4,.3,2}}};
    for(std::size_t i=0;i<surfaces.size();++i) {
        SCOPED_TRACE(i);
        const auto& t=trims[i];
        auto source=GeomConvert::SurfaceToBSplineSurface(
            new Geom_RectangularTrimmedSurface(surfaces[i],t[0],t[1],t[2],t[3]));
        const auto result=AccuracyValidator().ValidateAccuracy(source,surfaces[i]);
        EXPECT_TRUE(result.meetsRequirements) << result.message << " upper=" << result.certifiedMaxDistance;
        EXPECT_TRUE(result.maxDistanceCertified);
        EXPECT_LE(result.maxDistance,result.certifiedMaxDistance+1e-10);
        EXPECT_LE(result.certifiedMaxDistance,1e-5);
    }
}

TEST(ValidationRegression, FacePlacementIsAppliedBeforeComparisonWithWorldSpaceAnalyticSupport) {
    auto source= BRepBuilderAPI_MakeFace(PlanarPatch(),1e-7).Face();
    gp_Trsf transform;
    transform.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,2,3)),0.6);
    transform.SetTranslationPart(gp_Vec(120,-70,30));
    source.Move(TopLoc_Location(transform));
    Handle(Geom_Surface) local=new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1));
    const auto world=Handle(Geom_Surface)::DownCast(local->Transformed(transform));
    const auto target=BRepBuilderAPI_MakeFace(world,-2.0,3.0,-4.0,5.0,1e-7).Face();
    const auto result=AccuracyValidator().ValidateAccuracy(source,target);
    EXPECT_TRUE(result.meetsRequirements) << result.message;
    EXPECT_LE(result.hausdorffDistance,1e-8);
    EXPECT_LE(result.normalDeviationDegrees,1e-8);
}

TEST(ValidationRegression, NormalPreservationUsesCorrespondingWorldPointsAcrossParameterizations) {
    Handle(Geom_Surface) original=new Geom_CylindricalSurface(gp_Ax3(),5.0);
    auto source=GeomConvert::SurfaceToBSplineSurface(
        new Geom_RectangularTrimmedSurface(original,0.3,1.8,-2.0,9.0));
    const auto face=BRepBuilderAPI_MakeFace(source,1e-7).Face();
    gp_Ax3 rotated;
    rotated.Rotate(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),0.7);
    Handle(Geom_Surface) equivalent=new Geom_CylindricalSurface(rotated,5.0);
    const auto target=BRepBuilderAPI_MakeFace(equivalent,-0.4,1.1,-2.0,9.0,1e-7).Face();
    const auto result=AccuracyValidator().ValidateAccuracy(face,target);
    EXPECT_TRUE(result.meetsRequirements) << result.message;
    EXPECT_LE(result.normalDeviationDegrees,1e-7);
    EXPECT_LE(result.hausdorffDistance,1e-7);
}

TEST(ValidationRegression, FailedSourceEvaluationCannotBeDiscardedFromStatistics) {
    auto source=PlanarPatch();
    source->SetPole(1,1,gp_Pnt(std::numeric_limits<double>::quiet_NaN(),0,0));
    Handle(Geom_Surface) plane=new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1));
    const auto result=AccuracyValidator().ValidateAccuracy(source,plane);
    EXPECT_FALSE(result.meetsRequirements);
    EXPECT_FALSE(result.validationComplete);
    EXPECT_FALSE(result.surfaceChecked);
    EXPECT_GT(result.failedSampleCount,0u);
    EXPECT_TRUE(std::isinf(result.maxDistance));
    EXPECT_FALSE(result.message.empty());
}

TEST(ValidationRegression, ReverseBoundaryDirectionDetectsAnExtraInteriorHole) {
    const auto source=BRepBuilderAPI_MakeFace(PlanarPatch(),1e-7).Face();
    BRepBuilderAPI_MakePolygon hole;
    hole.Add(gp_Pnt(-.5,-.5,0)); hole.Add(gp_Pnt(-.5,.5,0));
    hole.Add(gp_Pnt(.5,.5,0)); hole.Add(gp_Pnt(.5,-.5,0)); hole.Close();
    BRepBuilderAPI_MakeFace target(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),-2.0,3.0,-4.0,5.0);
    target.Add(hole.Wire());
    const auto result=AccuracyValidator().ValidateAccuracy(source,target.Face());
    EXPECT_FALSE(result.meetsRequirements);
    ASSERT_TRUE(result.boundaryChecked) << result.message;
    // Every original outer-boundary point is on the target boundary. Only
    // reverse max-min sees the additional inner boundary, 1.5+ mm away.
    EXPECT_GT(result.hausdorffDistance,1.0);
}

TEST(ValidationRegression, BoundaryLimitStaysOneEminusFiveWhenSupportToleranceIsRelaxed) {
    const auto source=BRepBuilderAPI_MakeFace(PlanarPatch(),1e-7).Face();
    Handle(Geom_Surface) plane=new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1));
    const auto shifted=BRepBuilderAPI_MakeFace(plane,-1.9999,3.0001,-4.0,5.0,1e-7).Face();
    const auto result=AccuracyValidator(1e-3).ValidateAccuracy(source,shifted);
    EXPECT_FALSE(result.meetsRequirements);
    ASSERT_TRUE(result.boundaryChecked) << result.message;
    EXPECT_GT(result.hausdorffDistance,1e-5);
    EXPECT_LT(result.hausdorffDistance,1e-3);
}

TEST(ValidationRegression, OriginalPeriodicRepresentationsReceiveFullDomainCertificates) {
    const gp_Ax3 frame(gp_Pnt(12,-7,3),gp_Dir(1,2,3));
    const std::vector<Handle(Geom_Surface)> supports={
        new Geom_CylindricalSurface(frame,7.0),new Geom_SphericalSurface(frame,7.0),
        new Geom_ConicalSurface(frame,0.4,5.0),new Geom_ToroidalSurface(frame,15.0,3.0)};
    for(std::size_t i=0;i<supports.size();++i) {
        SCOPED_TRACE(i);
        const double v0=i==1?-.8:0.0,v1=i==3?2*M_PI:1.3;
        const auto source=GeomConvert::SurfaceToBSplineSurface(
            new Geom_RectangularTrimmedSurface(supports[i],0.0,2*M_PI,v0,v1));
        ASSERT_TRUE(source->IsUPeriodic());
        const auto result=AccuracyValidator().ValidateAccuracy(source,supports[i]);
        EXPECT_TRUE(result.meetsRequirements) << result.message << " bound=" << result.certifiedMaxDistance;
        EXPECT_TRUE(result.maxDistanceCertified);
        EXPECT_LE(result.certifiedMaxDistance,1e-5);
    }
}

TEST(ValidationRegression, LowMultiplicityPeriodicSeamBoundsTheOriginalSurfaceAcrossWrap) {
    TColgp_Array2OfPnt poles(1,6,1,2);
    for(int i=1;i<=6;++i) for(int j=1;j<=2;++j)
        poles(i,j)=gp_Pnt(i*i,i+j,i==1?0.1:0.0);
    TColStd_Array1OfReal uk(1,7),vk(1,2);
    TColStd_Array1OfInteger um(1,7),vm(1,2);
    for(int i=1;i<=7;++i) { uk(i)=i-1; um(i)=1; }
    vk(1)=0;vk(2)=1;vm(1)=2;vm(2)=2;
    Handle(Geom_BSplineSurface) source=new Geom_BSplineSurface(poles,uk,vk,um,vm,3,1,true,false);
    Handle(Geom_Surface) plane=new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1));
    const auto result=AccuracyValidator(0.2).ValidateAccuracy(source,plane);
    ASSERT_TRUE(result.maxDistanceCertified) << result.message;
    EXPECT_TRUE(result.meetsRequirements) << result.message;
    EXPECT_LE(result.certifiedMaxDistance,0.1+1e-10);
    // Independent kernel evaluations cover both sides of the cyclic seam.
    for(int i=0;i<=1024;++i) {
        const auto p=source->Value(6.0*i/1024.0,0.37);
        EXPECT_LE(std::abs(p.Z()),result.certifiedMaxDistance+1e-12);
    }
}

TEST(ValidationRegression, UnclampedHighEndpointCannotDropTheLastControlPoleFromItsBound) {
    TColgp_Array2OfPnt poles(1,6,1,2);
    for(int i=1;i<=6;++i) for(int j=1;j<=2;++j)
        poles.SetValue(i,j,gp_Pnt(i-1,j-1,i==6?0.01:0.0));
    TColStd_Array1OfReal u(1,10),v(1,2);
    TColStd_Array1OfInteger um(1,10),vm(1,2);
    for(int i=1;i<=10;++i) {u(i)=i-4.0;um(i)=1;}
    v(1)=0.0;v(2)=1.0;vm(1)=2;vm(2)=2;
    Handle(Geom_BSplineSurface) source=new Geom_BSplineSurface(poles,u,v,um,vm,3,1);
    Handle(Geom_Surface) plane=new Geom_Plane(gp_Pnt(),gp_Dir(0,0,1));
    const auto certificate=CertifyImplicitDistance(source,plane);
    ASSERT_TRUE(certificate.certified) << certificate.message;
    double lo,hi,vl,vh;source->Bounds(lo,hi,vl,vh);
    const double endpointHeight=source->Value(hi,0.5).Z();
    EXPECT_GT(endpointHeight,0.001);
    EXPECT_GE(certificate.upperBound,endpointHeight-1e-12);
    EXPECT_FALSE(AccuracyValidator().ValidateAccuracy(source,plane).meetsRequirements);
}

TEST(ValidationRegression, IdentityCertificateCannotHideAnInvalidSourceControlPoint) {
    auto source=PlanarPatch();
    source->SetPole(1,1,gp_Pnt(std::numeric_limits<double>::quiet_NaN(),0,0));
    const auto result=CertifyImplicitDistance(source,source);
    EXPECT_FALSE(result.certified);
    EXPECT_FALSE(result.message.empty());
}

TEST(ValidationRegression, ReversedNeighborCannotPassTheSpecifiedOrientedNormalAngleGate) {
    TopoDS_Edge edge;
    auto faces=AdjacentPlanarFaces(edge,false);
    faces.second.Reverse();
    const auto result=AccuracyValidator().ValidateAdjacentFacesG1(faces.first,faces.second);
    EXPECT_TRUE(result.adjacencyChecked);
    EXPECT_FALSE(result.meetsRequirements);
    EXPECT_NEAR(result.g1ContinuityError,180.0,1e-8);
}

TEST(ValidationRegression, EmptyBooleanResultsCannotPublishACompletedSpeedupPair) {
    auto source=PlanarPatch(100.0);
    TopoDS_Face before=BRepBuilderAPI_MakeFace(source,1e-7).Face();
    TopoDS_Face after=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,100),gp_Dir(0,0,1)),-2.0,3.0,-4.0,5.0).Face();
    AccuracyValidator validator;validator.SetPerformanceIterations(1);
    const auto result=validator.ComparePerformance(before,after);
    EXPECT_FALSE(result.booleanComplete);
    EXPECT_TRUE(std::isnan(result.GetBooleanSpeedup()));
}

TEST(ValidationRegression, ConversionWithActualNeighborsAcceptsSmoothAndRejectsCreasedFaces) {
    for(const bool crease:{false,true}) {
        TopoDS_Edge shared;
        auto faces=AdjacentPlanarFaces(shared,crease);
        Handle(Geom_Surface) plane=new Geom_Plane(gp_Pnt(),gp_Dir(0,0,1));
        auto spline=GeomConvert::SurfaceToBSplineSurface(new Geom_RectangularTrimmedSurface(plane,-1.0,0.0,0.0,1.0));
        BRep_Builder builder;
        builder.UpdateFace(faces.first,spline,TopLoc_Location(),1e-7);
        for(TopExp_Explorer e(faces.first,TopAbs_EDGE);e.More();e.Next()) {
            ShapeFix_Edge fix;
            fix.FixAddPCurve(TopoDS::Edge(e.Current()),faces.first,false,1e-7);
        }
        ASSERT_TRUE(BRepCheck_Analyzer(faces.first).IsValid());
        auto result=BS2QuadCore().Convert(faces.first,{faces.second});
        EXPECT_EQ(result.success,!crease) << result.message;
        EXPECT_TRUE(result.accuracyResult.adjacencyChecked) << result.message;
        EXPECT_NEAR(result.accuracyResult.g1ContinuityError,crease?90.0:0.0,1e-7);
        if(crease) EXPECT_TRUE(result.quadricFace.IsNull());
    }
}
