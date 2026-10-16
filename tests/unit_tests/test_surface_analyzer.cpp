/**
 * @file test_surface_analyzer.cpp
 * @brief 曲面分析器单元测试
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "SurfaceAnalyzer.h"
#include "TestDataGenerator.h"
#include <gtest/gtest.h>

using namespace BS2Quad;

/**
 * @brief 曲面分析器测试套件
 */
class SurfaceAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        analyzer = std::make_unique<SurfaceAnalyzer>();
        generator = std::make_unique<TestDataGenerator>();
    }
    
    std::unique_ptr<SurfaceAnalyzer> analyzer;
    std::unique_ptr<TestDataGenerator> generator;
};

/**
 * @brief 测试：分析标准圆柱面
 */
TEST_F(SurfaceAnalyzerTest, AnalyzeCylinder) {
    auto cylinder = generator->GenerateCylinderSurface(5.0, 10.0, 10, 10);
    ASSERT_FALSE(cylinder.IsNull());
    
    SurfaceAnalysisResult result = analyzer->Analyze(cylinder);
    
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.type, QuadricType::CYLINDER);
    EXPECT_GT(result.confidence, 0.5);
}

/**
 * @brief 测试：分析标准球面
 */
TEST_F(SurfaceAnalyzerTest, AnalyzeSphere) {
    auto sphere = generator->GenerateSphereSurface(5.0, 10, 10);
    ASSERT_FALSE(sphere.IsNull());
    
    SurfaceAnalysisResult result = analyzer->Analyze(sphere);
    
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.type, QuadricType::SPHERE);
    EXPECT_GT(result.confidence, 0.5);
}

/**
 * @brief 测试：分析标准平面
 */
TEST_F(SurfaceAnalyzerTest, AnalyzePlane) {
    auto plane = generator->GeneratePlaneSurface(10.0, 10.0, 5, 5);
    ASSERT_FALSE(plane.IsNull());
    
    SurfaceAnalysisResult result = analyzer->Analyze(plane);
    
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.type, QuadricType::PLANE);
    EXPECT_GT(result.confidence, 0.7);
}

/**
 * @brief 测试：分析标准圆锥面
 */
TEST_F(SurfaceAnalyzerTest, AnalyzeCone) {
    auto cone = generator->GenerateConeSurface(5.0, 1.0, 10.0, 10, 10);
    ASSERT_FALSE(cone.IsNull());
    
    SurfaceAnalysisResult result = analyzer->Analyze(cone);
    
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.type, QuadricType::CONE);
}

/**
 * @brief 测试：分析自由曲面（负样本）
 */
TEST_F(SurfaceAnalyzerTest, AnalyzeFreeForm) {
    auto freeForm = generator->GenerateFreeFormSurface(10, 10);
    ASSERT_FALSE(freeForm.IsNull());
    
    SurfaceAnalysisResult result = analyzer->Analyze(freeForm);
    
    // 自由曲面应该无法识别为二次曲面
    EXPECT_EQ(result.type, QuadricType::NONE);
    EXPECT_EQ(result.error, ErrorType::NON_QUADRIC);
}

/**
 * @brief 测试：空曲面输入
 */
TEST_F(SurfaceAnalyzerTest, AnalyzeNullSurface) {
    Handle(Geom_BSplineSurface) nullSurface;
    
    SurfaceAnalysisResult result = analyzer->Analyze(nullSurface);
    
    EXPECT_FALSE(result.IsSuccess());
    EXPECT_EQ(result.error, ErrorType::DATA_CORRUPT);
}

/**
 * @brief 测试：设置采样点数
 */
TEST_F(SurfaceAnalyzerTest, SetSampleSize) {
    analyzer->SetSampleSize(20, 20);
    
    auto cylinder = generator->GenerateCylinderSurface(5.0, 10.0, 10, 10);
    SurfaceAnalysisResult result = analyzer->Analyze(cylinder);
    
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_EQ(analyzer->GetSamplePoints().size(), 20 * 20);
}

/**
 * @brief 测试：容差设置
 */
TEST_F(SurfaceAnalyzerTest, SetTolerance) {
    double newTolerance = 1e-3;
    analyzer->SetTolerance(newTolerance);
    
    EXPECT_DOUBLE_EQ(analyzer->GetTolerance(), newTolerance);
}

// 主函数

