# 安装与远程复现

## 已验证环境

本次构建和测试在 `SKV_1` 的专属目录 `/home/kvgroup/chaomei/g_bs2quad_20261016` 下进行，使用 Ubuntu 22.04 容器、GCC 11、Google Test 1.11.0 和真实 OCCT 7.7.0。依赖安装在任务目录内，未替换服务器的系统几何库。具体编译器、CMake、CPU 和动态库路径以交付证据中的环境记录为准。

代码编辑和 Git 管理在本地进行，构建、测试及基准在远程服务器进行。SGK 1.7 SDK 尚不可用；当前 `BS2QUAD_BACKEND=SGK` 会报错，不能据此声明完成 SGK 兼容。

## 安装第三方依赖

Ubuntu 22.04 容器可安装：

```bash
apt-get update
apt-get install -y --no-install-recommends \
  build-essential cmake curl ca-certificates libgtest-dev \
  libocct-data-exchange-dev libocct-modeling-algorithms-dev valgrind
```

发行版中的 OCCT 7.5 仅提供构建所需的系统依赖，项目实际链接 7.7.0。随后在远程任务目录构建固定版本：

```bash
bash scripts/bootstrap_occt.sh /absolute/task/dependencies 8
export OpenCASCADE_DIR=/absolute/task/dependencies/occt770/lib/cmake/opencascade
export LD_LIBRARY_PATH=/absolute/task/dependencies/occt770/lib
```

脚本校验官方 `V7_7_0` 源码包 SHA-256，不会修改系统库。服务器网络较慢时，可先下载并同步官方源码包，再执行：

```bash
OCCT_ARCHIVE=/absolute/path/occt-7.7.0.tar.gz \
  bash scripts/bootstrap_occt.sh /absolute/task/dependencies 8
```

## 构建和检查

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure --no-tests=error
```

默认测试构建要求 Google Test。只构建库时可以使用 `-DBUILD_TESTING=OFF`，但不能将这种构建当作测试通过；完整验证脚本会强制重新启用测试。

区间证书依赖正确的浮点求值顺序，库已设置 `-fno-fast-math -ffp-contract=off`；MSVC 分支使用 `/fp:strict`。不要通过额外构建参数覆盖这些约束。当前执行证据来自 Linux x86-64；其他平台需重新验证。

## 完整证据运行

```bash
bash scripts/verify.sh build evidence/a-new-run
```

脚本拒绝覆盖非空证据目录，逐项记录退出码。它分别检查正确性、1000 例判定时延和三类 CAD 性能指标。CAD 性能尚未全项达标时，完整验证返回非零，原始测量仍会保存。

内存与未定义行为检查：

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_TESTING=ON -DBS2QUAD_SANITIZERS=ON
cmake --build build-asan -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-asan --output-on-failure --no-tests=error
```

ASan/UBSan 直接检查本项目已插桩代码；系统及几何内核动态库未在本次重新插桩，不能将这种检查声称为所有第三方代码均已完成内存审计。交付报告另记录 Valgrind 的实际运行范围和结果。

## Docker 配方

仓库提供固定 OCCT 校验和的 Dockerfile。远程服务器上可执行：

```bash
docker build -t bs2quad:1.1 .
docker run --rm bs2quad:1.1 --demo
```

这是交付的复现配方。完整镜像是否重新构建，以实验报告的实际回执为准；手工容器中成功的依赖构建与测试不自动等价于 Dockerfile 全流程已执行。
