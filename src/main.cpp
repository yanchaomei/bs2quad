/**
 * @file main.cpp
 * @brief BS2Quad主程序
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "BS2QuadCore.h"
#include "TestDataGenerator.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

using namespace BS2Quad;

/**
 * @brief 打印欢迎信息
 */
void PrintWelcome() {
    std::cout << "================================================\n";
    std::cout << "    BS2Quad - 样条面到二次曲面智能转换系统   \n";
    std::cout << "    版本: " << BS2QuadCore::GetVersion() << "\n";
    std::cout << "================================================\n\n";
}

/**
 * @brief 打印使用帮助
 */
void PrintUsage() {
    std::cout << "用法:\n";
    std::cout << "  bs2quad_main [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  -h, --help            显示帮助信息\n";
    std::cout << "  -g, --generate <n>    生成n个测试曲面\n";
    std::cout << "  -t, --tolerance <d>   设置误差容差（默认：1e-5mm）\n";
    std::cout << "  -d, --demo            运行演示程序\n";
    std::cout << "  -p, --perf            启用性能测试\n";
    std::cout << "\n";
}

/**
 * @brief 打印转换结果
 */
void PrintConversionResult(const ConversionResult& result, int index = 0) {
    std::cout << "\n--- 转换结果";
    if (index > 0) {
        std::cout << " #" << index;
    }
    std::cout << " ---\n";
    
    std::cout << "成功: " << (result.success ? "是" : "否") << "\n";
    std::cout << "类型: " << QuadricTypeToString(result.quadricType) << "\n";
    std::cout << "耗时: " << std::fixed << std::setprecision(3) 
              << result.conversionTime << " ms\n";
    
    if (result.success) {
        const auto& acc = result.accuracyResult;
        std::cout << "\n精度信息:\n";
        std::cout << "  最大距离误差: " << std::scientific << std::setprecision(3)
                  << acc.maxDistance << " mm\n";
        std::cout << "  平均距离误差: " << acc.avgDistance << " mm\n";
        std::cout << "  RMS距离误差: " << acc.rmsDistance << " mm\n";
        std::cout << "  满足精度要求: " << (acc.meetsRequirements ? "是" : "否") << "\n";
        
        if (result.performanceResult.nurbsProjectionTime > 0) {
            const auto& perf = result.performanceResult;
            std::cout << "\n性能信息:\n";
            std::cout << "  投影运算加速: " 
                      << std::fixed << std::setprecision(1)
                      << perf.GetProjectionSpeedup() * 100 << "%\n";
            std::cout << "  求交运算加速: " 
                      << perf.GetIntersectionSpeedup() * 100 << "%\n";
        }
    }
    
    std::cout << "\n信息: " << result.message << "\n";
    std::cout << "-------------------\n";
}

/**
 * @brief 运行演示程序
 */
void RunDemo(double tolerance, bool enablePerf) {
    std::cout << "运行演示程序...\n\n";
    
    // 创建转换引擎
    BS2QuadCore core(tolerance);
    core.SetEnablePerformanceTest(enablePerf);
    
    // 创建测试数据生成器
    TestDataGenerator generator;
    
    // 生成各种类型的测试曲面
    std::cout << "生成测试曲面...\n";
    
    std::vector<Handle(Geom_BSplineSurface)> testSurfaces;
    
    // 1. 圆柱面
    testSurfaces.push_back(generator.GenerateCylinderSurface(5.0, 10.0, 10, 10));
    std::cout << "✓ 生成圆柱面\n";
    
    // 2. 球面
    testSurfaces.push_back(generator.GenerateSphereSurface(5.0, 10, 10));
    std::cout << "✓ 生成球面\n";
    
    // 3. 平面
    testSurfaces.push_back(generator.GeneratePlaneSurface(10.0, 10.0, 5, 5));
    std::cout << "✓ 生成平面\n";
    
    // 4. 圆锥面
    testSurfaces.push_back(generator.GenerateConeSurface(5.0, 1.0, 10.0, 10, 10));
    std::cout << "✓ 生成圆锥面\n";
    
    // 5. 带噪声的圆柱面
    auto noisyCylinder = generator.GenerateCylinderSurface(5.0, 10.0, 10, 10);
    noisyCylinder = generator.AddNoiseToSurface(noisyCylinder, 1e-4);
    testSurfaces.push_back(noisyCylinder);
    std::cout << "✓ 生成带噪声的圆柱面\n";
    
    std::cout << "\n开始转换测试...\n";
    
    // 转换所有曲面
    std::vector<std::string> surfaceNames = {
        "标准圆柱面", "标准球面", "标准平面", "标准圆锥面", "带噪声圆柱面"
    };
    
    int successCount = 0;
    for (size_t i = 0; i < testSurfaces.size(); ++i) {
        std::cout << "\n[" << (i+1) << "/" << testSurfaces.size() << "] "
                  << "转换 " << surfaceNames[i] << "...\n";
        
        ConversionResult result = core.Convert(testSurfaces[i]);
        PrintConversionResult(result);
        
        if (result.success) {
            successCount++;
        }
    }
    
    std::cout << "\n" << core.GetStatistics();
    
    std::cout << "\n演示完成! 成功率: " 
              << (successCount * 100 / testSurfaces.size()) << "%\n";
}

/**
 * @brief 生成批量测试数据
 */
void GenerateTestData(int count) {
    std::cout << "生成 " << count << " 个测试曲面...\n";
    
    TestDataGenerator generator;
    generator.SetRandomSeed(42);
    
    std::string outputDir = "data/generated";
    std::vector<Handle(Geom_BSplineSurface)> surfaces = 
        generator.GenerateBatchTestData(count, outputDir);
    
    std::cout << "成功生成 " << surfaces.size() << " 个测试曲面\n";
    std::cout << "输出目录: " << outputDir << "\n";
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
    PrintWelcome();
    
    // 默认参数
    double tolerance = Precision::DEFAULT_TOLERANCE;
    bool runDemo = false;
    bool enablePerf = false;
    int generateCount = 0;
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            PrintUsage();
            return 0;
        } else if (arg == "-d" || arg == "--demo") {
            runDemo = true;
        } else if (arg == "-p" || arg == "--perf") {
            enablePerf = true;
        } else if (arg == "-t" || arg == "--tolerance") {
            if (i + 1 < argc) {
                tolerance = std::stod(argv[++i]);
            }
        } else if (arg == "-g" || arg == "--generate") {
            if (i + 1 < argc) {
                generateCount = std::stoi(argv[++i]);
            }
        }
    }
    
    // 执行相应操作
    if (generateCount > 0) {
        GenerateTestData(generateCount);
    } else if (runDemo) {
        RunDemo(tolerance, enablePerf);
    } else {
        // 默认运行简单演示
        std::cout << "使用 -h 或 --help 查看帮助信息\n";
        std::cout << "使用 -d 或 --demo 运行演示程序\n\n";
        
        RunDemo(tolerance, false);
    }
    
    return 0;
}

