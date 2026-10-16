# API 与证据说明

## 转换接口

| 接口 | 检查范围 | 使用注意 |
|---|---|---|
| `Convert(Geom_BSplineSurface)` | 候选恢复、输入检查、完整参数域支撑面距离界、诊断采样 | 不隐含边界或 G1 已检查 |
| `Convert(TopoDS_Face)` | 以上检查，以及裁剪面重建、拓扑/面积/边界/裁剪覆盖/原面有向法向保持 | 返回的 `quadricSurface` 与 `quadricFace` 都对应输入世界位置 |
| `Convert(face, tangentNeighbors)` | 裁剪面验收，以及指定真实共享边的邻面有向法向夹角 | 拒绝空邻面、自身邻面、无共享边和超过 0.1° 的邻面；锐边不要列入该参数 |
| `ConvertBatch(vector<surface>)` | 逐项执行支撑面转换，失败项保留结果和消息 | 不把单项拒绝当成整批中断；统计信息可读取 |
| `SetTolerance(d)` | 设置有限正距离容差，非法值抛 `invalid_argument` 且保留原值 | 边界阈值仍固定 `1e-5 mm` |

一个转换器包含可变计数，不应在多个线程中无锁共享。独立实例可以由调用方安排并行，但本次证据来自串行处理。

## 精度字段

- `maxDistanceCertified`：完整源参数域到解析支撑面的单向距离界是否构造成功。
- `certifiedMaxDistance`：该界的数值；它用于支撑面接受门槛。
- `maxDistance/avgDistance/rmsDistance`：诊断采样误差，不称作连续域实测最大值。
- `boundaryChecked/hausdorffDistance`：双向边界并集采样到完整目标边的距离。
- `normalChecked/normalDeviationDegrees`：原面与转换面对应空间位置的有向法向保持。
- `adjacencyChecked/g1ContinuityError`：提供邻面上下文后沿共享边的有向法向夹角。
- `validationComplete`：本次请求的检查是否完整；`meetsRequirements` 另表示结果是否满足对应门槛。

未执行或失败的未知量保留无穷值，CLI JSON 使用 `null`，避免把未检查写成零误差。程序必须检查 `success`；失败结果不发布可直接使用的转换面。

`decisionTime` 是候选恢复和支撑面几何验收耗时；`conversionTime` 包括该调用中的拓扑重建、邻面检查及可选旧版性能诊断。正式 CAD 性能结论使用独立受控基准。

## 有限点拟合

五个 fitter 的 `Fit(NURBS)` 使用内核候选和独立完整域距离界。`FitFromPoints` 仅拟合有限点，不对点间曲面发证，返回最大残差验收结果。失败会清空输出并把拟合误差设为无穷值；参数 getter 保存最后接受的参数，首次接受前不能当作拟合证据。

最少点数为平面 3、球面 4、圆柱 6、圆锥 10、环状圆环 12。算法检查秩与条件数，并对真正存储的世界坐标几何再次计算最大残差。有界优化最多使用 6 个初值、每个 100 步；病态或不足以确定类型的局部点集可能被安全拒绝。`SetOptimize(false)` 关闭这一有限点几何细化，不能关闭主转换路径的距离验收。旧 `BS2QuadCore::SetEnableOptimization` 已标记弃用兼容入口，固定的解析恢复/证书流程不提供绕过开关。

## 运行和错误

```bash
bs2quad_main --demo
bs2quad_main --generate 6 --output-dir data/six-families --seed 42
bs2quad_main --input source.brep --output converted.step --report result.json
```

文件入口只接收一个 NURBS 面。退出码 `0/1/2` 分别表示成功、明确拒绝转换、参数/文件/内核错误。输入、输出和报告必须指向不同文件，含已有文件及父目录符号链接的别名。报告写入必须完成 flush/close 检查，否则返回错误。

`DATA_CORRUPT`、`OUT_OF_RANGE`、`NON_QUADRIC`、`HIGH_ERROR` 和内核失败消息分别保留原因。`NON_QUADRIC` 表示当前候选恢复流程未找到可接受候选，不是对所有数学表达的不存在性证明。

`TrySimplify` 仅在整张面有已验收解析结果时返回该结果，否则保留自由曲面并说明没有实施局部分割，不会把原始 NURBS 塞进解析结果列表冒充转换。

## 回执与性能口径

`bs2quad_evidence` 的每行保存 case id、真值类型、实际类型、控制点数、是否接受、重复一致性、判定/总耗时、采样误差、全域界和诊断。全部输入由固定 case 配方生成并保存为 BRep；42 是固定配置种子，尺寸和位置实际由 case id 确定。

`bs2quad_cad_benchmark` 分别保存投影、Section、Common 的相同输入、操作数、结果 BRep、查询真值、逐次耗时和结果差异。进程成功仅表示测量完成且结果一致，`all_operation_targets_pass` 单独表示是否全部达到 50%。完整脚本检查性能标志，失败时退出非零，不能把 CTest 正确性通过当作赛事全项通过。

## 当前限制

SGK 1.7 和真实船舶数据尚未验收；边界/G1 为有限采样验收；装配级替换由上层集成实现；次数和环状圆环证书范围见方案。历史文档中的数值无原始回执支持，已转入 `docs/archive`，不作为当前结果。
