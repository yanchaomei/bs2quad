#include "BS2QuadCore.h"
#include "TestDataGenerator.h"
#include <gtest/gtest.h>
#include <GeomConvert.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <Geom_Plane.hxx>
#include <gp_Pln.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <gp_Trsf.hxx>
#include <limits>

using namespace BS2Quad;

TEST(ConversionContract, GeneratorProducesExactRationalCylinder) {
    TestDataGenerator generator;
    auto surface = generator.GenerateCylinderSurface(5.0, 12.0, 10, 10);
    ASSERT_FALSE(surface.IsNull());
    double u0,u1,v0,v1;
    surface->Bounds(u0,u1,v0,v1);
    for (int i=0;i<29;++i) {
        const auto p=surface->Value(u0+(u1-u0)*(i+0.37)/29, v0+(v1-v0)*0.39);
        EXPECT_NEAR(std::hypot(p.X(),p.Y()),5.0,1e-10);
    }
    EXPECT_GE(surface->NbUPoles()*surface->NbVPoles(),50);
    EXPECT_LE(surface->NbUPoles()*surface->NbVPoles(),200);
}

TEST(ConversionContract, ConvertsTranslatedRotatedPartialCylinder) {
    Handle(Geom_Surface) analytic = new Geom_CylindricalSurface(
        gp_Ax3(gp_Pnt(123,-24,15),gp_Dir(1,2,3)), 7.0);
    auto source=GeomConvert::SurfaceToBSplineSurface(
        new Geom_RectangularTrimmedSurface(analytic,0.3,1.8,-2.0,9.0));
    BS2QuadCore core;
    auto result=core.Convert(source);
    ASSERT_TRUE(result.success) << result.message;
    EXPECT_EQ(result.quadricType,QuadricType::CYLINDER);
    EXPECT_FALSE(result.quadricSurface.IsNull());
    EXPECT_LE(result.accuracyResult.maxDistance,1e-5);
}

TEST(ConversionContract, FaceConversionReturnsValidTrimmedFace) {
    Handle(Geom_Surface) plane=new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1));
    auto source=GeomConvert::SurfaceToBSplineSurface(
        new Geom_RectangularTrimmedSurface(plane,-2.0,3.0,-4.0,5.0));
    TopoDS_Face face=BRepBuilderAPI_MakeFace(source,1e-7).Face();
    auto result=BS2QuadCore().Convert(face);
    ASSERT_TRUE(result.success) << result.message;
    ASSERT_FALSE(result.quadricFace.IsNull());
    EXPECT_TRUE(BRepCheck_Analyzer(result.quadricFace).IsValid());
    EXPECT_LE(result.accuracyResult.hausdorffDistance,1e-5);
}

TEST(ConversionContract, RejectsInvalidTolerance) {
    EXPECT_THROW(BS2QuadCore(-1.0),std::invalid_argument);
    EXPECT_THROW(BS2QuadCore(std::numeric_limits<double>::quiet_NaN()),std::invalid_argument);
    BS2QuadCore core;
    EXPECT_THROW(core.SetTolerance(0),std::invalid_argument);
    EXPECT_DOUBLE_EQ(core.GetTolerance(),1e-5);
}

TEST(ConversionContract, NullBatchItemDoesNotPreventNextConversion) {
    Handle(Geom_Surface) plane=new Geom_Plane(gp_Pnt(0,0,0),gp_Dir(0,0,1));
    auto source=GeomConvert::SurfaceToBSplineSurface(
        new Geom_RectangularTrimmedSurface(plane,0.0,3.0,0.0,4.0));
    auto results=BS2QuadCore().ConvertBatch({{},source});
    ASSERT_EQ(results.size(),2u);
    EXPECT_FALSE(results[0].success);
    EXPECT_FALSE(results[0].message.empty());
    EXPECT_TRUE(results[1].success) << results[1].message;
}

TEST(ConversionContract, AllFiveExactFamiliesAndDegreeElevatedSurfacesAreAccepted) {
    TestDataGenerator generator;
    std::vector<Handle(Geom_BSplineSurface)> sources={
        generator.GeneratePlaneSurface(8,9,10,10),generator.GenerateCylinderSurface(5,12,10,10),
        generator.GenerateConeSurface(5,2,12,10,10),generator.GenerateSphereSurface(5,10,10),
        generator.GenerateTorusSurface(8,2,10,10)};
    const QuadricType types[]={QuadricType::PLANE,QuadricType::CYLINDER,QuadricType::CONE,QuadricType::SPHERE,QuadricType::TORUS};
    for(std::size_t i=0;i<sources.size();++i) {
        sources[i]->IncreaseDegree(4,4);
        gp_Trsf transform;transform.SetRotation(gp_Ax1(gp_Pnt(),gp_Dir(1,2,3)),0.47);sources[i]->Transform(transform);
        sources[i]->Translate(gp_Vec(37,-18,9));
        const auto result=BS2QuadCore().Convert(sources[i]);
        EXPECT_TRUE(result.success) << QuadricTypeToString(types[i]) << ": " << result.message;
        EXPECT_EQ(result.quadricType,types[i]);
        EXPECT_TRUE(result.accuracyResult.maxDistanceCertified);
        EXPECT_LE(result.accuracyResult.certifiedMaxDistance,1e-5);
    }
}

