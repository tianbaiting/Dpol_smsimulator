# SMSimulator 子系统深度文档

> 首次阅读请先看 [README.md](../README.md) 的"三条数据流"。本文对每个子系统展开：做什么、组件地图、I/O、入口命令、典型改动点、常见坑。

## 目录

- [§0 全局数据流](#0-全局数据流)
- [§1 Geant4 模拟](#1-geant4-模拟)
- [§2 ROOT 重建](#2-root-重建)
- [§3 几何筛选](#3-几何筛选)
- [§4 分析 & NN 后端](#4-分析--nn-后端)
- [§A 附录](#a-附录)

---

## §0 全局数据流

三条主干：

```
┌─────────────────────────────────────────────────────────────────┐
│  [A] 几何筛选（side path）                                       │
│      RunQMDGeoFilter → Acceptance CSV                            │
│      跳过 Geant4，纯 RK4 + 磁场 + 探测器包络                      │
│                                                                 │
│  [B] Geant4 模拟（main path）                                    │
│      sim_deuteron + .mac → sim.root                              │
│        ├ beam          : TClonesArray<TBeamSimData>              │
│        ├ FragSimData   : TClonesArray<TSimData>   ← 主分支       │
│        └ NEBULASimData : TClonesArray<TSimData>                  │
│                                                                 │
│  [C] ROOT 重建（main path）                                      │
│      reconstruct_target_momentum → reco.root                     │
│        ├ reco_target_momentum  (px, py, pz)                      │
│        ├ reco_uncertainty      (fisher/mcmc/profile σ)           │
│        ├ reco_interval         (68/95% CI)                       │
│        ├ reco_trajectory       (RK 步长数组)                      │
│        ├ reco_status           (收敛码 + χ²)                     │
│        └ reco_config_snapshot  (后端/参数/几何，可复现)           │
│                                                                 │
│  [D] 分析 & NN 后端                                              │
│      scripts/analysis/*.py                                       │
│      scripts/reconstruction/nn_target_momentum/                  │
│      ensemble coverage, per-truth σ, 模型训练→.pt→JSON          │
└─────────────────────────────────────────────────────────────────┘
```

产物体量（经验值）：

| 产物 | 典型体积 | 生成时间 |
| --- | --- | --- |
| sim.root（N=50 ensemble，5×3 网格） | 数百 MB | 20–60 min（取决于 physics list） |
| reco.root | 数 MB | 1–5 min |
| filter CSV | KB 量级 | 秒级 |

**坐标系约定：** SAMURAI 坐标系相对实验室系绕 y 轴旋转 30°。`sim.root/FragSimData::pos[3]` 是**全局坐标**（mm）。重建与几何筛选共享同一套磁场 / 轨迹积分代码（`libs/analysis/include/{MagneticField,ParticleTrajectory}.hh`）。

---

## §1 Geant4 模拟

### 1.1 做什么

`sim_deuteron` 是 Geant4 主程序。读 `.mac` 宏，构造 SAMURAI/DPOL 探测器（磁铁、PDC1/PDC2、NEBULA、靶），把入射氘核送进去，写事件级 ROOT TTree。

典型场景：
- 网格化 (px, py, pz) 真值点 × 1 事件/点 → `pdc_truth_grid_*.root`（用于 RK 重建验证）
- 同一真值点 × 50 次重复 → `ensemble_n50/sim.root`（用于 Fisher σ coverage）
- IPS target 位置扫描 → IPS veto 几何优化

### 1.2 组件地图

| 路径 | 角色 |
| --- | --- |
| `apps/sim_deuteron/main.cc` | 入口；装配 `DeutDetectorConstruction` + `DeutPrimaryGeneratorAction` + `DeutSteppingAction` + `DeutTrackingAction` |
| `libs/sim_deuteron_core/` | 氘核实验的探测器/源/步进动作（业务层） |
| `libs/smg4lib/include/DipoleConstruction.hh` | SAMURAI 磁铁几何 |
| `libs/smg4lib/include/PDCConstruction.hh` | PDC1/PDC2 漂移室几何 + Sensitive Detector |
| `libs/smg4lib/include/NEBULAConstruction.hh` | NEBULA 中子墙几何 |
| `libs/smg4lib/src/data/src/FragSimDataInitializer.cc` | 定义 `FragSimData` 分支（`TClonesArray<TSimData>`） |
| `libs/smg4lib/src/data/src/BeamSimDataInitializer.cc` | 定义 `beam` 分支 |
| `libs/smg4lib/include/MagField.hh` + `configs/simulation/geometry/filed_map/` | 磁场（常数或映射） |
| `libs/smg4lib/include/QGSP_BIC_XS.hh` / `QGSP_INCLXX_*` / `QGSP_MENATE_R` | Physics list 工厂 |
| `configs/simulation/macros/` | 运行宏：`simulation.mac`, `simulation_vis.mac`, `test_pencil.mac` 等 |
| `configs/simulation/geometry/*.mac` | 几何宏：`3deg_1.15T.mac`, `5deg_1.2T.mac` 等（角度 + 场强） |
| `configs/simulation/geometry/SAMURAIPDC.xml` | PDC 丝参数 |
| `configs/simulation/geometry/NEBULA_Dayone.csv` | NEBULA 模块表 |
| `scripts/event_generator/` | 束流输入文件生成（momentum grid、disk_pz 等） |

### 1.3 输入与输出

**输入：**
- 一个 `.mac`（如 `configs/simulation/macros/simulation.mac`），含 `/run/beamOn`、`/gps/...`、`/SMG4/physics/...`、`/SMG4/geometry/...`
- 可选：束流 ASCII 文件（`/SMG4/primary/inputFile ...`），每行一个 (px, py, pz) 真值
- 可选：`filed_map/` 下的磁场映射

**输出：** `<tag>_sim.root`，含三个 TTree 分支：

| 分支 | 类型 | 关键字段 | 含义 |
| --- | --- | --- | --- |
| `beam` | `TClonesArray<TBeamSimData>` | `px, py, pz, Ek, event_index` | 入射束流真值（每事件 1 条） |
| `FragSimData` | `TClonesArray<TSimData>` | `pos[3], mom[3], track_id, parent_id, detector_id, edep, time, pdg` | PDC 平面上的碎片命中（主分支） |
| `NEBULASimData` | `TClonesArray<TSimData>` | 同上 | NEBULA 模块中子命中 |

`TSimData::detector_id` 区分 PDC1 / PDC2 / NEBULA bar；`pos[3]` 是全局坐标 (mm)；`mom[3]` 是动量 (MeV/c)。

### 1.4 入口命令

```bash
bin/sim_deuteron configs/simulation/macros/simulation.mac
bin/sim_deuteron configs/simulation/macros/simulation_vis.mac   # 带可视化
python scripts/batch/batch_run_ypol.py --config configs/simulation/macros/simulation.mac --n-jobs 8
```

宏内常用命令：
- `/SMG4/geometry/fieldAngle 3.0 deg`
- `/SMG4/geometry/fieldStrength 1.15 tesla`
- `/SMG4/physics/list QGSP_INCLXX_XS`
- `/SMG4/primary/inputFile data/input/grid_5x3.txt`

### 1.5 典型改动点

| 想做什么 | 改哪里 |
| --- | --- |
| 加新探测器 | `libs/smg4lib/include/<Name>Construction.hh` + `<Name>SD.hh`；`DeutDetectorConstruction` 注册；`FragSimDataInitializer` 或新 Initializer 定义分支 |
| 换磁场映射 | 新 map 放 `configs/simulation/geometry/filed_map/`；宏里 `/SMG4/field/mapFile <path>` |
| 换 physics list | 宏里 `/SMG4/physics/list <name>`；可选列表见 `libs/smg4lib/include/QGSP_*.hh` |
| 换束流源 | 改 `DeutPrimaryGeneratorAction`，或 `/gps/` 命令，或 ASCII 文件 |
| 改 PDC 丝距/位置 | `configs/simulation/geometry/SAMURAIPDC.xml` + `PDCConstruction.hh` 解析逻辑 |
| 加丝数字化 | `FragmentSD.hh`/`FragmentSD.cc`，或新 stepping action |

### 1.6 常见坑

- **GDML 路径**：`configs/simulation/geometry/filed_map/` 与 `d_work/geometry/filed_map/` 都可能被 hardcode 引用，迁移时两边都要查。
- **PDC 丝坐标不是 (x,y)**：u/v 丝 ±57° 旋转；`FragSimData::pos[3]` 写**全局 (x,y,z)**，不要把丝坐标存进去。
- **Physics list 对中子影响大**：`QGSP_BIC_XS` / `QGSP_INCLXX_XS` / `QGSP_MENATE_R` 对 NEBULA 中子产额/能谱差很多，验收前固定一个。
- **`TARTSYS` / ANAROOT**：`WITH_ANAROOT=ON` 是默认；若环境无 `TARTSYS`，要么 `source thisroot.sh; export TARTSYS=...`，要么 `cmake -DWITH_ANAROOT=OFF`。
- **束流输入坐标系**：ASCII 的 (px, py, pz) 是**靶点动量**，不是实验室系。

---

## §2 ROOT 重建

### 2.1 做什么

读 sim.root 的 `FragSimData`，把 PDC1 / PDC2 两平面命中解成轨迹，反推**靶点动量** (px, py, pz)。产出 `reco.root`：点估计 + 不确定度（Fisher / Laplace MCMC / Profile）+ 收敛状态 + 轨迹采样。

`libs/analysis_pdc_reco/` 是主框架。`libs/analysis/include/TargetReconstructor.hh` 是遗留兼容层，**不要**在那里加新特性。

### 2.2 组件地图

| 路径 | 角色 |
| --- | --- |
| `apps/run_reconstruction/main.cc` | CLI（ParseArgs + runtime 装配） |
| `libs/analysis_pdc_reco/src/PDCRecoFactory.cc` | 按 `RuntimeBackend`（Auto/NN/RK/MultiDim/Legacy）装配 reconstructor |
| `libs/analysis_pdc_reco/src/PDCRecoRuntime.cc` | 顶层 runtime：读 sim.root、迭代、写 reco.root |
| `libs/analysis_pdc_reco/src/PDCMomentumReconstructorRK.cc` | **主 RK 重建器**：3 种 fit mode × 2 种 parameterization + LM |
| `libs/analysis_pdc_reco/src/PDCRkAnalysisInternal.cc` | LM 粗搜（7×3×5=105 候选）+ top-5 多起点 + dE/dx/MS 接口 |
| `libs/analysis_pdc_reco/src/PDCErrorAnalysis.cc` | Fisher / Laplace MCMC / Profile 三方法 |
| `libs/analysis_pdc_reco/src/PDCNNMomentumReconstructor.cc` | NN 后端推理（读 JSON） |
| `libs/analysis/src/PDCSimAna.cc` | sim.root → `PDCInputTrack`（u/v 丝坐标 + z 平面） |
| `libs/analysis_pdc_reco/include/PDCRecoTypes.hh` | `SolverStatus`, `SolveMethod`, `RkFitMode`, `RkParameterization`, `PDCInputTrack`, `IntervalEstimate` |
| `libs/analysis/include/GeometryManager.hh` | 几何宏解析（PDC 位置/角度、磁场角度/强度） |
| `libs/analysis/include/MagneticField.hh` | 常数场或映射场（30° 旋转） |
| `libs/analysis/include/ParticleTrajectory.hh` | RK4 积分 |
| `libs/analysis/include/EventDataReader.hh` | sim.root 读盘封装 |
| `libs/analysis/include/RecoEvent.hh` | reco.root 写盘封装 |

### 2.3 输入与输出

**输入（CLI，见 `apps/run_reconstruction/main.cc::ParseArgs`）：**
- `--input <sim.root>` 或 `--input-dir <dir>`
- `--output <reco.root>` 或 `--output-dir <dir>`
- `--geometry-macro <geom.mac>`（**必填**）
- `--backend {auto|nn|rk|multidim|legacy}`（默认 `auto`）
- `--nn-model <model.json>`（backend=nn 时必填）
- `--magnetic-field <T>`（覆盖几何宏）
- `--max-files <N>`, `--max-iterations <N>`
- `--pdc-sigma-u-mm <mm>`, `--pdc-sigma-v-mm <mm>`（测量模型 σ）

**输出（reco.root → `recoTree`）：**

| 分支 | 含义 |
| --- | --- |
| `reco_target_momentum` | `{px, py, pz}` MeV/c |
| `reco_uncertainty` | fisher/mcmc/profile σ |
| `reco_interval` | 68/95% CI |
| `reco_trajectory` | RK 步长采样 |
| `reco_status` | `SolverStatus` + 迭代次数 + χ² |
| `reco_config_snapshot` | YAML：backend / 参数 / 几何（可复现） |

伴生 CSV：`track_summary.csv`, `bayesian_samples.csv`, `profile_samples.csv`（供 §4 分析消费）。

### 2.4 入口命令

```bash
# Auto（有 NN 模型用 NN，否则 RK）
bin/reconstruct_target_momentum \
    --input  build/sim/pdc_truth_grid.root \
    --output build/reco/pdc_truth_grid_reco.root \
    --geometry-macro configs/simulation/geometry/3deg_1.15T.mac

# 强制 NN
bin/reconstruct_target_momentum ... --backend nn \
    --nn-model scripts/reconstruction/nn_target_momentum/models/formal/model.json

# 强制 RK + Fisher σ 诊断
bin/reconstruct_target_momentum ... --backend rk \
    --pdc-sigma-u-mm 2.0 --pdc-sigma-v-mm 2.0

# 批量
bin/reconstruct_target_momentum \
    --input-dir  build/sim/ensemble_n50/ \
    --output-dir build/reco/ensemble_n50/ \
    --geometry-macro configs/simulation/geometry/3deg_1.15T.mac \
    --max-files 50
```

三种 `RkFitMode`：
- `kTwoPointBackprop` — PDC 反传至靶点（简单、快）
- `kFixedTargetPdcOnly` — 固定靶点 z，拟合 PDC (u,v)（**主模式**）
- `kThreePointFree` — 靶点 + PDC 三点全自由（用于不确定度诊断）

两种 `RkParameterization`：
- `kDirectionCosines` — (u=px/pz, v=py/pz, p=|p|)，Fisher 默认
- `kCartesian` — (px, py, pz) 直接参数化

### 2.5 典型改动点

| 想做什么 | 改哪里 |
| --- | --- |
| 加新重建算法 | `libs/analysis_pdc_reco/src/PDC<Name>Reconstructor.cc/hh`；`PDCRecoFactory` 注册枚举 |
| 加新不确定度方法 | 扩 `PDCErrorAnalysis`；在 `reco_uncertainty` 分支加字段 |
| 改测量模型（dE/dx+MS） | `PDCRkAnalysisInternal.cc` 的 χ² 函数；`PDCMomentumReconstructorRK` 已预留接口 |
| 改 PDC 测量模型 | `RuntimeOptions::pdc_angle_deg` / `pdc_sigma_u_mm` / `pdc_sigma_v_mm` |
| NN 网络结构 | `scripts/reconstruction/nn_target_momentum/train_mlp.py` 的 `MLP` 类；重训练 + 导出 |
| 磁场旋转 | `RuntimeOptions::magnetic_field_rotation_deg`（默认 30.0） + `MagneticField::ApplyRotation` |

### 2.6 常见坑

- **`reco_status` 必须写盘**：solver_status 曾只在内存、不入 reco.root，批量分析时分不清 LM 失败 vs NN 异常。新方法一定 write-through。
- **LM 粗搜盲点**：`kFixedTargetPdcOnly` + `kDirectionCosines` 的 7×3×5 网格在 (px=-100, py=0) 附近有盲点，top-5 多起点才能稳。改网格前跑 `tests/integration/test_rk_nn_comparison.py`。
- **Fisher σ 欠估计**：(u,v,p) 线性化对大 |px|、大 |py| 会低估 2–10×（见 `docs/reports/reconstruction/rk_reconstruction_status_20260416.pdf`）。精度论断前对照 MCMC/Profile。
- **Auto 静默切换**：若 `--nn-model` 路径存在但 JSON 损坏，Auto 静默回落 RK。生产流水线**显式**指定 `--backend`。
- **几何宏不匹配**：sim.root 与重建必须用同一个 `.mac`；否则 PDC 平面 z 错，RK 全垮。
- **NN JSON schema**：训练侧必跑 `export_model_for_cpp.py`；直接给 `.pt` 会 fail。
