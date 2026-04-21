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

**小问题被发现**：stage A 的 baseline 几何实际上**不物理**。查 Geant4 源码后：

| 区域 | 当前物理占用 |
|---|---|
| World（世界体） | `fFillAir ? G4_AIR : G4_Galactic` — 受 Messenger 控制 ✓ |
| SAMURAI 偶极磁铁内腔（vchamber 真空室） | **没有独立 logical volume**，仅是从 yoke 里挖掉的 void → **继承 world 材质** |
| ExitWindowC2 cavity（出射窗法兰中间的孔洞） | **没有独立 logical volume**，仅是从 flange 里挖掉的 void → **继承 world 材质** |

在 `FillAir=true` 下，**这两个本应是 beam-line 真空的区域也被灌了空气**。实验上 SAMURAI 的磁铁内腔与 ExitWindowC2 内侧是联通真空，与大气在 ExitWindowC2 薄膜处分界；stage A 的 baseline 多算了 3.46 m 不存在的空气。

**动机**：

1. Stage A 的 "94% 空气贡献" 把 1.80 m 真实空气 + 3.46 m 虚假空气搅在一起，对 stage D 的 process-noise 幅度校准**不准**。
2. 需要一个**物理上真实的** baseline（world 空气 + beam-line 真空），作为 stage D 过程噪声幅度的直接输入。

**Stage A 并不作废**：它证明了"world 切 FillAir 是有效的 MS 消融开关"，且为代码几何发现的这个 bug 提供了直接证据。Stage A′ 在它之上补一个物理真实的中间条件。

## 2. 研究问题

**Q1**：1.80 m 真实空气路径（ExitWindowC2 → PDC2）对 PDC2 σ_y 单独贡献多少？

**Q2**：stage A 测到的 Δσ_y（all_air − all_vacuum ≈ 10–14 mm）里，多少来自**上游虚假空气**（3.46 m，不该存在），多少来自**下游真实空气**（1.80 m，物理存在）？

**Q3**：在物理真实的 `beamline_vacuum` 条件下，PDC2 σ_y 仍然远大于全真空残余（~1 mm）吗？若是 → stage D process-noise 修复仍然必要，且有了准确的 σ_MS 幅度。

## 3. 三种情形对照

全部 3 truth points × N=50 seeds，同一 SEED_A/SEED_B，同一 build，严格对照：

| 情形 | `FillAir` | `BeamLineVacuum` | Target→EW 路径 | EW→PDC2 路径 | 质子穿空气总长 | 物理真实性 |
|---|:---:|:---:|:---:|:---:|:---:|---|
| `all_air`（= stage A baseline） | true | false | 空气 3.46 m | 空气 1.80 m | **5.26 m** | 不物理 |
| `beamline_vacuum`（**新**，stage A′） | true | **true** | 真空 3.46 m | 空气 1.80 m | **1.80 m** | **实验真实** |
| `all_vacuum`（= stage A no_air） | false | 无关 | 真空 | 真空 | **0 m** | 参考上限，实验不可达 |

Highland 预测（σ_θ ∝ √L）：`beamline_vacuum` 的 σ_y(PDC2) 应为 `all_air` 的 √(1.80/5.26) ≈ 0.58 倍 → ~6.4–8.8 mm。

## 4. 飞行距离表

基于当前几何源码与 1.15 T dipole 的弯曲半径 ρ = p/(0.3B) ≈ 1.82 m（对 627 MeV/c proton）：

| 路段 | 长度 | 物理上的材质 |
|---|---:|---|
| Target (z=−107 cm) → dipole 入射面 (z=−175 cm) | 1.06 m | 真空（beam-line 上游） |
| dipole 入射 → dipole 出射（30° 弯曲弧长） | 0.95 m | 真空（磁铁内腔） |
| dipole 出射 (z=+175 cm) → ExitWindowC2 出射面 (z=+320 cm) | 1.45 m | 真空（出口联通管） |
| ExitWindowC2 出射面 → PDC1 中心 (z=+400 cm) | 0.80 m | 空气 |
| PDC1 → PDC2 | 1.00 m | 空气 |
| **Target → PDC2 总长** | **~5.26 m** | 真空 3.46 m + 空气 1.80 m |

