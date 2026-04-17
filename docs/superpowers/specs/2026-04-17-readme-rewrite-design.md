# README 重写设计文档

- 作者：Baiting Tian + Claude
- 日期：2026-04-17
- 状态：设计稿待评审

---

## 背景与问题

当前 `README.md`（158 行）把重点放在"怎么编译、怎么跑"，对**做了什么、数据怎么流、每个子系统的边界**几乎没交代。用户反馈：

> "我觉得 markdown 写得不好，很多都没概括到。我希望改进一下，包括 Geant4 模拟、ROOT 重建，还有几何筛选。"

具体欠缺：

1. **Geant4 模拟层**：`sim_deuteron` 到底读什么输入、吐什么 ROOT 分支、用哪个 physics list、几何宏放在哪，读者看完 README 不知道。
2. **ROOT 重建层**：`reconstruct_target_momentum` 的三种 RK 模式、NN 后端、Fisher/MCMC/Profile 三种不确定度方法、PDC→动量的链路，README 完全没提。
3. **几何筛选层**：`RunQMDGeoFilter` 这个完全绕过 Geant4 的快速几何筛选工具，从来没出现在 README 里。

目标：让一个**第一次进入仓库的贡献者**在十分钟内回答以下问题——

- 这个项目的三条数据流主干是什么？
- 我要加一个新探测器 / 新重建算法 / 新筛选条件，改哪个子目录？
- 我想跑一次完整的 sim → reco → coverage，命令序列是什么？
- 每个子系统的 I/O 边界在哪里？

---

## 方案选择

考察过三种写法：

1. **单文件扩写**：直接把 README 从 158 行写到 600 行。缺点：首屏信息密度太低，数据流图和子系统细节会互相稀释。
2. **全部搬进一个新 doc**：README 只留构建命令，细节全部放 `docs/SUBSYSTEMS.md`。缺点：新人在 README 里看不到数据流，不知道要去翻 doc。
3. ✅ **README + docs/SUBSYSTEMS.md 分层**（选定）：
   - README 约 100 行，承担"首屏路由"——含全局数据流图 + 三个子系统一句话描述 + 指向 docs 的链接 + 构建/环境信息。
   - `docs/SUBSYSTEMS.md` 约 600 行，承担"深度说明"——每个子系统一节，含组件地图、I/O、入口命令、典型改动点、常见坑。

代码体量（37,623 行 C++，分布在 8 个 libs + 3 个 apps）决定了需要分层。

---

## 顶层数据流

README 正文第一张图要表达的主干：

```
┌────────────────────────────────────────────────────────────────────────┐
│                                                                        │
│   [A] 几何筛选（side path）                                             │
│       RunQMDGeoFilter → Acceptance CSV / JSON                           │
│       不经过 Geant4，只用 RK4 + 磁场 + 探测器包络                         │
│                                                                        │
│   [B] Geant4 模拟（main path）                                          │
│       sim_deuteron + .mac → sim.root                                    │
│         ├── beam       : TClonesArray<TBeamSimData>                     │
│         ├── FragSimData: TClonesArray<TSimData>  ← 主分支（PDC 命中）    │
│         └── NEBULASimData: TClonesArray<TSimData>                       │
│                                                                        │
│   [C] ROOT 重建（main path）                                            │
│       reconstruct_target_momentum → reco.root                           │
│         ├── reco_target_momentum  (px, py, pz)                          │
│         ├── reco_uncertainty      (fisher/mcmc/profile σ)               │
│         ├── reco_interval         (68/95% CI)                           │
│         ├── reco_trajectory       (RK 步长数组)                          │
│         ├── reco_status           (收敛码)                               │
│         └── reco_config_snapshot  (后端/参数/几何)                        │
│                                                                        │
│   [D] 分析 & NN 后端训练                                                 │
│       scripts/analysis/*.py + scripts/reconstruction/nn_target_momentum │
│       ensemble coverage, per-truth bias/σ, 模型训练 → .pt → JSON        │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

关键点：
- **A 是 B/C 的一个旁路**——当你只关心"这个入射动量能不能被接收器看到"，不需要跑完整 Geant4。
- **B 和 C 是主流水线**——sim_deuteron 产生的 `FragSimData` 分支是 C 的唯一主入口。
- **D 是 C 的下游**——对 reco.root 做聚合分析；同时它也**向 C 的 NN 后端反向提供模型**（`train_mlp.py` → `PDCNNMomentumReconstructor::LoadModel`）。

---

## 文件结构

```
README.md                   ← 100 行：首屏路由（重写）
docs/SUBSYSTEMS.md          ← 600 行：子系统深度（新建）
```

README 不再包含：
- 目录树（搬进 SUBSYSTEMS.md 的组件地图）
- 完整 CMake 选项（搬进 SUBSYSTEMS.md §A）
- 各环境变量表（保留 3 个最常用的在 README，其它搬走）

---

## README 大纲（约 100 行）

```markdown
# SMSimulator 5.5 — SAMURAI/DPOL Simulation & Reconstruction

一句话定位（1 行）。

## 三条数据流

[插入上面的数据流框图]

## 快速上手

```bash
micromamba activate anaroot-env
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)

bin/sim_deuteron          configs/simulation/macros/simulation.mac
bin/reconstruct_target_momentum --input <sim.root> --output <reco.root>
bin/run_qmd_geo_filter    --angle 5 --field 1.15 --qmd <qmd.root>
```

