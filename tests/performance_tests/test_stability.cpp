/**
 * @file test_stability.cpp
 * @brief 稳定性测试（1000个曲面）
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "BS2QuadCore.h"
#include "TestDataGenerator.h"
#include <gtest/gtest.h>
#include <iostream>

using namespace BS2Quad;

/**
 * @brief 稳定性测试套件
 */
class StabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        core = std::make_unique<BS2QuadCore>();
        generator = std::make_unique<TestDataGenerator>();
    }
    
    std::unique_ptr<BS2QuadCore> core;
    std::unique_ptr<TestDataGenerator> generator;
};

/**
 * @brief 测试：连续处理1000个曲面
 */
TEST_F(StabilityTest, Process1000Surfaces) {
    const int testCount = Performance::STABILITY_TEST_COUNT;
    
    std::cout << "开始稳定性测试：处理" << testCount << "个曲面...\n";
    
    int successCount = 0;
    int failCount = 0;
    int exceptionCount = 0;
    
    for (int i = 0; i < testCount; ++i) {
        try {
            // 生成不同类型和参数的曲面
            Handle(Geom_BSplineSurface) surface;
            
            int type = i % 5;
            switch (type) {
                case 0:  // 圆柱面
                    surface = generator->GenerateCylinderSurface(
                        2.0 + (i % 10) * 0.5,
                        5.0 + (i % 8) * 1.0,
                        8 + i % 12,
                        8 + i % 12
                    );
                    break;
                case 1:  // 球面
                    surface = generator->GenerateSphereSurface(
                        2.0 + (i % 10) * 0.5,
                        8 + i % 12,
                        8 + i % 12
                    );
                    break;
                case 2:  // 平面
                    surface = generator->GeneratePlaneSurface(
                        5.0 + (i % 10) * 1.0,
                        5.0 + (i % 8) * 1.0,
                        4 + i % 6,
                        4 + i % 6
                    );
                    break;
                case 3:  // 圆锥面
                    surface = generator->GenerateConeSurface(
                        3.0 + (i % 5) * 0.5,
                        0.5 + (i % 3) * 0.3,
                        8.0 + (i % 6) * 1.0,
                        8 + i % 10,
                        8 + i % 10
                    );
                    break;
                case 4:  // 自由曲面（负样本）
                    surface = generator->GenerateFreeFormSurface(
                        8 + i % 8,
                        8 + i % 8
                    );
                    break;
            }
            
            ConversionResult result = core->Convert(surface);
            
            if (result.success) {
                successCount++;
            } else {
                failCount++;
            }
            
            // 定期输出进度
            if ((i + 1) % 100 == 0) {
                std::cout << "已处理: " << (i + 1) << " / " << testCount 
                          << " (成功: " << successCount << ", 失败: " << failCount << ")\n";
            }
            
        } catch (const std::exception& e) {
            exceptionCount++;
            std::cerr << "异常 #" << i << ": " << e.what() << "\n";
        } catch (...) {
            exceptionCount++;
            std::cerr << "未知异常 #" << i << "\n";
        }
    }
    
    std::cout << "\n稳定性测试完成！\n";
    std::cout << "总计: " << testCount << " 个曲面\n";
    std::cout << "成功: " << successCount << " (" << (successCount * 100 / testCount) << "%)\n";
    std::cout << "失败: " << failCount << " (" << (failCount * 100 / testCount) << "%)\n";
    std::cout << "异常: " << exceptionCount << "\n";
    
    std::cout << "\n" << core->GetStatistics();
    
    // 要求：无算法崩溃、内存泄漏、结果不一致现象
    EXPECT_EQ(exceptionCount, 0);  // 不应有异常
    EXPECT_GT(successCount, testCount * 0.6);  // 至少60%成功率（考虑负样本）
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

