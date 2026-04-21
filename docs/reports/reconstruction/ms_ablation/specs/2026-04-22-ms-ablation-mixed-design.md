# MS 消融实验 v2 · Beam-line 真空（stage A′） — 设计文档

- **日期**: 2026-04-22
- **作者**: TBT
- **状态**: 已通过 brainstorm，待生成实现计划
- **上游输入**:
  - Stage A memo: [`../ms_ablation_air_20260421_zh.md`](../ms_ablation_air_20260421_zh.md)
  - RK 状态报告：`docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.pdf` §7.5

---

## 1. 背景与动机

Stage A（2026-04-21）发现关掉 Geant4 世界空气后，PDC2 σ_y 从 11–15 mm 塌到 ~1 mm，air 贡献 91–94%，判决"air 主导 → 进入 RK process-noise 修复"。

**小问题被发现**：stage A 的 baseline 几何实际上**不完全物理**。查 Geant4 源码（`libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`、`libs/smg4lib/src/devices/ExitWindowC2Construction.cc`、`libs/smg4lib/src/construction/src/{Dipole,VacuumDownstream,VacuumUpstream}Construction.cc`）后结果如下：

| 区域 | 当前 logical volume 与材质 |
|---|---|
| World（世界体） | `fFillAir ? G4_AIR : G4_Galactic` — 受 Messenger 控制 ✓ |
| SAMURAI 偶极磁铁 yoke 内腔（vchamber 真空室） | **无独立 logical volume**，仅是从 yoke 里挖掉的 void → **继承 world 材质** ✗ |
| Target → dipole 入射面 之间（约 z=−1070 → z=−1750 mm）的上游束流区 | `VacuumUpstreamConstruction` 类**已存在**，但**未被实例化/拼装**到 `DeutDetectorConstruction` 中（hh 第 101 行注释掉） → **继承 world 材质** ✗ |
| Dipole 出射 → ExitWindowC2 flange（~1.45 m 下游联通管） | `VacuumDownstreamConstruction` **已拼装**（`DeutDetectorConstruction.cc:191-199`），恒为 `G4_Galactic` ✓ |
| ExitWindowC2 中心孔（flange 中间的 hole） | `ExitWindowC2Hole_log`（`ExitWindowC2Construction.cc:123`）**已独立**，恒为 `G4_Galactic` ✓ |
| ExitWindowC2 出射面 → PDC1 → PDC2（~1.8 m） | world 材质 → 实验真实的空气段 ✓ |

**结论**：问题仅有两段——(i) target→dipole 入射面 的上游约 **0.68 m**（target at z=−1069.458 mm → yoke 入射面 z=−1750 mm），(ii) dipole yoke 内腔（含 vchamber + trap extension），沿弯曲路径约 **0.95 m** 弧长。下游 1.45 m 真空联通管和 flange 中心孔**已经正确**（stage A 起就一直是真空）。

**动机**：

1. Stage A 的 "94% 空气贡献" 中，真正的空气贡献是上游上述 ~1.63 m 不该存在的 "虚假空气" + 下游约 **1.80 m** 真实空气。对 stage D 的 process-noise 幅度校准**不精确**。
2. 需要一个**物理上真实的** baseline（world 空气 + 上游束流段真空），作为 stage D 过程噪声幅度的直接输入。

**Stage A 并不作废**：它证明了 "world 切 FillAir 是有效的 MS 消融开关"，且本次代码审查发现的上游 vacuum-missing 问题直接解释了 stage A "air 占 94%" 偏大的原因。Stage A′ 在它之上补一个物理真实的中间条件。

## 2. 研究问题

**Q1**：下游约 1.80 m 真实空气路径（ExitWindowC2 出射 → PDC2）对 PDC2 σ_y 单独贡献多少？

**Q2**：stage A 测到的 Δσ_y(all_air − all_vacuum ≈ 10–14 mm) 里，多少来自**上游虚假空气**（target→dipole 入射约 0.68 m + dipole yoke 内腔约 0.95 m ≈ 1.63 m，应为真空），多少来自**下游真实空气**（约 1.80 m，物理存在）？

**Q3**：在物理真实的 `beamline_vacuum` 条件下，PDC2 σ_y 仍然远大于全真空残余（~1 mm）吗？若是 → stage D process-noise 修复仍然必要，且有了准确的 σ_MS 幅度。

## 3. 三种情形对照

