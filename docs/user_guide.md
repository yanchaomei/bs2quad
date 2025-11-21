# BS2Quad 用户指南

## 快速入门

### 基本用法

```cpp
#include "BS2QuadCore.h"

using namespace BS2Quad;

int main() {
    // 1. 创建转换引擎
    BS2QuadCore core(1.0e-5);  // 设置容差为1e-5mm
    
    // 2. 加载NURBS曲面（从文件或生成）
    TestDataGenerator generator;
    auto surface = generator.GenerateCylinderSurface(5.0, 10.0, 10, 10);
    
    // 3. 执行转换
    ConversionResult result = core.Convert(surface);
    
    // 4. 检查结果
    if (result.success) {
        std::cout << "转换成功！\n";
        std::cout << "类型: " << QuadricTypeToString(result.quadricType) << "\n";
        std::cout << "最大误差: " << result.accuracyResult.maxDistance << " mm\n";
    } else {
        std::cout << "转换失败: " << result.message << "\n";
    }
    
    return 0;
}
```

## API参考

### BS2QuadCore类

主要转换引擎类。

#### 构造函数
```cpp
BS2QuadCore(double tolerance = 1.0e-5);
```

#### 主要方法

**Convert** - 转换单个曲面
```cpp
ConversionResult Convert(const Handle(Geom_BSplineSurface)& surface);
ConversionResult Convert(const TopoDS_Face& face);
```

**ConvertBatch** - 批量转换
```cpp
std::vector<ConversionResult> ConvertBatch(
    const std::vector<Handle(Geom_BSplineSurface)>& surfaces
);
```

**SetTolerance** - 设置容差
```cpp
void SetTolerance(double tolerance);
```

**SetEnablePerformanceTest** - 启用性能测试
```cpp
void SetEnablePerformanceTest(bool enable);
```

**GetStatistics** - 获取统计信息
```cpp
std::string GetStatistics() const;
```

### ConversionResult结构

转换结果包含以下字段：

```cpp
struct ConversionResult {
    bool success;                          // 是否成功
    QuadricType quadricType;               // 曲面类型
    Handle(Geom_Surface) quadricSurface;   // 转换后的二次曲面
    AccuracyResult accuracyResult;         // 精度验证结果
    PerformanceResult performanceResult;   // 性能对比结果
    double conversionTime;                 // 转换耗时(ms)
    std::string message;                   // 详细信息
};
```

### 曲面类型枚举

```cpp
enum class QuadricType {
    NONE,      // 无法转换
    PLANE,     // 平面
    CYLINDER,  // 圆柱面
    CONE,      // 圆锥面
    SPHERE,    // 球面
    TORUS      // 圆环面
};
```

## 高级用法

### 自定义采样

```cpp
SurfaceAnalyzer analyzer;
analyzer.SetSampleSize(50, 50);  // 设置采样点数
analyzer.SetTolerance(1e-6);     // 设置容差
```

### 性能测试

```cpp
BS2QuadCore core;
core.SetEnablePerformanceTest(true);

ConversionResult result = core.Convert(surface);

// 获取性能数据
double projSpeedup = result.performanceResult.GetProjectionSpeedup();
std::cout << "投影运算性能提升: " << (projSpeedup * 100) << "%\n";
```

### 批量处理

```cpp
// 生成测试数据
TestDataGenerator generator;
std::vector<Handle(Geom_BSplineSurface)> surfaces;

for (int i = 0; i < 100; ++i) {
    surfaces.push_back(generator.GenerateCylinderSurface(
        2.0 + i * 0.1, 10.0, 10, 10
    ));
}

// 批量转换
BS2QuadCore core;
auto results = core.ConvertBatch(surfaces);

// 统计成功率
int successCount = 0;
for (const auto& result : results) {
    if (result.success) successCount++;
}
std::cout << "成功率: " << (successCount * 100 / results.size()) << "%\n";
```

### 异常处理

```cpp
ExceptionHandler handler;

// 检测异常
ExceptionInfo info = handler.DetectExceptions(surface);

if (!info.IsSuccess()) {
    // 生成错误报告
    std::string report = handler.GenerateErrorReport(info);
    std::cout << report;
    
    // 如果可以简化，尝试简化
    if (info.canSimplify) {
        SimplificationResult simplification = handler.TrySimplify(surface);
        if (simplification.success) {
            std::cout << "简化成功: " << simplification.description << "\n";
        }
    }
}
```

## 测试数据生成

### 生成标准曲面

```cpp
TestDataGenerator generator;

// 圆柱面
auto cylinder = generator.GenerateCylinderSurface(
    5.0,   // 半径
    10.0,  // 高度
    10,    // U方向控制点数
    10     // V方向控制点数
);

// 球面
auto sphere = generator.GenerateSphereSurface(5.0, 10, 10);

// 平面
auto plane = generator.GeneratePlaneSurface(10.0, 10.0, 5, 5);

// 圆锥面
auto cone = generator.GenerateConeSurface(5.0, 1.0, 10.0, 10, 10);

// 圆环面
auto torus = generator.GenerateTorusSurface(5.0, 1.0, 10, 10);
```

### 添加噪声

```cpp
auto noisySurface = generator.AddNoiseToSurface(cylinder, 1e-5);
```

### 生成复杂自由曲面

```cpp
auto freeForm = generator.GenerateFreeFormSurface(10, 10);
```

### 批量生成并导出

```cpp
generator.SetRandomSeed(42);
auto surfaces = generator.GenerateBatchTestData(1000, "data/generated");
```

### 导出到文件

