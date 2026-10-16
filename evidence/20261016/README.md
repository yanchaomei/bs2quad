# 2026-10-16 原始证据索引

## 主结论

本次实测为 OCCT 7.7.0 原型，SGK 1.7 未验证。主记录为 `final-release-v3/`；不要用较早版本的最好值替换最终结果。64 单元测试、5 个 Release CTest 组通过；1000 例 834 正确接受 / 166 正确拒绝，0 不一致，实际 50-200 控制点，最高判定 8.247364 ms。CAD 15 组测量有效、9 组达到 50%。完整 verify 返回 1，保留 SGK 和 CAD 门槛失败。

## 最终运行

| 文件 / 目录 | 含义 |
|---|---|
| `source-final.json` / `source-readback.json` | 51 个源码、测试和验证配方文件的逐文件散列；远程运行前后相同 |
| `environment-final.txt` | CPU、容器 OS、GCC、CMake、依赖、编译选项、实际动态链接路径 |
| `final-release-v3/status.tsv` | 构建、正确性、连续处理、SGK/判定/CAD 门槛逐项退出码 |
| `final-release-v3/correctness.log` / `unit-final.xml` | 5 CTest 组与 64 单元测试原始回执 |
| `final-release-v3/cli.log` / `cli/` | 22 项正常、异常与别名路径检查 |
| `final-release-v3/surface1000/` | 1000 份输入 BRep、输入配方、逐例分类/计时/误差 CSV 与汇总 |
| `final-release-v3/cad/` | 15 组 CAD 测量、90 行耗时、查询 CSV、输入/操作数/结果 BRep |
| `replay_remote_final.sh` | 实际最终远程检查命令；使用新输出目录重跑，避免覆盖旧证据 |
| `asan-build.*` / `asan-ctest.*` | 插桩构建成功；CTest 4/5 通过，STEP 导出退出 LSAN 失败（exit 8） |
| `asan1000*` | ASan/UBSan 连续 1000 例核心转换 exit 0 |
| `valgrind1000*` / `valgrind-interfaces*` | 1000 例及 46 接口回归 exit 0；lost/errors 0，still reachable 16 bytes |
| `occt-step-control/` | 无 BS2Quad 代码的依赖退出泄漏对照：源码、构建/运行脚本、SDK散列和10份控制日志 |
| `symlink-artifacts.json` | 历史路径别名测试夹具的链接元数据，交付树中不保留指向远程绝对路径的链接 |
| `files.sha256.json` | 此证据目录所有常规文件（清单自身除外）的逐文件散列 |

## 历史与失败记录

原始配置/构建失败、RED 回归和多次诊断日志保留在本目录顶层。`regressions/` 保存修复前的区间证书源码快照，仅供回溯，不能用于最终编译。`run1000/` 是优化前 1000 例，曾超过 10 ms；`run1000_optimized/` 为较早网格优化结果；`control-limits1000/` 才开始明确覆盖实际 50 与 200 控制点。`final-release/` 和 `final-release-v2/` 是源文件清单继续完善前的历史运行，不能代替 v3。

`cad-pilot/`、`cad-full/`、`cad-torus-repro/` 保留圆环投影判据失败；`cad-torus-oracle-probe.log` 显示在支撑且在裁剪域内的点被通用极值接口高估约 0.31 mm，后续以已知法向偏移真值及相同有限域算法修正判据。`cad-full-corrected/` 等是中间修正记录。`output-red.log` 等保留缓冲写失败被误报成功的回归。

不同运行的时间与控制网不完全相同；内存插桩下的耗时不参与 10 ms 指标。不要对本目录跨运行 CSV 做无条件合并。

## 未解决项

真实 SGK 1.7、连续边界/G1 证书、全部 CAD 50% 目标、完整 STEP 出口无泄漏均未通过。纯 OCCT 对照的固定泄漏总量不等于无泄漏，也不证明其他 STEP 工作负载不会增长。完整分析见 `docs/experiment_report.md`、`docs/error_analysis.md` 和独立审查记录。
