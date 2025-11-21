/**
 * @file test_quadric_fitter.cpp
 * @brief 二次曲面拟合器单元测试
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "CylinderFitter.h"
#include "SphereFitter.h"
#include "PlaneFitter.h"
#include "ConeFitter.h"
#include "TestDataGenerator.h"
#include <gtest/gtest.h>

using namespace BS2Quad;

/**
 * @brief 圆柱面拟合器测试套件
 */
class CylinderFitterTest : public ::testing::Test {
protected:
    void SetUp() override {
        fitter = std::make_unique<CylinderFitter>();
        generator = std::make_unique<TestDataGenerator>();
    }
    
    std::unique_ptr<CylinderFitter> fitter;
    std::unique_ptr<TestDataGenerator> generator;
};

TEST_F(CylinderFitterTest, FitStandardCylinder) {
    auto cylinder = generator->GenerateCylinderSurface(5.0, 10.0, 10, 10);
    ASSERT_FALSE(cylinder.IsNull());
    
    Handle(Geom_Surface) quadricSurface;
    bool success = fitter->Fit(cylinder, quadricSurface);
    
    EXPECT_TRUE(success);
    EXPECT_FALSE(quadricSurface.IsNull());
    EXPECT_LE(fitter->GetFittingError(), Precision::DEFAULT_TOLERANCE);
}

TEST_F(CylinderFitterTest, CheckCylinderParameters) {
    auto cylinder = generator->GenerateCylinderSurface(5.0, 10.0, 10, 10);
    
    Handle(Geom_Surface) quadricSurface;
    fitter->Fit(cylinder, quadricSurface);
    
    double radius = fitter->GetRadius();
    EXPECT_NEAR(radius, 5.0, 0.5);  // 允许一定误差
}

/**
 * @brief 球面拟合器测试套件
 */
class SphereFitterTest : public ::testing::Test {
protected:
    void SetUp() override {
        fitter = std::make_unique<SphereFitter>();
        generator = std::make_unique<TestDataGenerator>();
    }
    
    std::unique_ptr<SphereFitter> fitter;
    std::unique_ptr<TestDataGenerator> generator;
};

TEST_F(SphereFitterTest, FitStandardSphere) {
    auto sphere = generator->GenerateSphereSurface(5.0, 10, 10);
    ASSERT_FALSE(sphere.IsNull());
    
    Handle(Geom_Surface) quadricSurface;
    bool success = fitter->Fit(sphere, quadricSurface);
    
    EXPECT_TRUE(success);
    EXPECT_FALSE(quadricSurface.IsNull());
    EXPECT_LE(fitter->GetFittingError(), Precision::DEFAULT_TOLERANCE);
}

TEST_F(SphereFitterTest, CheckSphereParameters) {
    auto sphere = generator->GenerateSphereSurface(5.0, 10, 10);
    
    Handle(Geom_Surface) quadricSurface;
    fitter->Fit(sphere, quadricSurface);
    
    double radius = fitter->GetRadius();
    EXPECT_NEAR(radius, 5.0, 0.5);
}

/**
 * @brief 平面拟合器测试套件
 */
class PlaneFitterTest : public ::testing::Test {
protected:
    void SetUp() override {
        fitter = std::make_unique<PlaneFitter>();
        generator = std::make_unique<TestDataGenerator>();
    }
    
    std::unique_ptr<PlaneFitter> fitter;
    std::unique_ptr<TestDataGenerator> generator;
};

TEST_F(PlaneFitterTest, FitStandardPlane) {
    auto plane = generator->GeneratePlaneSurface(10.0, 10.0, 5, 5);
    ASSERT_FALSE(plane.IsNull());
    
    Handle(Geom_Surface) quadricSurface;
    bool success = fitter->Fit(plane, quadricSurface);
    
    EXPECT_TRUE(success);
    EXPECT_FALSE(quadricSurface.IsNull());
    EXPECT_LE(fitter->GetFittingError(), Precision::DEFAULT_TOLERANCE);
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

