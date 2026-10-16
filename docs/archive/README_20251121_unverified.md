> 历史未核验稿：下文数值未找到对应原始记录，仅保留来源，不作为本次实测依据。当前结果见上级目录的实验报告。

# BS2Quad - 样条面到二次曲面智能转换系统

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-green.svg)]()
[![OpenCASCADE](https://img.shields.io/badge/OpenCASCADE-7.7.0-orange.svg)]()

## 项目简介

BS2Quad是一套稳健、高效的样条面到二次曲面智能转换算法系统，专为国产CAD/CAM/CAE软件设计。通过精准的特征提取、兼容的模型匹配与可控的精度优化，实现从通用NURBS样条面到二次曲面（平面、圆柱面、圆锥面、球面、圆环面）的高精度转换。

### 核心特性

- **智能识别**: 自动分析NURBS曲面特征，识别可转换的二次曲面类型
- **高精度转换**: 转换后曲面与原样条面的最大误差≤1e-5mm
- **高效率**: 单曲面转换时间≤10ms，CAD操作性能提升≥50%
- **高稳定性**: 连续处理1000+曲面无崩溃、无内存泄漏
- **异常处理**: 完善的异常检测和局部简化方案

## 运行条件

### 系统要求
- 操作系统: macOS 10.15+, Linux, Windows 10+
- CPU: 支持SSE2指令集
- 内存: 至少2GB RAM
- 磁盘: 至少1GB可用空间

### 依赖库
- **OpenCASCADE 7.7.0** - CAD几何内核（必需）
- **CMake 3.15+** - 构建工具（必需）
- **C++17** 编译器 - GCC 7+, Clang 6+, MSVC 2019+（必需）
- **Google Test** - 单元测试框架（可选）
- **OpenMP** - 多线程优化（可选）
- **Doxygen** - 文档生成（可选）

详细安装说明请参考 [INSTALL.md](INSTALL.md)

## 快速开始

### 编译项目

```bash
# 克隆项目
cd /path/to/bs2quad

# 创建构建目录
mkdir -p build && cd build

# 配置（假设使用Homebrew安装的OpenCASCADE）
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_PREFIX_PATH=$(brew --prefix opencascade)

# 编译
make -j$(sysctl -n hw.ncpu)
```

### 运行演示

```bash
# 运行主程序演示
./bs2quad_main --demo

# 生成1000个测试曲面
./bs2quad_main --generate 1000

# 运行单元测试
./unit_tests

# 运行性能测试
./performance_tests
```

### 使用示例

```cpp
#include "BS2QuadCore.h"
#include "TestDataGenerator.h"

using namespace BS2Quad;

int main() {
    // 创建转换引擎
    BS2QuadCore core(1.0e-5);  // 设置容差为1e-5mm
    
    // 生成测试曲面
    TestDataGenerator generator;
    auto cylinder = generator.GenerateCylinderSurface(5.0, 10.0, 10, 10);
    
    // 执行转换
    ConversionResult result = core.Convert(cylinder);
    
    if (result.success) {
        std::cout << "转换成功！类型: " 
                  << QuadricTypeToString(result.quadricType) << "\n";
        std::cout << "最大误差: " << result.accuracyResult.maxDistance << " mm\n";
        std::cout << "转换耗时: " << result.conversionTime << " ms\n";
    }
    
    return 0;
}
```

## 运行说明

### 命令行选项

```bash
bs2quad_main [选项]

选项:
  -h, --help              显示帮助信息
  -g, --generate <n>      生成n个测试曲面
  -t, --tolerance <d>     设置误差容差（默认：1e-5mm）
  -d, --demo              运行演示程序
  -p, --perf              启用性能测试
```

### 运行测试

```bash
# 运行所有单元测试
cd build
./unit_tests

# 运行性能测试
./performance_tests

# 查看测试报告
cat Testing/Temporary/LastTest.log
```

## 测试说明

### 单元测试

项目包含完整的单元测试，覆盖所有核心模块：
- **test_surface_analyzer.cpp**: 曲面分析器测试
- **test_quadric_fitter.cpp**: 二次曲面拟合器测试
- **test_accuracy_validator.cpp**: 精度验证器测试
- **test_exception_handler.cpp**: 异常处理器测试

### 性能测试

- **test_conversion_performance.cpp**: 转换性能测试（≤10ms）
- **test_cad_operations.cpp**: CAD操作性能对比测试（≥50%提升）
- **test_stability.cpp**: 稳定性测试（1000+曲面）

### 测试数据

使用TestDataGenerator可生成多种类型的测试曲面：
- 标准圆柱面、圆锥面、球面、平面、圆环面
- 带噪声的二次曲面
- 复杂自由曲面（负样本）

## 技术架构

### 系统架构

```
BS2Quad核心系统
├── BS2QuadCore (转换引擎)
│   ├── SurfaceAnalyzer (曲面分析)
│   ├── QuadricFitter (曲面拟合)
│   │   ├── CylinderFitter (圆柱面)
│   │   ├── ConeFitter (圆锥面)
│   │   ├── SphereFitter (球面)
│   │   ├── PlaneFitter (平面)
│   │   └── TorusFitter (圆环面)
│   ├── AccuracyValidator (精度验证)
│   └── ExceptionHandler (异常处理)
└── TestDataGenerator (测试数据生成)
```

### 核心算法

1. **曲面分析**: 基于曲率分析、法向量分布、PCA主成分分析
2. **参数拟合**: 最小二乘法、SVD奇异值分解、Levenberg-Marquardt优化
3. **精度验证**: Hausdorff距离、RMS误差、G0/G1连续性检查
4. **性能优化**: 自适应采样、空间划分、多线程并行（OpenMP）

### 技术栈

- **编程语言**: C++17
- **几何内核**: OpenCASCADE 7.7.0
- **构建系统**: CMake
- **测试框架**: Google Test
- **并行计算**: OpenMP
- **文档生成**: Doxygen

## 性能指标

根据竞赛要求，系统满足以下性能指标：

| 指标 | 要求 | 实际表现 |
|------|------|----------|
| 最大几何距离误差 | ≤1e-5mm | ≤1e-5mm |
| 平均距离误差 | ≤1e-5mm | ≤5e-6mm |
| Hausdorff距离 | ≤1e-5mm | ≤1e-5mm |
| G1连续性误差 | ≤0.1° | ≤0.05° |
| 单曲面转换时间 | ≤10ms | 平均5-8ms |
| CAD操作性能提升 | ≥50% | 60-80% |
| 稳定性 | 1000+曲面无崩溃 | ✓ 通过 |

## 项目结构

```
bs2quad/
├── CMakeLists.txt          # CMake配置文件
├── README.md               # 项目说明文档
├── INSTALL.md              # 安装指南
├── LICENSE                 # Apache 2.0许可证
├── include/                # 头文件目录
│   ├── Common.h            # 公共定义
│   ├── BS2QuadCore.h       # 核心引擎
│   ├── SurfaceAnalyzer.h   # 曲面分析器
│   ├── QuadricFitter.h     # 拟合器基类
│   ├── CylinderFitter.h    # 圆柱面拟合器
│   ├── ConeFitter.h        # 圆锥面拟合器
│   ├── SphereFitter.h      # 球面拟合器
│   ├── PlaneFitter.h       # 平面拟合器
│   ├── TorusFitter.h       # 圆环面拟合器
│   ├── AccuracyValidator.h # 精度验证器
│   ├── ExceptionHandler.h  # 异常处理器
│   └── TestDataGenerator.h # 测试数据生成器
├── src/                    # 源文件目录
│   ├── BS2QuadCore.cpp
│   ├── SurfaceAnalyzer.cpp
│   ├── QuadricFitter.cpp
│   ├── CylinderFitter.cpp
│   ├── ConeFitter.cpp
│   ├── SphereFitter.cpp
│   ├── PlaneFitter.cpp
│   ├── TorusFitter.cpp
│   ├── AccuracyValidator.cpp
│   ├── ExceptionHandler.cpp
│   ├── TestDataGenerator.cpp
│   └── main.cpp
├── tests/                  # 测试目录
│   ├── unit_tests/         # 单元测试
│   │   ├── test_surface_analyzer.cpp
│   │   ├── test_quadric_fitter.cpp
│   │   ├── test_accuracy_validator.cpp
│   │   └── test_exception_handler.cpp
│   └── performance_tests/  # 性能测试
│       ├── test_conversion_performance.cpp
│       ├── test_cad_operations.cpp
│       └── test_stability.cpp
├── data/                   # 数据目录
│   └── generated/          # 生成的测试数据
└── docs/                   # 文档目录
    ├── experiment_report.md    # 实验报告
    ├── error_analysis.md       # 误差分析
    └── user_guide.md           # 用户指南
```

## 文档

- [安装指南](INSTALL.md) - 详细的安装和配置说明
- [实验报告](docs/experiment_report.md) - 算法设计和测试结果
- [误差分析](docs/error_analysis.md) - 误差来源和优化方向
- [用户指南](docs/user_guide.md) - API使用和示例代码

## 协作者

BS2Quad Team - 2025开放原子开源大赛参赛团队

## 许可证

本项目采用 Apache License 2.0 许可证 - 详见 [LICENSE](license) 文件

## 致谢

- 感谢OpenCASCADE项目提供强大的几何内核
- 感谢开放原子开源大赛提供技术交流平台
- 感谢所有为国产工业软件发展做出贡献的开发者