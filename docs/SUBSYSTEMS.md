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
