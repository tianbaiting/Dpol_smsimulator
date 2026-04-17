# SMSimulator

基于 Geant4 的 RIKEN SAMURAI/DPOL（氘极化）实验模拟与重建框架。

[Ask DeepWiki](https://deepwiki.com/tianbaiting/Dpol_smsimulator) · 仓库：<https://github.com/tianbaiting/Dpol_smsimulator>

## 快速命令

```
./build.sh      # 编译项目（完整重建）
./run_sim.sh    # 运行模拟
./test.sh       # 运行测试
```

增量构建：

```bash
micromamba activate anaroot-env
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_APPS=ON -DBUILD_ANALYSIS=ON -DWITH_ANAROOT=ON
make -j$(nproc)
```

## 目录结构

```
libs/                       C++ 库源码
  ├─ smlogger/              异步日志（SM_INFO / SM_DEBUG / SM_WARN / SM_ERROR）
  ├─ smg4lib/               Geant4 核心封装（actions / construction / physics）
  ├─ sim_deuteron_core/     氘核物理模拟核心
  ├─ analysis_pdc_reco/     PDC→靶动量重建（主运行时框架，新功能放这里）
  ├─ analysis/              遗留分析库（兼容用，避免新增功能）
  ├─ geo_accepentce/        几何接受度分析
  └─ qmd_geo_filter/        QMD 几何过滤

apps/                       可执行程序
  ├─ sim_deuteron/          主 Geant4 模拟
  ├─ run_reconstruction/    靶动量重建 CLI
  └─ tools/                 分析工具

configs/simulation/         配置文件
  ├─ macros/                Geant4 宏文件（.mac）
  └─ geometry/              GDML 几何定义

scripts/
  ├─ analysis/              Python / ROOT 分析脚本
  ├─ batch/                 批处理（如 batch_run_ypol.py）
  └─ reconstruction/nn_target_momentum/   NN 动量重建：训练与推理

data/
  ├─ input/                 输入数据
  ├─ simulation/            模拟输出
  └─ reconstruction/        重建输出

tests/                      GoogleTest 单元与集成测试
skills/                     子系统领域文档（SKILL.md）
docs/                       构建与分析文档
```

## 运行示例

```bash
# 基本模拟
./run_sim.sh configs/simulation/macros/simulation.mac

# 可视化模拟
./run_sim.sh configs/simulation/macros/simulation_vis.mac

# 直接运行
./bin/sim_deuteron configs/simulation/macros/your_macro.mac

# 靶动量重建
./bin/reconstruct_target_momentum --config configs/reconstruction/default.yaml
```

## 测试

```bash
./test.sh                            # 全部测试
cd build && ctest --output-on-failure
cd build && ctest -L unit            # 按 label 过滤（unit / performance / visualization / realdata）
cd build && ctest -R test_name       # 按名称过滤

# 可视化测试
SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor
```

## 数据分析

```bash
# ROOT 宏
cd configs/simulation/macros && root -l run_display.C

# Python 分析
cd scripts/analysis && python analyze_*.py

# 批量运行
cd scripts/batch && python batch_run_ypol.py

# NN 动量重建
python scripts/reconstruction/nn_target_momentum/train_mlp.py
python scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py
```

## CMake 选项

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `BUILD_TESTS` | ON | 构建单元/集成测试 |
| `BUILD_APPS` | ON | 构建全部可执行程序 |
| `BUILD_ANALYSIS` | ON | 构建分析库 |
| `WITH_ANAROOT` | ON | 链接 ANAROOT（未定义 `TARTSYS` 时关闭） |
| `WITH_GEANT4_UIVIS` | ON | 启用 Geant4 UI/Vis 驱动 |

## 环境变量

| 变量 | 用途 |
| --- | --- |
| `SM_LOG_LEVEL` | 日志级别：TRACE / DEBUG / INFO / WARN / ERROR / CRITICAL |
| `SM_BATCH_MODE` | 批处理模式日志 |
| `SM_TEST_VISUALIZATION` | 测试可视化开关 |
| `TARTSYS` | ANAROOT 安装路径（可选） |

## 常用工作流

开发：编辑源码 → `./build.sh` → `./test.sh` → `./run_sim.sh ...mac`

批量：编辑配置 → `scripts/batch/batch_run_ypol.py` → `scripts/analysis/analyze_*.py`

增量编译单模块：`cd build && make <target>`（例如 `make sim_deuteron`）

## 调试

```bash
make VERBOSE=1           # 查看完整编译命令
cmake .. --trace         # CMake 执行跟踪
ldd bin/sim_deuteron     # 检查动态库依赖
```

## 文档

- 子系统领域文档入口：`skills/`（每个子目录含 `SKILL.md`；PDC 重建从 `skills/smsim-pdc-target-reco-overview/` 开始）
- 项目说明：`CLAUDE.md`、`AGENTS.md`
- 构建指南：`docs/BUILD_GUIDE.md`
- 测试流程：`RUN_TEST_GUIDE.md`
- 分析报告：`docs/DPOL_Analysis_Report.md`、`docs/reports/`

## 常见问题

- 找不到 ROOT：`source $ROOTSYS/bin/thisroot.sh`
- 找不到 Geant4：`source /path/to/geant4.sh`
- 找不到 ANAROOT：`export TARTSYS=/path/to/anaroot`，或 `cmake .. -DWITH_ANAROOT=OFF`
- 编译失败：检查环境变量 → `make VERBOSE=1` → 必要时清空 `build/` 重来

## 编码约定

C++20；4 空格缩进，左括号同行；源文件 `.cc`，头文件 `.hh`；类型 PascalCase，成员字段 `f` / `fg` 前缀；日志统一用 `SM_INFO` / `SM_DEBUG` 等宏，不用 `std::cout` / `G4cout`。
