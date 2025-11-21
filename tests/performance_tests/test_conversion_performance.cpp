/**
 * @file test_conversion_performance.cpp
 * @brief 转换性能测试
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "BS2QuadCore.h"
#include "TestDataGenerator.h"
#include <gtest/gtest.h>
#include <chrono>
#include <iostream>

using namespace BS2Quad;

/**
 * @brief 性能测试套件
 */
class ConversionPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        core = std::make_unique<BS2QuadCore>();
        generator = std::make_unique<TestDataGenerator>();
    }
    
    std::unique_ptr<BS2QuadCore> core;
    std::unique_ptr<TestDataGenerator> generator;
};

/**
 * @brief 测试：单个圆柱面转换性能（应≤10ms）
 */
TEST_F(ConversionPerformanceTest, CylinderConversionTime) {
    auto cylinder = generator->GenerateCylinderSurface(5.0, 10.0, 50, 50);
    
    auto start = std::chrono::high_resolution_clock::now();
    ConversionResult result = core->Convert(cylinder);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "圆柱面转换耗时: " << duration.count() << " ms\n";
    
    EXPECT_TRUE(result.success);
    EXPECT_LE(duration.count(), Performance::MAX_CONVERSION_TIME_MS);
}

/**
 * @brief 测试：批量转换性能
 */
TEST_F(ConversionPerformanceTest, BatchConversionPerformance) {
    std::vector<Handle(Geom_BSplineSurface)> surfaces;
    
    // 生成100个不同的曲面
    for (int i = 0; i < 100; ++i) {
        double radius = 3.0 + (i % 10) * 0.5;
        double height = 8.0 + (i % 5) * 2.0;
        surfaces.push_back(generator->GenerateCylinderSurface(radius, height, 20, 20));
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    auto results = core->ConvertBatch(surfaces);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avgTime = static_cast<double>(duration.count()) / surfaces.size();
    
    std::cout << "批量转换100个曲面总耗时: " << duration.count() << " ms\n";
    std::cout << "平均每个曲面: " << avgTime << " ms\n";
    
    int successCount = 0;
    for (const auto& result : results) {
        if (result.success) successCount++;
    }
    
    std::cout << "成功率: " << (successCount * 100 / surfaces.size()) << "%\n";
    
    EXPECT_GT(successCount, surfaces.size() * 0.8);  // 至少80%成功率
    EXPECT_LE(avgTime, Performance::MAX_CONVERSION_TIME_MS);
}

/**
 * @brief 测试：CAD操作性能提升（应≥50%）
 */
TEST_F(ConversionPerformanceTest, CADOperationSpeedup) {
    core->SetEnablePerformanceTest(true);
    
    auto cylinder = generator->GenerateCylinderSurface(5.0, 10.0, 30, 30);
    ConversionResult result = core->Convert(cylinder);
    
    ASSERT_TRUE(result.success);
    
    double projSpeedup = result.performanceResult.GetProjectionSpeedup();
    double intSpeedup = result.performanceResult.GetIntersectionSpeedup();
    
    std::cout << "投影运算性能提升: " << (projSpeedup * 100) << "%\n";
    std::cout << "求交运算性能提升: " << (intSpeedup * 100) << "%\n";
    
    // 根据赛题要求，性能提升应≥50%
    EXPECT_GE(projSpeedup, Performance::MIN_PERFORMANCE_GAIN);
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

