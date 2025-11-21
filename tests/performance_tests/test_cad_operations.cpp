/**
 * @file test_cad_operations.cpp
 * @brief CAD操作性能对比测试
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "AccuracyValidator.h"
#include "CylinderFitter.h"
#include "TestDataGenerator.h"
#include <gtest/gtest.h>
#include <iostream>

using namespace BS2Quad;

/**
 * @brief CAD操作性能测试套件
 */
class CADOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        validator = std::make_unique<AccuracyValidator>();
        fitter = std::make_unique<CylinderFitter>();
        generator = std::make_unique<TestDataGenerator>();
        
        validator->SetPerformanceIterations(50);
    }
    
    std::unique_ptr<AccuracyValidator> validator;
    std::unique_ptr<CylinderFitter> fitter;
    std::unique_ptr<TestDataGenerator> generator;
};

/**
 * @brief 测试：投影运算性能对比
 */
TEST_F(CADOperationsTest, ProjectionPerformance) {
    auto nurbsSurf = generator->GenerateCylinderSurface(5.0, 10.0, 50, 50);
    
    Handle(Geom_Surface) quadricSurf;
    fitter->Fit(nurbsSurf, quadricSurf);
    
    PerformanceResult result = validator->ComparePerformance(nurbsSurf, quadricSurf);
    
    std::cout << "NURBS投影时间: " << result.nurbsProjectionTime << " ms\n";
    std::cout << "二次曲面投影时间: " << result.quadricProjectionTime << " ms\n";
    std::cout << "性能提升: " << (result.GetProjectionSpeedup() * 100) << "%\n";
    
    EXPECT_LT(result.quadricProjectionTime, result.nurbsProjectionTime);
}

/**
 * @brief 测试：求交运算性能对比
 */
TEST_F(CADOperationsTest, IntersectionPerformance) {
    auto nurbsSurf = generator->GenerateCylinderSurface(5.0, 10.0, 50, 50);
    
    Handle(Geom_Surface) quadricSurf;
    fitter->Fit(nurbsSurf, quadricSurf);
    
    PerformanceResult result = validator->ComparePerformance(nurbsSurf, quadricSurf);
    
    std::cout << "NURBS求交时间: " << result.nurbsIntersectionTime << " ms\n";
    std::cout << "二次曲面求交时间: " << result.quadricIntersectionTime << " ms\n";
    std::cout << "性能提升: " << (result.GetIntersectionSpeedup() * 100) << "%\n";
    
    EXPECT_LT(result.quadricIntersectionTime, result.nurbsIntersectionTime);
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

