# BS2Quad：带全域距离上界的 NURBS 解析面恢复

BS2Quad 将可等效的 NURBS 支撑面恢复为平面、圆柱面、圆锥面、球面和圆环面。候选由真实几何内核生成，接受与否由独立误差检查决定；验证失败时返回明确错误，并保留原输入。

**当前实测后端为 OpenCASCADE 7.7.0，SGK 1.7 接入尚未完成。** 本项目用于中船海舟赛题的算法实现与验证；兼容性、几何精度、判定时间和 CAD 操作加速分别记录，不能由单元测试通过推断全部赛事指标达标。圆环面按题目要求覆盖，其一般隐式方程为四次。

## 本轮实测（2026-10-16）

64 项单元测试和 5 个 Release CTest 组通过。1000 例合成支撑面（实际 50-200 控制点）中，834 个正例正确接受、166 个反例正确拒绝，重复不一致为 0；判定平均 2.041 ms、最高 8.247 ms。最大已接受单向完整域距离界为 1.795e-6 mm。

CAD 的 15 组结果检查有效，其中 9 组达到耗时降低 50%，仍有 6 组未达标。核心转换 ASan/UBSan 和 Valgrind 检查通过；STEP 导出退出时的 2964-byte LSAN 报告在纯 OCCT 对照复现，完整出口无泄漏要求仍未通过。详细证据和范围见 [实验报告](docs/experiment_report.md)、[证据索引](evidence/20261016/README.md)。

## 已实现的路径

- **候选恢复**：使用 OCCT 的 `GeomConvert_SurfToAnaSurf`，支持部分曲面、位置变换、升阶、插结和五类解析面；内核返回的 gap 仅作诊断。
- **全域支撑面距离上界**：对原始有理样条的齐次控制网做向外舍入区间插结，以 Bernstein 系数包围解析隐式残差，推导完整参数域的单向距离上界。支持周期和非夹持基；超出支持范围时拒绝发证。
- **裁剪面重建**：深复制输入面，保留环和边，重建新支撑面上的参数曲线；处理周期接缝和可映射的球面/圆锥极点退化边。通过有效性、面积、环/边数量、双向裁剪区域、边界及法向检查后再发布结果。
- **邻面验收**：调用方提供要求 G1 连续的真实相邻面，沿原共享边检查转换面与邻面的有向法向夹角，阈值为 0.1°。原面法向保持与相邻面 G1 是两个独立检查。
- **工程与证据**：C++17、Google Test、严格命令行校验、单面 STEP/BRep 转换、1000 例混合数据连续处理、重复一致性、受控 CAD 对比，以及原始输入和日志保存。

连续支撑面距离上界与采样检查使用不同字段。`certifiedMaxDistance` 是完整原始参数域到解析支撑面的上界；`maxDistance`、`avgDistance` 是诊断采样统计。边界、裁剪覆盖、法向和 G1 当前采用有限采样与几何内核检查，**不宣称连续 Hausdorff 或连续 G1 已得到全域数学证明**。实际结果与限制见 [实验报告](docs/experiment_report.md) 和 [误差分析](docs/error_analysis.md)。

## 构建与运行

实测环境为远程 Linux x86-64、Ubuntu 22.04 容器、GCC 11、OCCT 7.7.0。默认测试构建要求 Google Test。MSVC 配置提供严格浮点选项，但本次未完成 Windows/SGK 实机验证。

已有 OCCT 7.7.0 时，在构建服务器执行：

```bash
export OpenCASCADE_DIR=/absolute/path/occt770/lib/cmake/opencascade
export LD_LIBRARY_PATH=/absolute/path/occt770/lib
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure --no-tests=error
./build/bs2quad_main --demo
```

没有对应依赖时，可用 [依赖构建脚本](scripts/bootstrap_occt.sh) 在任务目录内构建官方固定版本，源码包 SHA-256 已锁定；详细步骤和 Docker 配方见 [安装说明](INSTALL.md)。所有构建、测试和基准按项目规范在远程服务器执行，本地用于编辑和版本管理。