全部 3 truth points × N=50 seeds，同一 SEED_A/SEED_B，同一 build，严格对照：

| 情形 | `FillAir` | `BeamLineVacuum` | Target→dipole 入射 (~0.68 m) | dipole yoke 内腔 (~0.95 m) | Dipole→EW (~1.45 m) | EW 中心孔 | EW→PDC2 (~1.80 m) | 质子穿空气总长 | 物理真实性 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|---|
| `all_air`（= stage A baseline） | true | false | 空气 | 空气 | 真空（已存在） | 真空（已存在） | 空气 | **~3.43 m** | 不物理 |
| `beamline_vacuum`（**新**，stage A′） | true | **true** | **真空** | **真空** | 真空 | 真空 | 空气 | **~1.80 m** | **实验真实** |
| `all_vacuum`（= stage A no_air） | false | 无关 | 真空 | 真空 | 真空 | 真空 | 真空 | **0 m** | 参考上限，实验不可达 |

Highland 预测（σ_θ ∝ √L）：`beamline_vacuum` 的 σ_y(PDC2) 应为 `all_air` 的 √(1.80/3.43) ≈ 0.72 倍 → ~8.3–11.0 mm。相较 stage A 原先 "94% 都是空气" 的结论，新 baseline 下空气贡献会下降到大致 60–80% 量级。

## 4. 飞行距离表

基于当前几何源码（target=(−1.25, 0, −106.95) cm，yoke 6.70×4.64×3.50 m 且中心在 (0,0,0)，dipole 30° 弯转，PDC1=(70,0,400) cm、PDC2=(70,0,500) cm 于 PDC 转 69° 前坐标系）以及 1.15 T 下 627 MeV/c proton 弯曲半径 ρ = p/(0.3 B) ≈ 1.82 m：

| 路段 | 长度 | 物理上的材质 | 当前 G4 中的材质（FillAir=true） |
|---|---:|---|---|
| Target (z=−1069.458 mm) → dipole yoke 入射面 (z=−1750 mm) | **~0.68 m** | 真空（beam-line 上游束流管） | **空气（✗ 虚假）** |
| dipole yoke 内腔（沿 30° 弯曲弧长） | **~0.95 m** | 真空（vchamber + trap extension） | **空气（✗ 虚假）** |
| dipole 出射 → ExitWindowC2 flange 面 | **~1.45 m** | 真空（VacuumDownstream，已拼装） | 真空（✓） |
| ExitWindowC2 中心孔（flange 30 mm 厚） | 0.03 m | 真空（ExitWindowC2Hole_log） | 真空（✓） |
| ExitWindowC2 出射面 → PDC1 | **~0.80 m** | 空气（实验室大气） | 空气（✓） |
| PDC1 → PDC2 | **~1.00 m** | 空气（实验室大气） | 空气（✓） |
| **Target → PDC2 总长** | **~4.91 m** | 真空 3.11 m + 空气 1.80 m | 真空 1.48 m + 空气 3.43 m |

> 注：数字为几何近似。精确值依赖具体 dipole field map 积分和 PDC 69° 转矩阵，memo 中给出实测路径（用 G4 SteppingAction 追踪）。

## 5. 架构

### 5.1 C++ 改动（Approach 1）

新增一个正交开关 `/samurai/geometry/BeamLineVacuum true|false`，默认 `false`（保留既有行为 bit-for-bit，即 stage A 的 baseline 仍是 stage A 的 baseline）。启用后：

