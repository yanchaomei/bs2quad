#include "PlaneFitter.h"
#include "SphereFitter.h"
#include "CylinderFitter.h"
#include "ConeFitter.h"
#include "TorusFitter.h"
#include <gtest/gtest.h>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <array>
#include <limits>
using namespace BS2Quad;
namespace {
const gp_Pnt Center(13,-7,5);
const gp_Ax3 Frame(Center,gp_Dir(1,2,3));
gp_Pnt World(double x,double y,double z) {
    return Center.Translated(gp_Vec(Frame.XDirection())*x+gp_Vec(Frame.YDirection())*y+gp_Vec(Frame.Direction())*z);
}
std::vector<gp_Pnt> Points(int type,bool partial) {
    std::vector<gp_Pnt> out;
    for(int i=0;i<11;++i)for(int j=0;j<9;++j) {
        const double u=partial?.3+1.6*i/10.0:2*M_PI*i/11.0;
        const double v=partial?-.6+1.2*j/8.0:-1.2+2.4*j/8.0;
        if(type==0)out.push_back(World(-3+6.0*i/10,-2+4.0*j/8,0));
        if(type==1)out.push_back(World(5*cos(v)*cos(u),5*cos(v)*sin(u),5*sin(v)));
        if(type==2)out.push_back(World(5*cos(u),5*sin(u),partial?-.03+.06*j/8.0:-3+6.0*j/8));
        if(type==3){const double h=3+5.0*j/8;out.push_back(World(h*tan(.4)*cos(u),h*tan(.4)*sin(u),h));}
        if(type==4){const double w=partial?-.8+1.7*j/8:2*M_PI*j/9.0;out.push_back(World((8+2*cos(w))*cos(u),(8+2*cos(w))*sin(u),2*sin(w)));}
    }
    return out;
}
void CheckPointResiduals(const std::vector<gp_Pnt>& points,const Handle(Geom_Surface)& surface) {
    ASSERT_FALSE(surface.IsNull());
    for(const auto& p:points){GeomAPI_ProjectPointOnSurf projection(p,surface);ASSERT_GT(projection.NbPoints(),0);EXPECT_LE(projection.LowerDistance(),1e-5);}
}
}
TEST(PointFitting, PlaneUsesTheLeastVarianceDirectionAndAcceptedGetter) {
    PlaneFitter fitter;Handle(Geom_Surface) fitted;auto points=Points(0,false);
    ASSERT_TRUE(fitter.FitFromPoints(points,fitted));CheckPointResiduals(points,fitted);
    EXPECT_NEAR(std::abs(fitter.GetNormal().Dot(Frame.Direction())),1.0,1e-9);
    EXPECT_LE(fitter.GetPlane().Distance(Center),1e-5);
}
TEST(PointFitting, SphereRecoversTranslatedCenterFromPartialPatch) {
    SphereFitter fitter;Handle(Geom_Surface) fitted;auto points=Points(1,true);
    ASSERT_TRUE(fitter.FitFromPoints(points,fitted));CheckPointResiduals(points,fitted);
    EXPECT_LE(fitter.GetCenter().Distance(Center),1e-5);EXPECT_NEAR(fitter.GetRadius(),5,1e-5);
}
TEST(PointFitting, CylinderRecoversThinPartialPatchAxisRatherThanDominantPCAAxis) {
    CylinderFitter fitter;Handle(Geom_Surface) fitted;auto points=Points(2,true);
    ASSERT_TRUE(fitter.FitFromPoints(points,fitted));CheckPointResiduals(points,fitted);
    EXPECT_NEAR(std::abs(fitter.GetAxis().Direction().Dot(Frame.Direction())),1.0,1e-8);
    EXPECT_NEAR(fitter.GetRadius(),5,1e-5);
}
TEST(PointFitting, ConeRecoversApexInsteadOfUsingPointCentroid) {
    ConeFitter fitter;Handle(Geom_Surface) fitted;auto points=Points(3,true);
    ASSERT_TRUE(fitter.FitFromPoints(points,fitted));CheckPointResiduals(points,fitted);
    EXPECT_LE(fitter.GetApex().Distance(Center),1e-5);EXPECT_NEAR(fitter.GetSemiAngle(),.4,1e-5);
}
TEST(PointFitting, TorusRecoversMajorAndMinorRadiiFromRotatedFullSamples) {
    TorusFitter fitter;Handle(Geom_Surface) fitted;auto points=Points(4,false);
    ASSERT_TRUE(fitter.FitFromPoints(points,fitted));CheckPointResiduals(points,fitted);
    EXPECT_LE(fitter.GetTorus().Location().Distance(Center),1e-5);
    EXPECT_NEAR(fitter.GetMajorRadius(),8,1e-5);EXPECT_NEAR(fitter.GetMinorRadius(),2,1e-5);
}
TEST(PointFitting, EveryFamilyRejectsNonfiniteRepeatedAndCollinearInputsWithoutLeakingOutput) {
    PlaneFitter plane;SphereFitter sphere;CylinderFitter cylinder;ConeFitter cone;TorusFitter torus;
    std::array<QuadricFitter*,5> fitters={{&plane,&sphere,&cylinder,&cone,&torus}};
    std::vector<gp_Pnt> line;for(int i=0;i<20;++i)line.emplace_back(i,2*i,3*i);
    std::vector<std::vector<gp_Pnt>> bad={{},std::vector<gp_Pnt>(20,Center),line};
    auto nonfinite=Points(1,false);nonfinite[0].SetX(std::numeric_limits<double>::quiet_NaN());bad.push_back(nonfinite);
    for(auto* fitter:fitters)for(const auto& points:bad){Handle(Geom_Surface) output=new Geom_Plane(gp_Pln());EXPECT_FALSE(fitter->FitFromPoints(points,output));EXPECT_TRUE(output.IsNull());EXPECT_TRUE(std::isinf(fitter->GetFittingError()));}
}
TEST(PointFitting, MaximumResidualGateRejectsOneOutlierEvenWhenRMSWouldFitTolerance) {
    PlaneFitter fitter(.001);auto points=Points(0,false);
    points.push_back(World(0,0,.005));Handle(Geom_Surface) output;
    EXPECT_FALSE(fitter.FitFromPoints(points,output));EXPECT_TRUE(output.IsNull());
}