## 5. 架构

### 5.1 C++ 改动（Approach 1）

新增一个正交开关 `/samurai/geometry/BeamLineVacuum true|false`，默认 `false`（保留既有行为 bit-for-bit）。启用后：四个新 logical volume 材质改为 `G4_Galactic`；否则继承世界材质。

| 新 logical volume | 源文件 | 几何 | 用途 |
|---|---|---|---|
| Target→dipole 真空管 | `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc` | `G4Tubs`，半径 20 cm，z 从 −1070 mm 到 −1750 mm | 替换当前 world 材质占用 |
| Dipole vchamber 内腔 | `libs/smg4lib/src/construction/src/DipoleConstruction.cc` | 复用现有 `vchamber_box + vchamber_trap`（union solid），与 yoke 同 transform | 填充 yoke 挖掉的 void |
| Dipole→EWC2 真空管 | `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc` | `G4Tubs`，半径 30 cm，z 从 +1750 mm 到 +3170 mm，**绕 dipole 30° 旋转轴对齐** | 对齐弯转后束流方向 |
| ExitWindowC2 cavity | `libs/smg4lib/src/devices/ExitWindowC2Construction.cc` | 复用现有 `hole_box`，放在 flange 局部坐标系中心 | 填充 flange 挖掉的 void |

`DeutDetectorConstructionMessenger.cc` 新增 `/samurai/geometry/BeamLineVacuum` 命令（参考现有 `/samurai/geometry/FillAir` 模板）。`DeutDetectorConstruction` 加 `fBeamLineVacuum` 布尔成员 + getter `IsBeamLineVacuum()`，供三个子 constructor 读取。

**无回归风险保证**：`BeamLineVacuum` 默认 `false` → 新 logical volume 材质 = world 材质 → 与无它们时的 Geant4 行为**一致**（空气/真空都均匀）。ctest 默认全绿。

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
| `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac` | 复制 `_target3deg.mac` + 追加 `/samurai/geometry/BeamLineVacuum true` |
| C++ 改动（上述 5.1 的 4 个新 logical volume + Messenger 命令） | 见 §5.1 |
| `scripts/analysis/ms_ablation/tests/test_beamline_vacuum_materials.sh` | 解析 run log，验证 `BeamLineVacuum=true` 下 4 个新 volume 的材质打印为 `G4_Galactic` |
| `docs/reports/reconstruction/ms_ablation/specs/2026-04-22-ms-ablation-mixed-design.md` | 本文档 |

### 6.2 修改

| 文件 | 变动 |
|---|---|
| `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc` | +2 新 G4Tubs volume + 读取 `fBeamLineVacuum` |
| `libs/sim_deuteron_core/include/DeutDetectorConstruction.hh` | 加 `fBeamLineVacuum` 成员 + getter |
| `libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc` | 新 Messenger 命令 |
| `libs/smg4lib/src/construction/src/DipoleConstruction.cc` | 新 vchamber logical volume |
| `libs/smg4lib/src/devices/ExitWindowC2Construction.cc` | 新 cavity logical volume |
| `scripts/analysis/ms_ablation/run_ablation.sh` | 循环 3 条件 |
| `scripts/analysis/ms_ablation/compare_ablation.py` | 扩展 3-way 比较 + 3-color overlay |
| `scripts/analysis/ms_ablation/tests/test_compare_ablation.py` | 加 3-way 用例 |

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

- 既有 `_target3deg.mac` 和 `_target3deg_noair.mac`（成为 `all_air` 和 `all_vacuum` 的 macro）
- `compute_metrics.py`（已是条件无关）
- stage A memo 正文（只加顶部的修正说明 pointer）
- 其它任何 C++ 源文件

## 7. Memo 要回答的三问题

stage A′ memo 必须包含：