### 转换一个裁剪面

```bash
./build/bs2quad_main \
  --input source.brep --output converted.step --report result.json
```

文件入口当前接收恰好一个 NURBS 裁剪面，支持 `.brep`、`.step`、`.stp`。返回码：`0` 为成功、`1` 为明确拒绝转换、`2` 为参数/读写/内核错误。输入、输出和报告必须是不同文件，路径别名也会检查。多面装配的事务替换、缝合和邻面上下文应通过 C++ 集成层实现；CLI 不将孤立面转换冒充装配级布尔模型替换。

### 连续处理与 CAD 性能复现

```bash
./build/bs2quad_evidence --count 1000 --output-dir evidence/my-run
./build/bs2quad_cad_benchmark --output-dir evidence/my-cad --iterations 5
bash scripts/verify.sh build evidence/my-verification
```

`bs2quad_evidence` 保留逐例 CSV、输入参数、BRep 输入和汇总。数据覆盖五类解析正例与自由曲面反例，旋转、平移、50–200 个控制点和重复转换。其返回码反映分类、稳定性与一致性；10 ms 指标另以 `decision_10ms_pass` 记录。

CAD 基准针对同一有限裁剪域、相同查询/操作数，先核验结果有效性和几何一致性，再报告投影、Section 求交及 Common 布尔运算的逐次耗时。基准进程返回 `0` 表示测量有效完成，50% 要求另以 `all_operation_targets_pass` 记录。完整验证脚本同时检查这两个性能门槛，**任何门槛未通过都会返回非零**，并拒绝覆盖非空证据目录。

## C++ 接口示例

```cpp
#include "BS2QuadCore.h"
#include "TestDataGenerator.h"

BS2Quad::TestDataGenerator generator;
auto source = generator.GenerateCylinderSurface(5.0, 12.0, 10, 10);
BS2Quad::BS2QuadCore converter(1e-5);
auto result = converter.Convert(source);
if (result.success) {
    // Source-support distance only. No boundary or adjacency claim is implicit.
    auto analytic = result.quadricSurface;
    double bound_mm = result.accuracyResult.certifiedMaxDistance;
}
```

`Convert(face)` 执行裁剪面验收；`Convert(face, tangentNeighbors)` 另外检查指定邻面。锐边不应作为要求 G1 的邻面传入。各 fitter 的 `FitFromPoints` 仅拟合给定有限点，使用最大实际世界坐标残差验收，不对点间连续曲面发证。一个转换器实例保存可变统计信息，应由单线程调用，或由调用方加锁。

## 交付与限制

[本轮交付状态](docs/delivery_status.md)；[六页方案与实测 PDF](output/pdf/BS2Quad_方案与实测报告_20261016.pdf)；[GitHub 私有仓库](https://github.com/yanchaomei/bs2quad)。

- [竞赛策略与评分映射](docs/competition_strategy.md)、[方案设计](docs/design.md)、[项目故事](docs/story.md)、[实验报告](docs/experiment_report.md)、[误差分析](docs/error_analysis.md)、[API 与复现说明](docs/user_guide.md)。
- 完整原始记录随交付包提供；本次历史失败记录和未达标的 CAD 操作保留，不改写为成功。
- SGK 1.7 的真实 SDK、初始化、对象适配、目标环境构建和同一套验收仍需完成。配置 `BS2QUAD_BACKEND=SGK` 会明确失败，工程中没有模拟 SGK 接口。
- 证书采用充分条件，可能保守拒绝实际可接受的近圆锥等曲面；支持次数上限为 12，圆环证书限环状圆环面 `R > r > 0`。
- 原有未附原始记录的 2025 年说明已存放在 [历史未核验材料](docs/archive/)，不作为本次实测依据。

项目沿用原仓库的 [Apache 2.0 许可证](license)。OCCT 和 Google Test 为独立依赖，其许可证与来源见 [第三方说明](THIRD_PARTY_NOTICES.md)。