| 动作 | 源文件 | 内容 | 既存资产 |
|---|---|---|---|
| **A. 复活 VacuumUpstream 装配** | `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc` 与 `include/DeutDetectorConstruction.hh` | 取消注释 `fVacuumUpstreamConstruction` 前向声明与成员；`new VacuumUpstreamConstruction()` ctor / `delete` dtor；在 Construct() 中`if (fBeamLineVacuum)` 分支里调 `ConstructSub(Dipole_phys)` 并 `G4PVPlacement` 摆到 `expHall_log` 原点。 | `VacuumUpstreamConstruction` 类**已存在**（`libs/smg4lib/src/construction/{include,src}/VacuumUpstreamConstruction.{hh,cc}`），`ConstructSub` 已实现（1200×1000×5000 mm³，-z 方向延伸，dipole solid 扣除），只缺拼装。 |
| **B. 新增 Dipole 内腔真空 logical volume** | `libs/smg4lib/src/construction/src/DipoleConstruction.cc` 与 `include/DipoleConstruction.hh` | `ConstructSub()` 里构造 yoke 同时，额外做一个 `G4UnionSolid("vchamber_cavity", vchamber_box, vchamber_trap, vct_rm, *vct_trans)`（与 yoke subtraction 用同一组 solid+transform），其材质受新布尔 `fBeamLineVacuum` 控制（true → `G4_Galactic`，false → `fWorldMaterial` 即 world 同款）；挂到 yoke 里用 `G4PVPlacement(nullptr, {0,0,0}, cavity_log, "vchamber_cavity", fLogicDipole, false, 0)`。 | 已有 `vchamber_box`、`vchamber_trap` 变量。本次增加一个 setter `SetBeamLineVacuum(G4bool)` 和成员 `fBeamLineVacuum`，DeutDetectorConstruction 在 `fDipoleConstruction->ConstructSub()` 前调 setter 传入。 |
| **C. Messenger 新命令** | `libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc` 与 `.hh` | 参考现有 `fFillAirCmd` 模板新增 `fBeamLineVacuumCmd = new G4UIcmdWithABool("/samurai/geometry/BeamLineVacuum", this)`，默认 `false`，`SetNewValue` 分支里调 `fDetectorConstruction->SetBeamLineVacuum(...)`。 | 完全照抄 `fFillAirCmd` 结构，零风险。 |
| **D. DeutDetectorConstruction 新增 setter** | `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc` 与 `.hh` | `fBeamLineVacuum` 成员 + `SetBeamLineVacuum(G4bool tf) { fBeamLineVacuum = tf; }` + `IsBeamLineVacuum() const` 供两处读取（Dipole setter 和 VacuumUpstream 的 if 分支）。 | — |

**无回归风险保证**：`BeamLineVacuum` 默认 `false` → 
- vchamber_cavity logical volume 材质 = `fWorldMaterial`（world 同款，即空气或真空随 FillAir），该区域在 Geant4 中的有效物理跟"没放这个 logical volume"**bit-for-bit 一致**。
- VacuumUpstream 分支完全不进入，不放置任何物理体。
- 全部既存 ctest / smoke 测试默认全绿。

### 5.2 数据流

```
geometry_*.mac (FillAir, BeamLineVacuum 组合)
        ↓ sim_deuteron (既有二进制, 不动)
reco_<seed>.root × 50 × 3 truth points × 3 条件 = 450 events
        ↓ compute_metrics.py (既有, 条件无关)
pdc_hits.csv + dispersion_summary.csv (3 份)
        ↓ compare_ablation.py (扩展到 3-way)
compare.csv (9 行) + 3 张 3-color overlay PNG + stage A′ memo
```

## 6. 文件清单

### 6.1 新建

| 文件 | 内容 |
|---|---|
| `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac` | 复制 `_target3deg.mac` + 追加 `/samurai/geometry/BeamLineVacuum true`（`FillAir` 仍保留 `true`） |
| `scripts/analysis/ms_ablation/tests/test_beamline_vacuum_materials.sh` | 跑一个 minimal smoke（1 event），解析 Geant4 材质打印，断言 `BeamLineVacuum=true` 下 `vchamber_cavity` 与 `vacuum_upstream_log` 材质名为 `G4_Galactic`；`BeamLineVacuum=false` 下二者不出现或为 world 材质。 |
| `docs/reports/reconstruction/ms_ablation/specs/2026-04-22-ms-ablation-mixed-design.md` | 本文档 |

### 6.2 修改

