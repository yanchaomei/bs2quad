/**
 * @file test_accuracy_validator.cpp
 * @brief 精度验证器单元测试
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "AccuracyValidator.h"
#include "CylinderFitter.h"
#include "TestDataGenerator.h"
#include <gtest/gtest.h>

using namespace BS2Quad;

/**
 * @brief 精度验证器测试套件
 */
class AccuracyValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        validator = std::make_unique<AccuracyValidator>();
        generator = std::make_unique<TestDataGenerator>();
        fitter = std::make_unique<CylinderFitter>();
    }
    
    std::unique_ptr<AccuracyValidator> validator;
    std::unique_ptr<TestDataGenerator> generator;
    std::unique_ptr<CylinderFitter> fitter;
};

TEST_F(AccuracyValidatorTest, ValidateConversionAccuracy) {
    auto cylinder = generator->GenerateCylinderSurface(5.0, 10.0, 10, 10);
    
    Handle(Geom_Surface) quadricSurface;
    fitter->Fit(cylinder, quadricSurface);
    
    AccuracyResult result = validator->ValidateAccuracy(cylinder, quadricSurface);
    
    EXPECT_LE(result.maxDistance, Precision::DEFAULT_TOLERANCE);
    EXPECT_LE(result.avgDistance, Precision::DEFAULT_TOLERANCE);
    EXPECT_LE(result.rmsDistance, Precision::DEFAULT_TOLERANCE);
    EXPECT_TRUE(result.meetsRequirements);
}

TEST_F(AccuracyValidatorTest, ComparePerformance) {
    auto cylinder = generator->GenerateCylinderSurface(5.0, 10.0, 10, 10);
    
    Handle(Geom_Surface) quadricSurface;
    fitter->Fit(cylinder, quadricSurface);
    
    PerformanceResult result = validator->ComparePerformance(cylinder, quadricSurface);
    
    // 二次曲面的投影运算应该更快
    EXPECT_GT(result.GetProjectionSpeedup(), 0.0);
}

// 主函数