```cpp
generator.ExportToSTEP(cylinder, "cylinder.step");
generator.ExportToIGES(sphere, "sphere.iges");
```

## 常见问题

### Q: 如何提高转换精度？

A: 可以：
1. 减小容差：`core.SetTolerance(1e-6)`
2. 增加采样点：`analyzer.SetSampleSize(100, 100)`
3. 启用优化：`fitter.SetOptimize(true)`

### Q: 转换速度太慢怎么办？

A: 可以：
1. 减少采样点（牺牲一些精度）
2. 关闭性能测试：`core.SetEnablePerformanceTest(false)`
3. 使用批量转换（利用并行优化）

### Q: 如何判断曲面是否可转换？

A: 使用SurfaceAnalyzer先分析：
```cpp
SurfaceAnalyzer analyzer;
SurfaceAnalysisResult result = analyzer.Analyze(surface);

if (result.IsSuccess()) {
    std::cout << "可转换为: " << QuadricTypeToString(result.type) << "\n";
    std::cout << "置信度: " << result.confidence << "\n";
} else {
    std::cout << "无法转换: " << result.message << "\n";
}
```

### Q: 转换失败如何处理？

A: 检查ConversionResult中的message字段：
```cpp
if (!result.success) {
    std::cout << "失败原因: " << result.message << "\n";
    
    // 可能的原因：
    // - 曲面不是二次曲面
    // - 精度不满足要求
    // - 数据损坏
}
```

### Q: 如何获取转换后的二次曲面参数？

A: 使用特定的Fitter类：
```cpp
CylinderFitter fitter;
Handle(Geom_Surface) quadricSurf;
if (fitter.Fit(surface, quadricSurf)) {
    double radius = fitter.GetRadius();
    gp_Ax1 axis = fitter.GetAxis();
    std::cout << "半径: " << radius << "\n";
}
```

## 性能优化建议

### 批量处理优化

```cpp
// 不推荐：逐个转换
for (auto& surf : surfaces) {
    core.Convert(surf);  // 每次都初始化
}

// 推荐：批量转换
auto results = core.ConvertBatch(surfaces);  // 一次性处理
```

### 内存管理

```cpp
// Handle自动管理内存，无需手动释放
{
    Handle(Geom_BSplineSurface) surface = ...;
    // 离开作用域自动释放
}
```

### 多线程处理

```cpp
#include <omp.h>

std::vector<ConversionResult> results(surfaces.size());

#pragma omp parallel for
for (size_t i = 0; i < surfaces.size(); ++i) {
    BS2QuadCore core;  // 每个线程独立的core
    results[i] = core.Convert(surfaces[i]);
}
```

## 完整示例

### 示例1：船舶零件转换

```cpp
#include "BS2QuadCore.h"
#include <STEPControl_Reader.hxx>

int main() {
    // 从STEP文件读取曲面
    STEPControl_Reader reader;
    reader.ReadFile("ship_part.step");
    reader.TransferRoots();
    TopoDS_Shape shape = reader.OneShape();
    
    // 提取曲面
    TopExp_Explorer explorer(shape, TopAbs_FACE);
    
    BS2QuadCore core(1.0e-5);
    int converted = 0;
    
    while (explorer.More()) {
        TopoDS_Face face = TopoDS::Face(explorer.Current());
        
        ConversionResult result = core.Convert(face);
        
        if (result.success) {
            converted++;
            std::cout << "Face " << converted << ": "
                      << QuadricTypeToString(result.quadricType)
                      << " (误差: " << result.accuracyResult.maxDistance << "mm)\n";
        }
        
        explorer.Next();
    }
    
    std::cout << "\n转换统计:\n" << core.GetStatistics();
    
    return 0;
}
```

### 示例2：大规模测试

```cpp
#include "BS2QuadCore.h"
#include "TestDataGenerator.h"
#include <iostream>
#include <fstream>

int main() {
    TestDataGenerator generator;
    BS2QuadCore core(1.0e-5);
    
    // 生成1000个测试曲面
    std::cout << "生成测试数据...\n";
    auto surfaces = generator.GenerateBatchTestData(1000, "data/generated");
    
    // 批量转换
    std::cout << "开始转换...\n";
    auto results = core.ConvertBatch(surfaces);
    
    // 统计结果
    int success = 0;
    double totalError = 0.0;
    double totalTime = 0.0;
    
    std::ofstream csv("results.csv");
    csv << "ID,Type,Success,MaxError,Time\n";
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        
        if (result.success) {
            success++;
            totalError += result.accuracyResult.maxDistance;
            totalTime += result.conversionTime;
        }
        
        csv << i << ","
            << QuadricTypeToString(result.quadricType) << ","
            << result.success << ","
            << result.accuracyResult.maxDistance << ","
            << result.conversionTime << "\n";
    }
    
    csv.close();
    
    std::cout << "\n=== 测试报告 ===\n";
    std::cout << "总数: " << results.size() << "\n";
    std::cout << "成功: " << success << " (" << (success*100/results.size()) << "%)\n";
    std::cout << "平均误差: " << (totalError/success) << " mm\n";
    std::cout << "平均时间: " << (totalTime/success) << " ms\n";
    
    return 0;
}
```

## 编译和运行

### 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

### 运行

```bash
# 运行演示
./bs2quad_main --demo

# 生成测试数据
./bs2quad_main --generate 1000

# 运行测试
./unit_tests
./performance_tests
```

## 支持与反馈

- GitHub Issues: [项目地址]
- Email: [联系邮箱]
- 文档: docs/

