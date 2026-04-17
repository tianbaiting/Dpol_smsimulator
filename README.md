# SMSimulator 5.5 — SAMURAI/DPOL Simulation & Reconstruction

RIKEN SAMURAI 终端氘核极化（DPOL）实验的 Geant4 模拟 + ROOT 重建 + 几何接受率筛选一体化仓库。

[Ask DeepWiki](https://deepwiki.com/tianbaiting/Dpol_smsimulator) · 仓库：<https://github.com/tianbaiting/Dpol_smsimulator>

---

## 三条数据流

```
┌──────────────────────────────────────────────────────────────────┐
│  [A] 几何筛选（side path，跳过 Geant4）                           │
│      RunQMDGeoFilter → acceptance CSV                             │
│                                                                  │
│  [B] Geant4 模拟                                                  │
│      sim_deuteron + .mac → sim.root                               │
│        ├ beam          : TClonesArray<TBeamSimData>               │
│        ├ FragSimData   : TClonesArray<TSimData>   ← 主分支        │
│        └ NEBULASimData : TClonesArray<TSimData>                   │
│                                                                  │
│  [C] ROOT 重建                                                    │
│      reconstruct_target_momentum → reco.root                      │
│        reco_target_momentum + reco_uncertainty + reco_interval    │
│        + reco_trajectory + reco_status + reco_config_snapshot     │
│                                                                  │
│  [D] 分析 & NN 后端                                               │
│      scripts/analysis/, scripts/reconstruction/nn_target_momentum │
└──────────────────────────────────────────────────────────────────┘
```

详见 [docs/SUBSYSTEMS.md §0 全局数据流](docs/SUBSYSTEMS.md#0-全局数据流)。

---

## 快速上手

```bash
micromamba activate anaroot-env

mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

# B: Geant4 模拟
bin/sim_deuteron configs/simulation/macros/simulation.mac

# C: ROOT 重建
bin/reconstruct_target_momentum \
    --input  build/sim/pdc_truth_grid.root \
    --output build/reco/pdc_truth_grid_reco.root \
    --geometry-macro configs/simulation/geometry/3deg_1.15T.mac

# A: 几何筛选（side path）
bin/run_qmd_geo_filter \
    --qmd    data/qmd/imqmd.root \
    --field  1.15 --angle 3 \
    --output build/filter/3deg_115T/
```

---

## 子系统速览

| 子系统 | 入口 | 做什么 | 细节 |
| --- | --- | --- | --- |
| Geant4 模拟 | `bin/sim_deuteron` | 追粒子到 PDC/NEBULA，写 sim.root | [§1](docs/SUBSYSTEMS.md#1-geant4-模拟) |
| ROOT 重建 | `bin/reconstruct_target_momentum` | PDC → 靶点动量（RK 或 NN） | [§2](docs/SUBSYSTEMS.md#2-root-重建) |
| 几何筛选 | `bin/run_qmd_geo_filter` | 跳过 Geant4 的快速接受率扫描 | [§3](docs/SUBSYSTEMS.md#3-几何筛选) |
| 分析 & NN | `scripts/analysis/`, `scripts/reconstruction/nn_target_momentum/` | ensemble coverage, 训练 MLP | [§4](docs/SUBSYSTEMS.md#4-分析--nn-后端) |

---

## 常用开发命令

```bash
./build.sh              # clean rebuild（少用）
./test.sh               # 跑所有测试
cd build && ctest -L unit                     # 只跑单元测试
SM_LOG_LEVEL=DEBUG bin/sim_deuteron <macro>   # 高日志级别
```

---

## 目录总览

```
apps/           可执行入口（sim_deuteron, run_reconstruction, tools/）
libs/           核心库（smg4lib, sim_deuteron_core, analysis_pdc_reco,
                analysis, qmd_geo_filter, geo_accepentce, smlogger）
configs/        Geant4 宏 + GDML 几何 + 物理配置
scripts/        分析 / 批处理 / NN 训练 / 事件生成
tests/          单元 + 集成测试
docs/           设计 spec / 报告 / 子系统文档
skills/         领域 skill（Claude Code 深度文档）
```

---

## 更多文档

- [docs/SUBSYSTEMS.md](docs/SUBSYSTEMS.md) — 各子系统深度（I/O、组件地图、入口命令、典型改动点、坑）
- `docs/reports/` — 物理结果报告（PDF + TeX）
- `skills/` — 按领域组织的 Claude Code skill（PDC 重建、几何筛选等）
- `CLAUDE.md` — Claude Code 工作指引（本地文件，不入库）

---

## 依赖与环境

- **基础栈**：ROOT 6.x, Geant4 10.x+, XercesC, CMake 3.16+
- **可选**：ANAROOT（`WITH_ANAROOT=ON`，需 `TARTSYS` 环境变量）
- **Python（分析 + NN）**：`anaroot-env` micromamba 环境（含 PyROOT、PyTorch、numpy、matplotlib）
- **语言**：C++20（核心），Python 3.x（分析 / ML）

构建前：
```bash
micromamba activate anaroot-env   # 必须
export TARTSYS=/path/to/anaroot   # 若 WITH_ANAROOT=ON
```

CMake 选项与环境变量完整列表见 [docs/SUBSYSTEMS.md §A](docs/SUBSYSTEMS.md#a-附录)。

---

## 提交风格

简短、祈使、常小写：`add ...`, `fix ...`, `delete ...`, `update ...`。