| 文件 | 变动 |
|---|---|
| `libs/sim_deuteron_core/include/DeutDetectorConstruction.hh` | 取消第 13 行附近注释，复活 `VacuumUpstreamConstruction` 前向声明与第 101 行的成员指针；新增 `fBeamLineVacuum` 成员 + `SetBeamLineVacuum`/`IsBeamLineVacuum`。 |
| `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc` | Ctor: `new VacuumUpstreamConstruction()`；Dtor: delete；Construct(): 在 `fDipoleConstruction->ConstructSub()` 前调 `fDipoleConstruction->SetBeamLineVacuum(fBeamLineVacuum)`；紧跟 VacuumDownstream 拼装后加 `if (fBeamLineVacuum)` 分支 `ConstructSub(Dipole_phys)` 并 place。 |
| `libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc` 与 `.hh` | 新 `fBeamLineVacuumCmd` 命令（抄 `fFillAirCmd` 结构）。 |
| `libs/smg4lib/src/construction/include/DipoleConstruction.hh` | `fBeamLineVacuum` 成员 + `SetBeamLineVacuum` setter。 |
| `libs/smg4lib/src/construction/src/DipoleConstruction.cc` | `ConstructSub()` 末尾新增 `vchamber_cavity` logical volume 和 placement（材质由 `fBeamLineVacuum` 决定）。 |
| `scripts/analysis/ms_ablation/run_ablation.sh` | 循环 3 条件（all_air / beamline_vacuum / all_vacuum），输出目录也从 `{baseline,no_air}` 改成 `{all_air,beamline_vacuum,all_vacuum}`；保持向后兼容的顶层 `compare.csv` 和 `figures/` 路径（覆盖既有）。 |
| `scripts/analysis/ms_ablation/compare_ablation.py` | 扩展到读 3 份 dispersion_summary.csv；`compare.csv` 9 行；overlay PNG 3 条色（all_air=灰、beamline_vacuum=蓝、all_vacuum=绿）。 |
| `scripts/analysis/ms_ablation/tests/test_compare_ablation.py` | 加 3-way 用例 fixture。 |

### 6.3 实现阶段创建（运行完实验后）

| 文件 | 内容 |
|---|---|
| `docs/reports/reconstruction/ms_ablation/ms_ablation_mixed_20260422_zh.md` | stage A′ 主 memo |
| `docs/reports/reconstruction/ms_ablation/data/{all_air,beamline_vacuum,all_vacuum}_dispersion_summary.csv` | 快照 |
| `docs/reports/reconstruction/ms_ablation/data/compare.csv` | 覆盖（3-way） |
| `docs/reports/reconstruction/ms_ablation/figures/dispersion_overlay_tp*.png` | 重绘（3-color） |
| 在 `ms_ablation_air_20260421_zh.md` 顶部加 "修正说明" 段落，指向 stage A′ memo | |
| `README.md` 索引表格加 stage A′ 行 | |

### 6.4 不改

- 既有 `_target3deg.mac` 和 `_target3deg_noair.mac`（成为 `all_air` 和 `all_vacuum` 的 macro，一字未改）
- `compute_metrics.py`（已是条件无关）
- stage A memo 正文（只加顶部的修正说明 pointer）
- `VacuumDownstreamConstruction`、`VacuumUpstreamConstruction`、`ExitWindowC2Construction` 的 ConstructSub（它们已正确）
- 其它任何 C++ 源文件

## 7. Memo 要回答的三问题

stage A′ memo 必须包含：

1. **三情形 σ_y(PDC2) 原始数据**：3 truth points × 3 条件 = 9 行表。包括 σ_x, σ_y(PDC1, PDC2), σ_θ_{x,y}。
2. **Δσ_y 分解**：
   - Δσ_y(all_air − beamline_vacuum) = **上游虚假空气贡献**（~1.63 m，应为真空）
   - Δσ_y(beamline_vacuum − all_vacuum) = **下游真实空气贡献**（~1.80 m，物理存在）
   - 比 Highland(1.80 m) 预测的 θ_0 ≈ 2.56 mrad，核对 σ_θ_y 端的一致性（linear propagation over PDC1→PDC2 drift 1 m）
3. **Stage D 噪声幅度修正**：stage A memo 给 stage D 的 σ_y,MS(PDC2) ≈ 14 mm 数字**偏大**。正确值应为 Δσ_y(beamline_vacuum − all_vacuum)，按 Highland ratio 预期 8–11 mm 量级，最终以实测为准。本 memo 公布最终数字。

## 8. 成功判据

- [ ] `BeamLineVacuum` Messenger 命令工作；默认 `false` 下 ctest 全绿（既有测试无破坏）
- [ ] `all_vacuum` 回归检查：50-event ensemble σ 列与 stage A `no_air` 各点差 ≤ ±5%（同 seed，同 truth grid）
- [ ] `all_air` 回归检查：50-event ensemble σ 列与 stage A `baseline` 各点差 ≤ ±5%（同 seed，同 truth grid，同 build）
- [ ] `beamline_vacuum` 的 σ_y(PDC2) ∈ [`all_vacuum`, `all_air`] 区间（理论必成立）；单调性三点一致
- [ ] 三情形的 `compare.csv` 共 9 行，3 张 overlay PNG
- [ ] memo `ms_ablation_mixed_20260422_zh.md` 含 §7 的三节完整回答
- [ ] stage A memo 顶部有 "修正说明" pointer 指向 A′；README 索引更新
- [ ] `test_beamline_vacuum_materials.sh` 通过

