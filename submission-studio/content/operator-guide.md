# BS2Quad 演示与操作指南

## 一、前提与范围

本指南与演示视频使用相同 S01-S07 场景编号。当前支持单个 NURBS 裁剪面文件，输入输出可用 BRep/STEP。多面装配替换需要上层提供邻接上下文；本指南不把孤立面转换当作装配级替换。

在远程 Linux 安装 OCCT 7.7.0、C++17 编译器、CMake 与 Google Test，具体命令见工程根目录 INSTALL.md。设置 OpenCASCADE_DIR 和 LD_LIBRARY_PATH 后构建。原始核心代码不因本轮材料制作发生改变。

```bash
export OpenCASCADE_DIR=/path/to/occt770/lib/cmake/opencascade
export LD_LIBRARY_PATH=/path/to/occt770/lib
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure --no-tests=error
```

## 二、逐场景操作与观察

### S01 项目与核心矛盾

操作：查看 assets/mechanism.png 及 provenance，理解候选与独立验收的边界。

预期观察：AI 机制示意不构成实验结果；G1 需要实际邻面输入。

### S02 接口与处理流程

```bash
./build/bs2quad_main --help
```

预期观察：版本 1.1.0，OCCT 后端；帮助文本明确 SGK 尚未接入。

### S03 真实圆柱面转换

```bash
./build/bs2quad_main --input evidence/20261016/final-release-v3/surface1000/inputs/case_1.brep --output converted-cylinder.brep --report cylinder.json
```

预期观察：退出 0；success=true，type=Cylinder，保存新几何与报告。判定与完整转换耗时分别记录。

### S04 自由曲面明确拒绝

```bash
./build/bs2quad_main --input evidence/20261016/final-release-v3/surface1000/inputs/case_5.brep --output rejected.brep --report rejected.json
```

预期观察：退出 1；success=false，拒绝自由曲面并保存原因；没有可发布的新几何。

### S05 针对性反例回归

```bash
./build/unit_tests --gtest_filter='ValidationRegression.FullDomainCertificateRejectsNarrowBumpBetweenAllSamplingColumns:ValidationRegression.FaceOrientationReversalIsNotNormalPreservation:ValidationRegression.ReverseBoundaryDirectionDetectsAnExtraInteriorHole:ValidationRegression.ConversionWithActualNeighborsAcceptsSmoothAndRejectsCreasedFaces'
```

预期观察：四项固定回归通过。覆盖窄凸起、反向法向、多孔边界与平滑/折角邻面。

### S06 已保存的一千例实验

```bash
cat evidence/20261016/final-release-v3/surface1000/summary.json
```

预期观察：这是 2026-10-16 主实验的归档读取；834/166、0 FP/FN、控制点50-200、max8.247364ms。

### S07 性能结果与集成边界

```bash
cat evidence/20261016/final-release-v3/cad/summary.json
```

预期观察：15 项有效、9 项达到50%；SGK、连续边界/G1及STEP出口仍有缺口。


## 三、结果字段与错误处理

| 字段 / 状态 | 含义与处理 |
|---|---|
| success / error / message | 先检查成功状态和错误原因，再使用输出几何 |
| directed_support_bound_mm | 原样条完整参数域到解析支撑面的单向上界 |
| sampled_boundary_hausdorff_mm | 双向边界并集的有限采样诊断；不是连续域证书 |
| normal_deviation_degrees | 原面与转换面的有向法向保持，不代替相邻面 G1 |
| adjacency_checked | 只有提供并检查真实邻面才为真 |
| null 数值 | 未执行或无法可靠得到的量，不应解释为零误差 |
| 退出码 0 / 1 / 2 | 成功 / 明确拒绝转换 / 参数、文件或内核错误 |
| 输入输出冲突 | 输入、输出与报告应为不同文件；符号链接别名也会拒绝 |
| SGK 配置失败 | 当前不支持真实 SGK 构建；不要建立假接口绕过失败 |

## 四、复现与保留回执

```bash
./build/bs2quad_evidence --count 1000 --output-dir evidence/a-new-run
./build/bs2quad_cad_benchmark --output-dir evidence/a-new-cad --iterations 5
bash scripts/verify.sh build evidence/a-new-verification
```

使用新的输出目录。返回码为 0 的 CAD 测量只表示有效执行；全部达到 50% 需另查 all_operation_targets_pass。完整脚本会检查该门槛及 SGK 标志。内存工具下的耗时不用于十毫秒指标。

本视频保留真实远程执行事件，执行完成后停留画面讲解；未缩短等待、未加速或截断旁白。终端视窗会滚动，完整输出另附。几何图来自实际 BRep 求值，统计图来自主实验 CSV。中文声音来自本机 macOS Tingting；主观自然度与逐字发音仍需人工试听确认。

## 五、验收与提交状态

查看 qa/ 下的 PDF、旁白、视频和包审计记录。字节校验通过不代表几何硬指标通过、主观听审通过或主办方接受。此材料包尚未发送邮件或上传门户。
