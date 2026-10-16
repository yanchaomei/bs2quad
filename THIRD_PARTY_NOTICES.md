# 第三方依赖

本项目没有将第三方几何识别能力声称为自研算法。

| 依赖 | 本次验证版本 | 用途 | 上游来源与许可证 |
|---|---|---|---|
| Open CASCADE Technology | 7.7.0 | 几何对象、解析候选恢复、STEP/BRep、拓扑修复、距离/求交/布尔基础操作 | [官方仓库](https://github.com/Open-Cascade-SAS/OCCT/tree/V7_7_0)，LGPL 2.1 及 OCCT 特别例外；以该版本随附 LICENSE_LGPL_21.txt、OCCT_LGPL_EXCEPTION.txt 为准 |
| Google Test | Ubuntu 22.04 包 1.11.0 | 公开接口单元与回归测试 | [官方仓库](https://github.com/google/googletest)，BSD-3-Clause |
| CMake | 实际版本记录在环境回执 | 构建和 CTest | [官方项目](https://cmake.org/)，BSD-3-Clause |

OCCT 官方 `V7_7_0` codeload 源码包 SHA-256：

`4cadb6d0a9e92dbb1a4eeb48d497b009d8138ffde64f2468d4a1c3f10a052722`

独立增量为本项目的区间/Bernstein 距离上界、错误关闭验收、公开接口修复、裁剪面集成及证据流程。发布可执行文件或第三方源码时应同时保留对应版本附带的许可证和说明。本仓库的依赖构建配方不会变更系统几何内核。