## 子系统速览

| 子系统 | 入口 | 做什么 | 细节 |
| --- | --- | --- | --- |
| Geant4 模拟 | `bin/sim_deuteron` | 追粒子到 PDC/NEBULA，写 sim.root | [docs/SUBSYSTEMS.md §1](docs/SUBSYSTEMS.md#1-geant4-模拟) |
| ROOT 重建 | `bin/reconstruct_target_momentum` | PDC → 靶点动量（RK / NN） | [docs/SUBSYSTEMS.md §2](docs/SUBSYSTEMS.md#2-root-重建) |
| 几何筛选 | `bin/run_qmd_geo_filter` | 跳过 Geant4 的快速接受率扫描 | [docs/SUBSYSTEMS.md §3](docs/SUBSYSTEMS.md#3-几何筛选) |
| 分析 & NN | `scripts/analysis`, `scripts/reconstruction/nn_target_momentum` | ensemble coverage, 训练 MLP | [docs/SUBSYSTEMS.md §4](docs/SUBSYSTEMS.md#4-分析--nn-后端) |

## 常用开发命令

- `./build.sh` — clean rebuild（少用）
- `./test.sh`  — 跑所有测试
- `ctest -L unit` — 只跑单元测试
- `SM_LOG_LEVEL=DEBUG bin/sim_deuteron ...` — 调高日志等级

## 目录总览

（简表：apps/、libs/、configs/、scripts/、tests/、docs/、skills/）

## 更多文档

- `docs/SUBSYSTEMS.md` — 各子系统深度
- `skills/` — 领域 skill（复杂子系统专用）
- `CLAUDE.md` — Claude Code 工作指引（本地，不入库）

## 依赖与环境

micromamba 环境名、ROOT/Geant4 版本最低要求、`TARTSYS` 开关、`WITH_ANAROOT` 开关。

## 提交风格

一行说明（"add ...", "fix ...", "delete ..."）。
```

---

## docs/SUBSYSTEMS.md 大纲

### §0 全局数据流（约 30 行）

复述 README 的数据流图，再加：
- 输入产物（sim.root、reco.root、filter.csv）的生命周期
- 每个产物的**体积数量级**（经验值：N=50 ensemble 的 sim.root ~数百 MB；reco.root ~数 MB；filter CSV ~KB）
- 物理坐标系提示：SAMURAI 坐标系 vs 实验室系，磁场 30° 旋转

---

### §1 Geant4 模拟（约 130 行）

#### 1.1 做什么

`sim_deuteron` 是 Geant4 主程序。它读一个 `.mac` 宏文件，构造 SAMURAI/DPOL 探测器（磁铁、PDC1/PDC2、NEBULA、靶），把入射氘核（或其他束流）送进去，追到产物止，把事件级数据写成 ROOT TTree。

典型场景：
- 对一个网格化的 (px, py, pz) 真值点各发 1 个事件 → `pdc_truth_grid_*.root`（用于 RK 重建验证）
- 对同一真值点重复 50 次 → `ensemble_n50/sim.root`（用于 Fisher σ coverage 评估）
- 对 IPS target 不同位置扫描 → IPS veto 几何优化

#### 1.2 组件地图

| 路径 | 角色 |
| --- | --- |
| `apps/sim_deuteron/main.cc` | 入口，实例化 `DeutDetectorConstruction` + `DeutPrimaryGeneratorAction` + `DeutSteppingAction` + `DeutTrackingAction` |
| `libs/sim_deuteron_core/` | 氘核实验的探测器构造、粒子源、步进/追踪动作（业务逻辑层） |
| `libs/smg4lib/include/DipoleConstruction.hh` | SAMURAI 磁铁几何 |
| `libs/smg4lib/include/PDCConstruction.hh` | PDC1/PDC2 漂移室几何与 Sensitive Detector |
| `libs/smg4lib/include/NEBULAConstruction.hh` | NEBULA 中子墙几何 |
| `libs/smg4lib/src/data/src/FragSimDataInitializer.cc` | 定义 `FragSimData` 分支（`tree->Branch(name, &TClonesArray<TSimData>)`）|
| `libs/smg4lib/src/data/src/BeamSimDataInitializer.cc` | 定义 `beam` 分支 |
| `libs/smg4lib/include/MagField.hh` + `filed_map/` | 磁场（常数场或映射场） |
| `libs/smg4lib/include/QGSP_BIC_XS.hh` / `QGSP_INCLXX_*` / `QGSP_MENATE_R` | Physics list 工厂 |
| `configs/simulation/macros/` | 运行宏：`simulation.mac`, `simulation_vis.mac`, `test_pencil.mac` 等 |
| `configs/simulation/geometry/*.mac` | 几何宏：`3deg_1.15T.mac`, `5deg_1.2T.mac` 等（磁场角度 + 场强） |
| `configs/simulation/geometry/SAMURAIPDC.xml` | PDC 丝参数 XML |
| `configs/simulation/geometry/NEBULA_Dayone.csv` | NEBULA 模块表 |
| `scripts/event_generator/` | 束流输入文件生成（momentum grid、disk_pz 等） |

#### 1.3 输入与输出

**输入**：
- 一个 `.mac`（例如 `configs/simulation/macros/simulation.mac`）。内含 `/run/beamOn`、`/gps/...`、`/SMG4/physics/...`、`/SMG4/geometry/...` 等命令。
- 可选：一个束流 ASCII 文件（通过 `/SMG4/primary/inputFile ...` 设置），每行一个 (px, py, pz) 真值点。
- 可选：`filed_map/` 下的磁场映射文件（默认使用 `1.15T` 常数场）。

**输出**：`<tag>_sim.root`，含三个 TTree 分支：

| 分支 | 类型 | 关键字段 | 含义 |
| --- | --- | --- | --- |
| `beam` | `TClonesArray<TBeamSimData>` | `px, py, pz, Ek, event_index` | 入射束流真值（每事件 1 条） |
| `FragSimData` | `TClonesArray<TSimData>` | `pos[3], mom[3], track_id, parent_id, detector_id, edep, time` | PDC 平面上的"碎片"命中（主分支） |
| `NEBULASimData` | `TClonesArray<TSimData>` | 同上 | NEBULA 模块上的中子命中 |

`TSimData::detector_id` 用于区分 PDC1 / PDC2 / NEBULA bar。`pos[3]` 是全局坐标 (mm)，`mom[3]` 是动量 (MeV/c)。

#### 1.4 入口命令

```bash
# 基础运行
bin/sim_deuteron configs/simulation/macros/simulation.mac

# 带可视化
bin/sim_deuteron configs/simulation/macros/simulation_vis.mac

# 批量（Python 驱动）
python scripts/batch/batch_run_ypol.py --config configs/simulation/macros/simulation.mac --n-jobs 8
```

宏内常见自定义命令：
- `/SMG4/geometry/fieldAngle 3.0 deg`
- `/SMG4/geometry/fieldStrength 1.15 tesla`
- `/SMG4/physics/list QGSP_INCLXX_XS`
- `/SMG4/primary/inputFile data/input/grid_5x3.txt`

#### 1.5 典型改动点

| 想做什么 | 改哪里 |
| --- | --- |
| 加一个新探测器 | 新建 `libs/smg4lib/include/<Name>Construction.hh` + `<Name>SD.hh`；在 `DeutDetectorConstruction` 中注册；在 `FragSimDataInitializer` 或新建 Initializer 中定义分支 |
| 换磁场映射 | 把新 map 丢到 `configs/simulation/geometry/filed_map/`，在几何宏里 `/SMG4/field/mapFile <path>` |
| 换 physics list | 在宏里 `/SMG4/physics/list <name>`，name 见 `libs/smg4lib/include/QGSP_*.hh` |
| 换束流输入源 | 改 `DeutPrimaryGeneratorAction`，或用 `/gps/` 命令，或提供 ASCII 文件 |
| 改 PDC 丝距/位置 | 改 `configs/simulation/geometry/SAMURAIPDC.xml` + `PDCConstruction.hh` 里的解析逻辑 |
| 加数字化（如丝采样）| 在 `FragmentSD.hh`/`FragmentSD.cc` 里加，或写新的 stepping action |

#### 1.6 常见坑

- **GDML 路径**：`configs/simulation/geometry/filed_map/` 和 `d_work/geometry/filed_map/` 都可能被 hardcode 引用，迁移时两边都要检查。
- **PDC 丝坐标不是 (x,y)**：u/v 丝以 ±57° 旋转，重建端会还原；模拟端写进 `FragSimData` 的 `pos[3]` 是**全局 (x,y,z)**，不要误存丝坐标。
- **Physics list 对中子影响大**：QGSP_BIC_XS、QGSP_INCLXX_XS、QGSP_MENATE_R 对 NEBULA 中子的产额/能谱差很多，验收前请固定一个。
- **`TARTSYS` / ANAROOT**：默认 `WITH_ANAROOT=ON`，若环境里没有 `TARTSYS`，构建会失败——要么 `source .../thisroot.sh; export TARTSYS=...`，要么 `cmake -DWITH_ANAROOT=OFF`。
- **束流输入的坐标系**：ASCII 文件里的 (px, py, pz) 是**靶点动量**，不是实验室系。写生成脚本时别搞反。

---

### §2 ROOT 重建（约 160 行）

#### 2.1 做什么

读 sim.root 的 `FragSimData`，把 PDC1 / PDC2 两个平面的命中点解成一条粒子轨迹，反推**靶点动量** (px, py, pz)。产出 `reco.root`，含点估计、不确定度（Fisher / Laplace MCMC / Profile）、收敛状态、轨迹采样。

`libs/analysis_pdc_reco/` 是**主框架**。`libs/analysis/TargetReconstructor.hh` 是遗留兼容层，**不要**在那里加新特性。

#### 2.2 组件地图

| 路径 | 角色 |
| --- | --- |
| `apps/run_reconstruction/main.cc` | CLI 入口（ParseArgs + runtime 装配） |
| `libs/analysis_pdc_reco/src/PDCRecoFactory.cc` | 根据 `RuntimeBackend` 枚举（Auto/NN/RK/MultiDim/Legacy）装配对应 reconstructor |
| `libs/analysis_pdc_reco/src/PDCRecoRuntime.cc` | 顶层 runtime：读入 sim.root、迭代事件、调用 reconstructor、写 reco.root |
| `libs/analysis_pdc_reco/src/PDCMomentumReconstructorRK.cc` | **主 RK 重建器**，支持三种 fit mode + 两种 parameterization + LM 求解 |
| `libs/analysis_pdc_reco/src/PDCRkAnalysisInternal.cc` | LM 粗搜+多起点（7×3×5 grid + top-5 restart）+ dE/dx / MS 预留接口 |
| `libs/analysis_pdc_reco/src/PDCErrorAnalysis.cc` | Fisher 信息矩阵 / Laplace 后验 / Profile 似然 / MCMC 三方法 |
| `libs/analysis_pdc_reco/src/PDCNNMomentumReconstructor.cc` | NN 后端推理（读 JSON 模型） |
| `libs/analysis_pdc_reco/src/PDCSimAna.cc` | sim.root → 每事件 `PDCInputTrack`（u/v 丝坐标 + z 平面） |
| `libs/analysis_pdc_reco/include/PDCRecoTypes.hh` | 枚举与结构体（`SolverStatus`, `SolveMethod`, `RkFitMode`, `RkParameterization`, `PDCInputTrack`, `IntervalEstimate`） |
| `libs/analysis/include/GeometryManager.hh` | 几何宏解析（PDC 位置/角度、磁场角度/强度） |
| `libs/analysis/include/MagneticField.hh` | 常数场或映射场（30° 旋转） |
| `libs/analysis/include/ParticleTrajectory.hh` | RK4 轨迹积分（步长、记录点） |
| `libs/analysis/include/EventDataReader.hh` | sim.root 读盘封装 |
| `libs/analysis/include/RecoEvent.hh` | reco.root 写盘封装 |
| `scripts/reconstruction/nn_target_momentum/` | NN 训练+导出（见 §4） |

#### 2.3 输入与输出

**输入（CLI 部分，见 `ParseArgs` L152+）**：
- `--input <sim.root>` 或 `--input-dir <dir>`
- `--output <reco.root>` 或 `--output-dir <dir>`
- `--geometry-macro <geom.mac>`（必填，解析磁场角度/强度/PDC 位置）
- `--backend {auto|nn|rk|multidim|legacy}`（默认 `auto` — 有 NN 模型用 NN，否则 RK）
- `--nn-model <model.json>`（backend=nn 时必填）
- `--magnetic-field <T>`（覆盖几何宏里的场强）
- `--max-files <N>`, `--max-iterations <N>`
- `--pdc-sigma-u-mm <mm>`, `--pdc-sigma-v-mm <mm>`（测量模型 σ，用于 Fisher/MCMC）

**输出（reco.root 的 `recoTree`）**：

| 分支 | 类型 | 含义 |
| --- | --- | --- |
| `reco_target_momentum` | `{px, py, pz}` MeV/c | 点估计 |
| `reco_uncertainty` | `{fisher_σ_{px,py,pz}, mcmc_σ_*, profile_σ_*}` | 三种不确定度 |
| `reco_interval` | `IntervalEstimate` 数组 | 68% / 95% CI（Fisher / MCMC / Profile） |
| `reco_trajectory` | `TVector3` 数组 | RK 步长采样轨迹（用于可视化） |
| `reco_status` | `SolverStatus` 枚举 + 迭代次数 + 最终 χ² | 收敛码 |
| `reco_config_snapshot` | YAML 字符串 | 当次运行的 backend / 参数 / 几何（可复现） |

还会写伴生 CSV：`track_summary.csv`, `bayesian_samples.csv`, `profile_samples.csv`（供 `scripts/analysis/analyze_ensemble_coverage.py` 消费）。

#### 2.4 入口命令

```bash
# 自动挑后端（有 NN 模型用 NN，否则 RK）
bin/reconstruct_target_momentum \
    --input  build/sim/pdc_truth_grid.root \
    --output build/reco/pdc_truth_grid_reco.root \
    --geometry-macro configs/simulation/geometry/3deg_1.15T.mac

# 强制 NN
bin/reconstruct_target_momentum ... --backend nn \
    --nn-model scripts/reconstruction/nn_target_momentum/models/formal/model.json

# 强制 RK（用于 Fisher σ 诊断）
bin/reconstruct_target_momentum ... --backend rk \
    --pdc-sigma-u-mm 2.0 --pdc-sigma-v-mm 2.0

# 批量目录模式
bin/reconstruct_target_momentum \
    --input-dir  build/sim/ensemble_n50/ \
    --output-dir build/reco/ensemble_n50/ \
    --geometry-macro configs/simulation/geometry/3deg_1.15T.mac \
    --max-files 50
```

三种 RK 模式（`RkFitMode`）：
- `kTwoPointBackprop` — 从 PDC 出发向靶点反传，两点约束（简单、快）
- `kFixedTargetPdcOnly` — 固定靶点 z，只让 (px, py, pz) 变，拟合 PDC (u,v) — **主模式**
- `kThreePointFree` — 靶点 + PDC 三点全自由（用于不确定度诊断）

两种参数化（`RkParameterization`）：
- `kDirectionCosines` — (u=px/pz, v=py/pz, p=|p|)，Fisher 计算的默认
- `kCartesian` — (px, py, pz) 直接参数化

#### 2.5 典型改动点

| 想做什么 | 改哪里 |
| --- | --- |
| 加一个新重建算法 | 在 `libs/analysis_pdc_reco/src/` 新建 `PDC<Name>Reconstructor.cc/hh`；在 `PDCRecoFactory` 中注册枚举 |
| 加一种不确定度方法 | 扩 `PDCErrorAnalysis`，在 `reco_uncertainty` 分支里加新字段 |
| 改测量模型（含 dE/dx+MS） | 入口是 `PDCRkAnalysisInternal.cc` 里的 χ² 函数；dE/dx 预留接口已在 `PDCMomentumReconstructorRK` |
| 改 PDC 测量模型（角度/σ） | 改 `RuntimeOptions::pdc_angle_deg` / `pdc_sigma_u_mm` / `pdc_sigma_v_mm` |
| NN 网络结构 | 改 `scripts/reconstruction/nn_target_momentum/train_mlp.py` 的 `MLP` 类；重训练导出新 JSON |
| 磁场 30° 旋转变化 | 改 `RuntimeOptions::magnetic_field_rotation_deg`（默认 30.0）及 `MagneticField::ApplyRotation` |

#### 2.6 常见坑

- **`reco_status` 必须写盘**：曾出现事件级 solver_status 只在内存、不进 reco.root 的 bug，批量分析时无法区分 LM 收敛失败 vs NN 推理异常。新加方法记得 write-through。
- **LM 粗搜盲点**：`kFixedTargetPdcOnly` + `kDirectionCosines` 的 7×3×5 候选网格在 (px=-100, py=0) 附近有盲点，经验上需要 top-5 多起点才能稳定命中。改网格尺寸前先跑 `tests/integration/test_rk_nn_comparison.py`。
- **Fisher σ 欠估计**：Fisher 默认用 (u,v,p) 线性化，对大 |px|、大 |py| 处的真实分布会低估 2–10×（见 `docs/reports/reconstruction/rk_reconstruction_status_20260416.pdf`）。做精度论断前对照 MCMC/Profile。
- **Auto 后端静默切换**：若 `--nn-model` 路径存在但 JSON 损坏，Auto 会静默回落到 RK。建议在生产流水线里**显式** `--backend`。
- **几何宏不匹配**：sim.root 必须用同一个 `*.mac` 生成和重建；否则 PDC 平面 z 值会错，RK 全垮。
- **NN JSON schema**：训练侧必须跑 `export_model_for_cpp.py` 生成 C++ 可读 JSON；直接拿 `.pt` 文件给重建端会 fail。

---

### §3 几何筛选（约 170 行）

#### 3.1 做什么

`bin/run_qmd_geo_filter` 是一个**完全绕过 Geant4** 的快速几何接受率工具。给它一个 QMD 事件文件（含 (px, py, pz) 和 A/Z 的候选产物），它用 RK4 + SAMURAI 磁场 + PDC/NEBULA 几何包络，判定每个事件里的每个产物**能否飞到 PDC / NEBULA 的有效区域**，输出四种分类的接受率。

用途：
- 在跑 Geant4 前快速扫描"这组动量分布值不值得跑"
- 对 IPS target / dipole 角度做参数扫描，找最大接受率配置
- 给理论预言打接受率权重

#### 3.2 组件地图

| 路径 | 角色 |
| --- | --- |
| `libs/qmd_geo_filter/src/RunQMDGeoFilter.cc` | CLI 入口（getopt 解析）|
| `libs/qmd_geo_filter/include/QMDGeoFilter.hh` | 筛选主类；含 `MomentumData`, `RatioResult`, `GeometryFilterResult` 结构 |
| `libs/qmd_geo_filter/src/QMDGeoFilter.cc` | RK4 + 接受率计算主体 |
| `libs/geo_accepentce/include/BeamDeflectionCalculator.hh` | 找给定偏转角度下的靶点 / 跟踪起点（氘核 190 MeV/u，从 `(0, 0, -4000)` mm 沿 +z）|
| `libs/geo_accepentce/include/DetectorAcceptanceCalculator.hh` | PDC / NEBULA 包络判定；`PDCConfiguration` 默认 1680×780×380 mm，`NEBULAConfiguration` 在 `(0,0,5000)`，3600×1800×600 mm |
| `libs/geo_accepentce/include/GeoAcceptanceManager.hh` | 串联 beam deflection + detector acceptance |
| `libs/analysis/include/ParticleTrajectory.hh` | 复用的 RK4（与 §2 同一份实现）|
| `libs/analysis/include/MagneticField.hh` | 复用的磁场（与 §2 同一份）|

#### 3.3 输入与输出

**输入（CLI 部分）**：
- `--qmd <qmd.root>` — QMD 产物的 TTree（事件级，每事件数个产物）
- `--field <T>` — 磁场强度（如 1.15, 1.2）
- `--angle <deg>` — 磁场/偏转角（如 3, 5, 8, 10）
- `--target <x,y,z>` — 靶点位置（默认 (0,0,0)）
- `--pol <value>` — 极化（占位参数）
- `--gamma <value>` — 束流 γ
- `--fieldmap <path>` — 可选，磁场映射文件（不给则用常数场）
- `--fixed-pdc` — 使用固定 PDC 位置（否则按 `--angle` 自动计算）
- `--pdc-x <mm>`, `--pdc-y <mm>`, `--pdc-z <mm>`, `--pdc-angle <deg>` — 固定 PDC 模式下的 PDC 位姿
- `--pdc-macro <geom.mac>` — 从 Geant4 几何宏读取 PDC 位姿（与 §2 的 geometry-macro 相同格式）
- `--px-range <lo,hi>` — 可选 px 截断
- `--output <dir>` — 输出目录

**输出**：
- `<dir>/ratio_result.csv` — 四类计数与比例（`GeometryFilterResult`）：
  - `bothAccepted` — PDC 和 NEBULA 同时命中
  - `pdcOnlyAccepted` — 只命中 PDC
  - `nebulaOnlyAccepted` — 只命中 NEBULA
  - `bothRejected` — 都没中
- `<dir>/trajectories.csv` — 每条轨迹的采样点（可选大文件）
- `<dir>/config_snapshot.yaml` — 运行配置（用于复现）

#### 3.4 入口命令

```bash
# 典型：1.15 T, 3° 偏转，自动 PDC 位置
bin/run_qmd_geo_filter \
    --qmd     data/qmd/imqmd_pb208_190mevu.root \
    --field   1.15 \
    --angle   3 \
    --output  build/filter/imqmd_3deg_115T/

# 与 Geant4 几何对齐：用同一个 geometry macro
bin/run_qmd_geo_filter \
    --qmd      <qmd.root> \
    --field    1.15 --angle 3 \
    --pdc-macro configs/simulation/geometry/3deg_1.15T.mac \
    --output   build/filter/aligned/

# 扫描：shell 驱动
for angle in 3 5 8 10; do
  bin/run_qmd_geo_filter --qmd <qmd.root> --field 1.15 --angle $angle \
      --output build/filter/scan_${angle}deg/
done
```

#### 3.5 典型改动点

| 想做什么 | 改哪里 |
| --- | --- |
| 加第三个探测器（如 VETO） | 新建 `libs/geo_accepentce/include/VetoAcceptanceCalculator.hh`；扩 `GeoAcceptanceManager` 与 `GeometryFilterResult` 分类 |
| 改 PDC / NEBULA 包络 | 改 `DetectorAcceptanceCalculator.hh` 里的 `PDCConfiguration` / `NEBULAConfiguration` 默认值 |
| 换 RK 步长 | 改 `ParticleTrajectory` 的步长策略（与 §2 共享！）|
| 加能量损失 | `QMDGeoFilter::PropagateOne` 里加 dE/dx（目前是纯 Lorentz 力）|
| 加自定义束流入口 | 改 `BeamDeflectionCalculator`（默认氘核 190 MeV/u 从 `(0,0,-4000)` mm 沿 +z）|
| 加新分类 | 扩 `GeometryFilterResult` 结构，改 `QMDGeoFilter::Classify` |

#### 3.6 常见坑

- **`--angle` 与 `--pdc-macro` 同时给**：以 `--pdc-macro` 为准，`--angle` 只用于磁场旋转。如果希望两边一致，要么都给、要么只给 macro。
- **QMD TTree schema**：输入 ROOT 的 branch 名必须和 `QMDGeoFilter::ReadQMD` 里 hardcode 的一致（见源码），否则静默读 0 事件。
- **PDC 包络是"平面框"不是"丝网"**：acceptance 判定只看粒子有没有穿过 PDC 外框（1680×780 mm 的矩形）；真正丝能不能 readout 还要跑 §1/§2。这是"上界"估计。
- **与 Geant4 的一致性**：filter 的 RK4 与 §2 共享实现，但 **NEBULA 默认几何固定在 `(0,0,5000)` mm**，与 Geant4 宏里 NEBULA 位置可能不一致（如果你用 6 m、7 m 等非默认值）。改宏时记得同步 `NEBULAConfiguration::position`。
- **`--fixed-pdc` 下的陷阱**：必须同时给 `--pdc-x/y/z/angle`，否则 PDC 位置是未初始化的默认值，接受率会错得离谱。
- **输出目录不会自动创建**：`--output build/filter/foo/` 如果上级不存在会失败。
- **磁场 30° 旋转**：与 §2 一致，即 SAMURAI 坐标系相对实验室系绕 y 轴旋转 30°。`--angle` 是**偏转角**不是旋转角，别搞混。

---

### §4 分析 & NN 后端（约 140 行）

#### 4.1 做什么

"分析"在本仓库指两类下游消费：

1. **Ensemble coverage / bias 诊断**（纯读 reco.root + sim.root）：
   - 对同一真值点的 N=50 重复事件，算 Fisher/MCMC/Profile 声明的 σ 实际覆盖了 p16–p84 的多少（Clopper-Pearson 95% CI），对比真实 G4 半宽。
   - 画 reco 动量弥散、PDC 命中弥散、per-truth bias / ratio 表。
2. **NN 后端生命周期**（训练 → 导出 → C++ 推理）：
   - 从 sim.root 生成训练集 → PyTorch MLP → 导出 `model_meta.json` + `model.pt` → 再导成纯 JSON 供 C++ 端 `PDCNNMomentumReconstructor::LoadModel` 加载。

#### 4.2 组件地图

| 路径 | 角色 |
| --- | --- |
| `scripts/analysis/analyze_ensemble_coverage.py` | 主覆盖率分析：读 `track_summary.csv` + `bayesian_samples.csv` + `profile_samples.csv`，算 68/95% 覆盖、Clopper-Pearson CI |
| `scripts/analysis/compute_per_truth_g4_vs_fisher.py` | 每个真值点：Fisher σ vs G4 实际 p16-p84 半宽比值 |
| `scripts/analysis/extract_pdc_hits_from_reco.py` | 从 reco.root 抽 PDC 命中 → CSV |
| `scripts/analysis/plot_pdc_hit_dispersion.py` | PDC 命中分布图（5×3 grid + 单点放大）|
| `scripts/analysis/plot_reco_momentum_dispersion.py` | reco 动量分布图（5×3 grid + 单点放大）|
| `scripts/analysis/run_ensemble_coverage.sh` | 一键：sim → reco → coverage |
| `scripts/analysis/batch_analysis.C` / `diagnose_truth_pdc_reco.C` | ROOT 宏层面诊断（兼容历史用法）|
| `scripts/batch/batch_run_ypol.py` | Geant4 批量驱动（ypol 扫描）|
| `scripts/reconstruction/nn_target_momentum/build_dataset.C` | 从 sim.root 抽 `(pdc1/2_xyz, px/py/pz_truth)` → HDF5 |
| `scripts/reconstruction/nn_target_momentum/train_mlp.py` | PyTorch MLP 训练（Linear+ReLU），features=pdc1/2_xyz, targets=px/py/pz_truth |
| `scripts/reconstruction/nn_target_momentum/evaluate_mlp.py` | 留出测试集评估 |
| `scripts/reconstruction/nn_target_momentum/infer_mlp.py` | Python 端推理（sanity check）|
| `scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py` | `.pt` + `model_meta.json` → 纯 JSON（C++ 端消费格式）|
| `scripts/reconstruction/nn_target_momentum/run_pipeline.sh` | 端到端：`run_pipeline.sh <sim.root> <geometry.mac> [out] [max_events]` |
| `scripts/reconstruction/nn_target_momentum/run_formal_training.sh` | 正式训练配置：TRAIN=200k / VAL=30k / TEST=30k, R_MAX=150 MeV/c, PZ ∈ [500, 700] |
| `scripts/reconstruction/nn_target_momentum/run_domain_matched_retrain.sh` | 领域匹配再训练（针对 Fisher 欠估计区）|
| `scripts/reconstruction/nn_target_momentum/generate_tree_input_disk_pz.C` | 训练用动量网格生成（disk in px/py, fixed pz range）|

#### 4.3 输入与输出

**Coverage 分析**：
- 输入：`<reco_dir>/track_summary.csv`, `bayesian_samples.csv`, `profile_samples.csv`（由 §2 的重建运行产生）
- 输出：`<out>/coverage_summary.csv`, `<out>/per_truth_coverage.csv`, 配套 PNG 图

**NN 训练流水线**：
- 输入：sim.root（必须已跑重建得到 PDC 命中）或直接从 FragSimData 抽特征
- 特征：`FEATURE_NAMES = ["pdc1_x", "pdc1_y", "pdc1_z", "pdc2_x", "pdc2_y", "pdc2_z"]`
- 目标：`TARGET_NAMES = ["px_truth", "py_truth", "pz_truth"]`
- 训练产物：
  - `models/<tag>/model.pt` — PyTorch state dict
  - `models/<tag>/model_meta.json` — 层结构、归一化常数、训练配置
  - `models/<tag>/model.json` — C++ 可读 JSON（`export_model_for_cpp.py` 产出）
  - `models/<tag>/train_history.csv` — 每 epoch loss

#### 4.4 入口命令

```bash
# Coverage 全流程（建议）
bash scripts/analysis/run_ensemble_coverage.sh  configs/simulation/geometry/3deg_1.15T.mac

# 只跑 coverage 分析（reco 已有）
micromamba run -n anaroot-env python scripts/analysis/analyze_ensemble_coverage.py \
    --reco-dir build/reco/ensemble_n50/ \
    --out-dir  build/analysis/ensemble_n50/

# per-truth G4 vs Fisher σ 对比表
micromamba run -n anaroot-env python scripts/analysis/compute_per_truth_g4_vs_fisher.py \
    --merged build/reco/ensemble_n50/ensemble_pdc_reco_merged.csv

# NN 端到端
bash scripts/reconstruction/nn_target_momentum/run_pipeline.sh \
    build/sim/nn_train.root \
    configs/simulation/geometry/3deg_1.15T.mac \
    build/nn_models/exp42 \
    50000

# NN 正式训练（大数据集）
bash scripts/reconstruction/nn_target_momentum/run_formal_training.sh

# 只导出（已有 .pt）
micromamba run -n anaroot-env python \
    scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py \
    --model-dir build/nn_models/exp42
```

#### 4.5 典型改动点

| 想做什么 | 改哪里 |
| --- | --- |
| 加一种新诊断图 | 新建 `scripts/analysis/plot_<name>.py`；数据源优先复用 `track_summary.csv` |
| 改 coverage 的置信水平 | `analyze_ensemble_coverage.py` 里的 `Z_68=1.0`, `Z_95=1.96`, Clopper-Pearson α |
| 改 NN 网络结构 | `train_mlp.py` 的 `MLP` 类；改后必须重跑 `export_model_for_cpp.py` 并更新 `model_meta.json` |
| 加新特征 | 改 `FEATURE_NAMES` 和 `build_dataset.C` 的抽取逻辑；同步 `PDCNNMomentumReconstructor::BuildInput` |
| 改训练集动量范围 | `run_formal_training.sh` 的 `R_MAX_MEVC` / `PZ_MIN` / `PZ_MAX`；同步 `generate_tree_input_disk_pz.C` |
| 加领域匹配重训 | 参考 `run_domain_matched_retrain.sh`（冻结前 N 层 + 小学习率 fine-tune）|

#### 4.6 常见坑

- **训练集覆盖**：训练动量范围 (R_MAX=150 MeV/c, pz∈[500,700]) 之外的预测会退化；coverage 问题的一部分根因是训练分布与测试分布不一致。
- **Feature/Target 顺序**：C++ 端读 JSON 时按 `FEATURE_NAMES` 和 `TARGET_NAMES` 的**顺序**喂值，改名字而不改 `BuildInput` 会静默错位。
- **归一化**：训练时做了 mean/std 标准化，常数写在 `model_meta.json`，C++ 端必须同样应用——忘加 = 推理结果全错。
- **micromamba env**：所有 Python 分析脚本都假设 `anaroot-env` 已激活（含 PyROOT、PyTorch、numpy）；`micromamba run -n anaroot-env python ...` 是安全写法。
- **N=50 ensemble 体积**：sim.root 可到数百 MB，reco.root 数 MB，merged CSV 约 MB 量级；对于全套 5×3 网格 × 50 事件，跑一次要 20–60 分钟（取决于 physics list）。
- **`run_pipeline.sh` 用的是新 sim.root**：它不会复用已有的 reco.root；如果只想更新 NN 不想重跑 Geant4，直接走 `build_dataset.C` + `train_mlp.py`。

---

### §A 附录（约 40 行）

#### A.1 FragSimData 分支字段（sim.root）

```
FragSimData : TClonesArray<TSimData>
  - pos[3]        (Double_t, mm)  全局位置
  - mom[3]        (Double_t, MeV/c) 动量
  - track_id      (Int_t)          Geant4 track ID
  - parent_id     (Int_t)          父 track ID
  - detector_id   (Int_t)          1=PDC1, 2=PDC2（枚举见 FragmentSD.cc）
  - edep          (Double_t, MeV)  能量沉积
  - time          (Double_t, ns)   相对事件开始
  - pdg           (Int_t)          PDG code（2212 质子, 2112 中子, 1000010020 氘核, ...）
```

#### A.2 reco.root 分支

见 §2.3 表；完整 schema 参考 `libs/analysis/include/RecoEvent.hh`。

#### A.3 CMake 选项完整列表

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `CMAKE_BUILD_TYPE` | Release | Release / Debug / RelWithDebInfo |
| `BUILD_TESTS` | ON | GoogleTest + CTest 单元测试 |
| `BUILD_APPS` | ON | `bin/` 下的所有可执行 |
| `BUILD_ANALYSIS` | ON | `libs/analysis*` 全家桶 |
| `WITH_ANAROOT` | ON | 需 `TARTSYS`；OFF 时跳过实验数据读盘 |
| `WITH_GEANT4_UIVIS` | ON | Geant4 UI/Vis 驱动（visualization test 需要）|

#### A.4 环境变量

| 变量 | 作用 |
| --- | --- |
| `SM_LOG_LEVEL` | TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL |
| `SM_BATCH_MODE` | 1=批量日志 |
| `SM_TEST_VISUALIZATION` | ON 时启用可视化测试 |
| `TARTSYS` | ANAROOT 根目录 |

#### A.5 测试标签

| label | 意图 |
| --- | --- |
| `unit` | 纯单元（< 1s/个） |
| `performance` | 性能回归 |
| `visualization` | 需要 `SM_TEST_VISUALIZATION=ON` |
| `realdata` | 需要真实数据 |

#### A.6 相关报告

- `docs/reports/reconstruction/rk_reconstruction_status_20260416.pdf`（EN）/ `_zh.pdf`（ZH）— 当前 Fisher σ 欠估计诊断
- `docs/reports/imqmd_analysis/` — IMQMD 几何筛选结果
- `docs/imqmd_rawdata_analysis_guide.md` — IMQMD 原始数据指南

---

## 验证

1. **README.md**：新文件行数应 ≤ 120，且必须包含三条数据流图、四个子系统速览表、快速上手代码块。
2. **docs/SUBSYSTEMS.md**：新文件行数 500–700，每个 § 都能独立读懂（不依赖 README 以外的知识）。
3. **链接检查**：README 里指向 docs/SUBSYSTEMS.md 的锚点（`#1-geant4-模拟` 等）必须都跳得动（GitHub 渲染后）。
4. **文件路径正确性**：所有组件地图表里的路径都存在（用 `ls` 逐一验证）。
5. **命令可跑**：README 和 SUBSYSTEMS.md 里给的示例命令，能在 `anaroot-env` 激活后直接跑（不要求产出正确，只要求 ParseArgs 不报错）。

## 超出本次范围

- **不重写** `CLAUDE.md`（本地文件，已从 git 移除）
- **不重写** `skills/` 内的 SKILL.md（那是 Claude Code 用的深度文档，另一条线）
- **不动** `README.md` 外部链接到的 anaroot/ 等子仓库文档
- **不写英文版**（本次只做中文，英文版后续再说）