TEST(ConversionContract, PlacedFaceReturnsWorldSpaceSurfaceWithoutChangingInput) {
    TestDataGenerator generator;
    auto source=generator.GeneratePlaneSurface(8,9,5,5);
    TopoDS_Face face=BRepBuilderAPI_MakeFace(source,1e-7).Face();
    gp_Trsf placement;placement.SetRotation(gp_Ax1(gp_Pnt(),gp_Dir(1,2,3)),0.31);
    placement.SetTranslationPart(gp_Vec(21,-13,7));
    face.Location(TopLoc_Location(placement));
    const auto oldLocation=face.Location();
    const auto pole=source->Pole(1,1);
    const auto result=BS2QuadCore().Convert(face);
    ASSERT_TRUE(result.success) << result.message;
    auto plane=Handle(Geom_Plane)::DownCast(result.quadricSurface);
    ASSERT_FALSE(plane.IsNull());
    EXPECT_LT(plane->Pln().Distance(source->Value(2.0,3.0).Transformed(placement)),1e-8);
    EXPECT_TRUE(face.Location().IsEqual(oldLocation));
    EXPECT_DOUBLE_EQ(source->Pole(1,1).Distance(pole),0.0);
}

TEST(ConversionContract, CannotClaimItselfAsItsOwnTangentNeighbor) {
    TestDataGenerator generator;
    auto source=generator.GeneratePlaneSurface(8,9,5,5);
    TopoDS_Face face=BRepBuilderAPI_MakeFace(source,1e-7).Face();
    auto result=BS2QuadCore().Convert(face,{face});
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.accuracyResult.adjacencyChecked);
    EXPECT_TRUE(result.quadricFace.IsNull());
    EXPECT_NE(result.message.find("INVALID_NEIGHBOR"),std::string::npos);
}

TEST(ConversionContract, RejectsNonQuadricAndCorruptControlPointsWithoutThrowing) {
    TestDataGenerator generator;BS2QuadCore core;
    auto source=generator.GenerateFreeFormSurface(10,10);
    const auto freeform=core.Convert(source);
    EXPECT_FALSE(freeform.success);
    EXPECT_EQ(freeform.error,ErrorType::NON_QUADRIC);
    auto corrupt=generator.GeneratePlaneSurface(8,9,5,5);
    corrupt->SetPole(1,1,gp_Pnt(std::numeric_limits<double>::quiet_NaN(),0,0));
    const auto invalid=core.Convert(corrupt);
    EXPECT_FALSE(invalid.success);
    EXPECT_EQ(invalid.error,ErrorType::DATA_CORRUPT);
    EXPECT_NE(invalid.message.find("DATA_CORRUPT"),std::string::npos);
}

TEST(ConversionContract, PartialSphereConeAndTorusKeepValidTrimmedFaces) {
    const gp_Ax3 frame(gp_Pnt(4,-2,8),gp_Dir(1,2,3));
    const std::vector<Handle(Geom_Surface)> supports={
        new Geom_SphericalSurface(frame,5.0),new Geom_ConicalSurface(frame,0.3,4.0),new Geom_ToroidalSurface(frame,8.0,2.0)};
    for(const auto& support:supports) {
        auto source=GeomConvert::SurfaceToBSplineSurface(new Geom_RectangularTrimmedSurface(support,0.2,1.4,0.15,0.9));
        auto face=BRepBuilderAPI_MakeFace(source,1e-7).Face();
        auto result=BS2QuadCore().Convert(face);
        EXPECT_TRUE(result.success) << result.message;
        if(result.success) {
            EXPECT_FALSE(result.quadricFace.IsNull());
            EXPECT_TRUE(result.accuracyResult.boundaryChecked);
            EXPECT_TRUE(result.accuracyResult.normalChecked);
            EXPECT_LE(result.accuracyResult.hausdorffDistance,1e-5);
            EXPECT_LE(result.accuracyResult.normalDeviationDegrees,0.1);
        }
    }
}

TEST(ConversionContract, CompleteClosedCylinderSphereAndTorusReturnTrimmedFaces) {
    TestDataGenerator generator;
    const std::vector<Handle(Geom_BSplineSurface)> sources={
        generator.GenerateCylinderSurface(5,12,10,10),
        generator.GenerateSphereSurface(5,10,10),
        generator.GenerateTorusSurface(8,2,10,10)};
    for(std::size_t i=0;i<sources.size();++i) {
        auto face=BRepBuilderAPI_MakeFace(sources[i],1e-7).Face();
        auto result=BS2QuadCore().Convert(face);
        EXPECT_TRUE(result.success) << "family=" << i << " " << result.message;
        if(result.success) { EXPECT_TRUE(BRepCheck_Analyzer(result.quadricFace).IsValid()); }
    }
}