TEST(PointFitting, PartialTorusAndFullCylinderUseTheSameFinitePointContract) {
    TorusFitter torus;CylinderFitter cylinder;Handle(Geom_Surface) fitted;
    const auto partial=Points(4,true);
    ASSERT_TRUE(torus.FitFromPoints(partial,fitted));CheckPointResiduals(partial,fitted);
    EXPECT_NEAR(torus.GetMajorRadius(),8,1e-5);EXPECT_NEAR(torus.GetMinorRadius(),2,1e-5);
    EXPECT_LE(torus.GetTorus().Location().Distance(Center),1e-5);
    const auto full=Points(2,false);
    ASSERT_TRUE(cylinder.FitFromPoints(full,fitted));CheckPointResiduals(full,fitted);
    EXPECT_NEAR(cylinder.GetRadius(),5,1e-5);
}
TEST(PointFitting, EveryFamilyUsesMaximumResidualRatherThanAveragingAnOutlierAway) {
    PlaneFitter plane(.02);SphereFitter sphere(.02);CylinderFitter cylinder(.02);ConeFitter cone(.02);TorusFitter torus(.02);
    std::array<QuadricFitter*,5> fitters={{&plane,&sphere,&cylinder,&cone,&torus}};
    for(int type=0;type<5;++type){SCOPED_TRACE(type);auto points=Points(type,false);
        const gp_Vec offset=gp_Vec(type<=1?Frame.Direction():Frame.XDirection())*.1;
        points.push_back(points.front().Translated(offset));Handle(Geom_Surface) fitted;
        EXPECT_FALSE(fitters[type]->FitFromPoints(points,fitted));EXPECT_TRUE(fitted.IsNull());
        EXPECT_TRUE(std::isinf(fitters[type]->GetFittingError()));
    }
}
TEST(PointFitting, DisablingOptionalRefinementStillFitsWellConditionedExactFullFixtures) {
    PlaneFitter plane;SphereFitter sphere;CylinderFitter cylinder;ConeFitter cone;TorusFitter torus;
    std::array<QuadricFitter*,5> fitters={{&plane,&sphere,&cylinder,&cone,&torus}};
    for(int type=0;type<5;++type){SCOPED_TRACE(type);fitters[type]->SetOptimize(false);Handle(Geom_Surface) fitted;
        const auto points=Points(type,false);ASSERT_TRUE(fitters[type]->FitFromPoints(points,fitted));CheckPointResiduals(points,fitted);
    }
}
TEST(PointFitting, WorldCoordinateRoundingMustNotBypassTheResidualGate) {
    // The exact circumcenter has z=1e12+5/6, which cannot be stored at this
    // coordinate magnitude within the requested absolute tolerance.
    const double a=1e12;
    const std::vector<gp_Pnt> points={{a,a,a},{a+3,a,a},{a,a+3,a},{a+1,a+1,a+3}};
    SphereFitter sphere(1e-6);Handle(Geom_Surface) fitted;
    EXPECT_FALSE(sphere.FitFromPoints(points,fitted));EXPECT_TRUE(fitted.IsNull());
    EXPECT_TRUE(std::isinf(sphere.GetFittingError()));
}