## 9. 风险与边界

### 9.1 vchamber_cavity 与 yoke subtraction 的 overlap 一致性

新 `vchamber_cavity` logical volume 摆放在 `fLogicDipole`（yoke）里，其 solid 必须与 yoke subtraction 所用 `vchamber_box ∪ vchamber_trap` **完全等同**，否则 Geant4 `CheckOverlaps` 会报错。

**缓解**：在 `DipoleConstruction::ConstructSub()` 内，先构造 `vchamber_box` 和 `vchamber_trap`（现有代码已有），再用 **同一对指针** 做 `G4UnionSolid("vchamber_cavity", vchamber_box, vchamber_trap, vct_rm, *vct_trans)`。placement 时 `G4PVPlacement(nullptr, {0,0,0}, cavity_log, "vchamber_cavity", fLogicDipole, false, 0)`（yoke 内部坐标系原点）。smoke 测试里开 `CheckOverlaps(true)` 跑一遍。

### 9.2 VacuumUpstream 与 dipole 的 overlap

`VacuumUpstreamConstruction::ConstructSub(dipole_phys)` 已做 `G4SubtractionSolid(vacuum_box, dipole_solid, rm, -fPosition)`，原理上 dipole 部分被扣除，不与 yoke 重叠。但 `fPosition = (0,0,-vacuum_dz)`（即 z=−2500 mm），vacuum_box 尺寸 1200×1000×5000 mm³，覆盖 z ∈ [−5000, 0]。这跟 dipole yoke 的 z ∈ [−1750, +1750] 有重叠需要扣除。

**缓解**：VacuumUpstream 已实现扣除。smoke 测试 `CheckOverlaps(true)`；若报 overlap，调整 vacuum_box 尺寸或扩大 subtraction 精度（Geant4 `G4SubtractionSolid` 默认精度足够）。

### 9.3 VacuumUpstream 覆盖到 target 吗

Target 位置 z=−1069.458 mm，vacuum_box 覆盖 z ∈ [−5000, 0]，包含 target 区间。Target 本身是 expHall 里另一块 logical volume（`Target` + `Target_SD`）；Geant4 会在 overlap 处用最"里层"的 volume，target 依然存在。但 target 的母体变成了 vacuum_upstream_log 而非 expHall_log —— **需要验证**：target 是否在 `SetTarget true` 时能被正确命中。

**缓解**：本 stage 的实验全部 `SetTarget false`（与 stage A 一致，target 用 tree-gun 注入），所以**不触及此边界**。若未来 stage D 要开 target，需在 VacuumUpstream 里挖出 target 形状的洞，或把 target 摆在 vacuum_upstream_log 里。本 spec 留 hook 不实现。

### 9.4 Stage A regression

stage A 的 baseline CSV/figure 不被此 spec 覆盖——它们是 stage A 那次运行的历史记录。新运行的 `all_air` 条件会写入**新** CSV（`all_air_dispersion_summary.csv`），新数字和 stage A 的 baseline 数字应相差 ≤ 几 %（同 build、同 seed、同 macro）但不强行相等（build 日期可能不同）。§8 成功判据给出 ±5% 容差。

### 9.5 Memo 并存

stage A memo 保留，不被删，不被重写正文；只在顶部加 "修正说明" 指向 A′。stage D 的后续 spec（若启动）直接引用 A′ 的数字，不再引用 stage A。

## 10. 不在本 spec 范围

- **Stage D（RK process-noise 修复）**：独立 spec。本 spec 只提供 Δσ_y(beamline_vacuum − all_vacuum) 这个噪声幅度输入。
- **ExitWindowC2 的薄膜建模**：当前 flange 是 30 mm 铁法兰，没有建 thin foil。若 A′ 的 `all_vacuum` 残余 σ_y ~1 mm 还嫌大，才考虑加 thin foil；这是 stage B 的事。
- **PDC 气体 MS 消除**：仍然是 stage C，未启动。
- **Target 摆进 VacuumUpstream 内**：见 §9.3，本 spec 不覆盖带 target 的场景。