1. **三情形 σ_y(PDC2) 原始数据**：3 truth points × 3 条件 = 9 行表。包括 σ_x, σ_y(PDC1, PDC2), σ_θ_{x,y}。
2. **Δσ_y 分解**：
   - Δσ_y(all_air − beamline_vacuum) = 上游**虚假**空气贡献（3.46 m，应为真空）
   - Δσ_y(beamline_vacuum − all_vacuum) = 下游**真实**空气贡献（1.80 m，物理存在）
   - 比 Highland(1.80 m) 预测的 θ_0 ≈ 2.56 mrad，核对 σ_θ_y 端的一致性
3. **Stage D 噪声幅度修正**：stage A memo 给 stage D 的 σ_y,MS(PDC2) ≈ 14 mm 数字**偏大**。正确值应为 Δσ_y(beamline_vacuum − all_vacuum)，预计 5–8 mm 量级。本 memo 公布最终数字。

## 8. 成功判据

- [ ] `BeamLineVacuum` Messenger 命令工作；默认 `false` 下 ctest 全绿（既有测试无破坏）
- [ ] `all_vacuum` 回归检查：50-event ensemble σ 列与 stage A `no_air` 各点差 ≤ ±5%（同 seed，同 truth grid）
- [ ] `beamline_vacuum` 的 σ_y(PDC2) ∈ [`all_vacuum`, `all_air`] 区间（理论必成立）；单调性三点一致
- [ ] 三情形的 `compare.csv` 共 9 行，3 张 overlay PNG
- [ ] memo `ms_ablation_mixed_20260422_zh.md` 含 §7 的三节完整回答
- [ ] stage A memo 顶部有 "修正说明" pointer 指向 A′；README 索引更新

## 9. 风险与边界

### 9.1 几何 overlap

新 vchamber logical volume 必须与 yoke 的 subtraction 完全匹配（同 G4VSolid 指针或精确复制），否则 Geant4 `CheckOverlaps` 报错。

**缓解**：复用现有 `vchamber_box / vchamber_trap` 变量（而非 new 相同参数），transform 直接抄 yoke placement。实现阶段如果 DipoleConstruction 里这两个 solid 不在类成员作用域，需要把它们 hoist 出来或重新构造一次，此时需 `CheckOverlaps(true)` 在 unit test 里跑一遍。

### 9.2 Dipole→EWC2 真空管的旋转

Dipole 出射后束流方向绕 y 轴转了 30°。新 G4Tubs 必须绕同一轴旋转，否则一端会伸出 dipole yoke 之外，另一端偏离 ExitWindowC2 入射面。

**缓解**：拷贝 ExitWindowC2 当前 placement 的 rotation matrix（已经是 30°），把 tubs 中心定位在两端点中点。unit test 跑 `CheckOverlaps` 确认。

### 9.3 ExitWindowC2 cavity 局部坐标

需要确认 `hole_box` 是在 flange 局部坐标还是世界坐标里定义。**实现阶段先读源码**；若是世界坐标要小心 placement 叠加。

### 9.4 Stage A regression

stage A 的 baseline CSV/figure 不被此 spec 覆盖——它们是 stage A 那次运行的历史记录。新运行的 `all_air` 条件会写入**新** CSV（`all_air_dispersion_summary.csv`，对应 compare.csv 的 all_air 行）；新数字和 stage A 的 baseline 数字应相差 ≤ 几 %（同 build、同 seed、同 macro）但不强行相等（build 日期可能不同）。

### 9.5 Memo 并存

stage A memo 保留，不被删，不被重写正文；只在顶部加 "修正说明" 指向 A′。stage D 的后续 spec（若启动）直接引用 A′ 的数字，不再引用 stage A。

## 10. 不在本 spec 范围

- **Stage D（RK process-noise 修复）**：独立 spec。本 spec 只提供 Δσ_y(beamline_vacuum − all_vacuum) 这个噪声幅度输入。
- **ExitWindowC2 的薄膜建模**：当前 flange 是 30 mm 铁法兰，没有建 thin foil（参考 Explore 调研）。若 A′ 的 `all_vacuum` 残余 σ_y ~1 mm 还嫌大，才考虑加 thin foil；这是 stage B 的事。
- **PDC 气体 MS 消除**：仍然是 stage C，未启动。
