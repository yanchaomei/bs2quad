# BS2Quad 安装指南

## OpenCASCADE 7.7.0 安装

### macOS 安装步骤

#### 方法1：使用Homebrew（推荐）

```bash
# 安装Homebrew（如果未安装）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装OpenCASCADE
brew install opencascade
```

#### 方法2：从源码编译

```bash
# 下载OpenCASCADE 7.7.0
wget https://git.dev.opencascade.org/gitweb/?p=occt.git;a=snapshot;h=refs/tags/V7_7_0;sf=tgz
tar -xzf occt-V7_7_0.tar.gz
cd occt-V7_7_0

# 创建构建目录
mkdir build && cd build

# 配置CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local/opencascade-7.7.0 \
    -DUSE_TBB=OFF \
    -DUSE_VTK=OFF

# 编译和安装
make -j$(sysctl -n hw.ncpu)
sudo make install
```

## 依赖项安装

### 必需依赖

```bash
# 安装CMake（3.15或更高版本）
brew install cmake

# 安装Google Test（用于单元测试）
brew install googletest
```

### 可选依赖

```bash
# 安装OpenMP（用于多线程优化）
brew install libomp

# 安装Doxygen（用于生成文档）
brew install doxygen
```

## 编译BS2Quad项目

```bash
# 克隆或进入项目目录
cd /path/to/bs2quad

# 创建构建目录
mkdir -p build
cd build

# 配置项目
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/usr/local/opencascade-7.7.0

# 如果使用Homebrew安装的OpenCASCADE
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$(brew --prefix opencascade)

# 编译
make -j$(sysctl -n hw.ncpu)

# 运行测试
make test

# 安装（可选）
sudo make install
```

## 验证安装

```bash
# 运行主程序
./bs2quad_main

# 运行单元测试
./unit_tests

# 运行性能测试
./performance_tests
```

## 环境变量配置

将以下内容添加到 `~/.zshrc` 或 `~/.bash_profile`：

```bash
# OpenCASCADE环境变量
export CASROOT=/usr/local/opencascade-7.7.0
export LD_LIBRARY_PATH=$CASROOT/lib:$LD_LIBRARY_PATH
export PATH=$CASROOT/bin:$PATH

# 如果使用Homebrew安装
export CASROOT=$(brew --prefix opencascade)
export DYLD_LIBRARY_PATH=$CASROOT/lib:$DYLD_LIBRARY_PATH
```

## 常见问题

### 找不到OpenCASCADE

如果CMake找不到OpenCASCADE，请明确指定路径：

```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/opencascade
```

### 链接错误

确保OpenCASCADE的库路径在系统搜索路径中：

```bash
export DYLD_LIBRARY_PATH=/usr/local/opencascade-7.7.0/lib:$DYLD_LIBRARY_PATH
```

### Google Test未找到

手动指定GTest路径：

```bash
cmake .. -DGTest_DIR=/path/to/gtest/lib/cmake/GTest
```

## 开发工具推荐

- IDE: Visual Studio Code, CLion, Xcode
- 调试器: lldb
- 性能分析: Instruments (macOS自带)
- 内存检查: valgrind (需要额外安装)

## 项目结构

```
bs2quad/
├── build/              # 构建目录（生成）
├── include/            # 头文件
├── src/                # 源文件
├── tests/              # 测试文件
│   ├── unit_tests/     # 单元测试
│   └── performance_tests/  # 性能测试
├── data/               # 测试数据
│   └── generated/      # 生成的测试数据
├── docs/               # 文档
├── CMakeLists.txt      # CMake配置
└── README.md           # 项目说明
```

