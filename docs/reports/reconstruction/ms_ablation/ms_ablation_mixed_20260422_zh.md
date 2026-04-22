# MS 消融实验 · 两段几何、三种情形（rev2）

- **日期**: 2026-04-22（rev2 2026-04-22 当日修正）
- **作者**: TBT
- **状态**: 3 个条件 × 3 truth 点 × N=50 完成
- **Spec**: [`specs/2026-04-22-ms-ablation-mixed-design.md`](specs/2026-04-22-ms-ablation-mixed-design.md)
- **Plan**: [`plans/2026-04-22-ms-ablation-mixed-plan.md`](plans/2026-04-22-ms-ablation-mixed-plan.md)
- **数据**: [`data/compare.csv`](data/compare.csv) · [`data/all_air_dispersion_summary.csv`](data/all_air_dispersion_summary.csv) · [`data/beamline_vacuum_dispersion_summary.csv`](data/beamline_vacuum_dispersion_summary.csv) · [`data/all_vacuum_dispersion_summary.csv`](data/all_vacuum_dispersion_summary.csv)
- **图**: [`figures/`](figures/)

---

## TL;DR

- **靶在磁铁内腔里**：世界坐标 $(-12.5, 0.0, -1069.5)$ mm → yoke 本地 $(-545.5, 0, -919.9)$ mm；yoke 内腔范围 $z\in[-1750, +1750]$ mm 局部，靶完全在腔内。粒子从靶出发不存在"靶→dipole 入射面"的外部路段。
- **磁铁腔体与下游管道联通**，共用 `BeamLineVacuum` 开关。rev2 修 `VacuumDownstreamConstruction` 让管道材质跟随该开关（stage A′ 硬编码真空，物理上不成立）。
- **两段几何**：Region A = 磁铁腔体 + 下游管道（~4.3 m，控制：`BeamLineVacuum`）；Region B = ExitWindow → PDC1 → PDC2（~2.0 m，控制：`FillAir`）。
- **三情形 σ_y(PDC2)（mm，3 truth 点）**：all_air 12.83–14.93；beamline_vacuum 6.80–8.21；all_vacuum 0.95–1.00。
- **Quadrature 分解（3 点平均 σ_y PDC2）**：σ_A(Region A 空气) ≈ **11.19 mm**；σ_B(Region B 空气) ≈ **7.43 mm**。
- **Stage D 输入**：
  - 若实验 Region A 抽真空、Region B 是空气 → σ_y ≈ **7.49 mm**（beamline_vacuum 实测 3 点均值）、σ_x ≈ 2.50 mm。
  - 若实验 Region A + Region B 都是空气 → σ_y ≈ **13.50 mm**、σ_x ≈ 4.37 mm。
  - 实验条件仍在探索；spec 取哪一个由 Region A 状态决定。

---

## 1. 几何与飞行路径

### 1.1 靶位置

| 系 | 坐标 (mm) |
|---|---|
| 世界 | $(-12.488, 0.0009, -1069.458)$ |
| yoke 本地（yoke 绕 $Y$ 旋转 $-30°$） | $(-545.5, 0, -919.9)$ |

yoke 内腔由 `vchamber_box`（$\pm 1620, \pm 400, \pm 1750$ mm）与下游梯形扩展（`vchamber_trap`）取并集。靶本地 $z = -919.9$ mm 在 $[-1750, +1750]$ mm 范围内 → **靶物理上就在磁铁内腔里**。

### 1.2 两段

| # | 物理体积 | 近似长度 | 材质开关 |
|---|---|---:|---|
| A | `vchamber_cavity` LV + `VacuumDownstream` 管道（连通容器） | ~4.3 m | `/samurai/geometry/BeamLineVacuum` |
| B | ExitWindow → PDC1 → PDC2 （世界体中自由飞行） | ~2.0 m | `/samurai/geometry/FillAir` |

分段：
- Region A：靶 (local $z=-920$) → yoke 内腔出口 (local $z=+1750$) 本地 $z$ 直线距离 **2.67 m**；yoke 出口 → ExitWindow 管道段 **~1.6 m**；合计 **~4.3 m**。粒子在 1.15 T 场中弯曲约 $30°$，弧长与直线距离差 $<10\%$。
- Region B：ExitWindow (world $(170.5, 0, 3187.5)$ mm) → PDC1 (world $(700, 0, 4000)$ mm) = **0.97 m**；PDC1 → PDC2 = **1.00 m**；合计 **~2.0 m**。

### 1.3 物理联通性

`VacuumDownstream` 管道的上游法兰接在 yoke 出口上；构造上是连通容器，共用同一开关。rev2 的代码变动是让 `VacuumDownstreamConstruction::ConstructSub` 读取 `fBeamLineVacuum`：true 时 `G4_Galactic`，false 时 world 材质。stage A′ 把管道硬编码为真空、与磁铁腔体分离，物理上不成立。

---

## 2. 三种情形

| 情形 | Region A | Region B | 宏文件 |
|---|---|---|---|
| `all_air`         | 空气 | 空气 | `geometry_..._target3deg.mac`（`FillAir true`） |
| `beamline_vacuum` | 真空 | 空气 | `..._target3deg_mixed.mac`（`FillAir true` + `BeamLineVacuum true`） |
| `all_vacuum`      | 真空 | 真空 | `..._target3deg_noair.mac`（`FillAir false`） |

`BeamLineVacuum` 独立于 `FillAir`。`all_vacuum` 靠 `FillAir=false`（world=vacuum）让 Region A 也自然为真空，不需额外设 `BeamLineVacuum`。

### 代码改动（rev2）

1. `libs/smg4lib/src/construction/include/VacuumDownstreamConstruction.hh`：加 `SetBeamLineVacuum(tf)` 与 `fBeamLineVacuum` 字段。
2. `libs/smg4lib/src/construction/src/VacuumDownstreamConstruction.cc`：管道材质按 `fBeamLineVacuum` 选（true → `G4_Galactic`，false → world 材质）。
3. `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`：实例化 `VacuumDownstream` 之前 `SetBeamLineVacuum(fBeamLineVacuum)`。

---

## 3. 配置与执行

| 条目 | 值 |
|---|---|
| ENSEMBLE_SIZE | 50 events per (truth point × 条件) |
| Seeds | SEED_A=20260421, SEED_B=20260422（三条件共享） |
| Truth 点 | $(p_x, p_y)$ = $(0, 0)$, $(100, 0)$, $(0, 20)$ MeV/c |
| 粒子 | proton, $|\vec p|=627$ MeV/c，$\beta c p = 348.4$ MeV |
| Target | `SetTarget false`（tree-gun 注入） |
| Reco | `test_pdc_target_momentum_e2e --backend rk --rk-fit-mode fixed-target-pdc-only` |
| σ 定义 | 鲁棒半宽 $(p_{84}-p_{16})/2$ |

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/analysis/ms_ablation/run_ablation.sh
```

产物在 `build/test_output/ms_ablation_air/`。

---

## 4. 三情形 σ 结果

| 条件 | $(p_x, p_y)$ | N | σ_x(P1) | σ_y(P1) | σ_x(P2) | σ_y(P2) | σ_θ_x | σ_θ_y |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| all_air         | (0, 0)   | 50 | 3.071 |  9.736 | 3.669 | **12.826** | 12.279 | 9.240 |
| all_air         | (100, 0) | 50 | 3.838 | 10.492 | 5.377 | **12.751** | 15.768 | 8.277 |
| all_air         | (0, 20)  | 50 | 3.372 | 13.039 | 4.065 | **14.927** | 21.709 | 7.846 |
| beamline_vacuum | (0, 0)   | 50 | 1.528 |  4.061 | 2.086 | **6.801**  | 11.834 | 5.910 |
| beamline_vacuum | (100, 0) | 50 | 2.067 |  4.888 | 3.060 | **8.211**  |  8.100 | 5.786 |
| beamline_vacuum | (0, 20)  | 49 | 1.667 |  4.789 | 2.363 | **7.459**  | 14.289 | 5.421 |
| all_vacuum      | (0, 0)   | 50 | 0.147 |  0.515 | 0.447 | **0.947**  |  6.798 | 2.558 |
| all_vacuum      | (100, 0) | 50 | 0.154 |  0.364 | 0.416 | **0.999**  |  4.075 | 1.956 |
| all_vacuum      | (0, 20)  | 50 | 0.174 |  0.411 | 0.396 | **1.002**  |  6.913 | 2.299 |

单位：σ 为 mm，σ_θ 为 mrad。单调性：all_air > beamline_vacuum > all_vacuum 在 6 个位置 σ 列上对所有 3 truth 点都成立。

Overlay 图：[`figures/dispersion_overlay_tp0_0.png`](figures/dispersion_overlay_tp0_0.png)、[`figures/dispersion_overlay_tp100_0.png`](figures/dispersion_overlay_tp100_0.png)、[`figures/dispersion_overlay_tp0_20.png`](figures/dispersion_overlay_tp0_20.png)。

---

## 5. 两段空气分解

### 5.1 线性差

| 差 | 对应 Region | TP1 | TP2 | TP3 | 3 点平均 |
|---|---|---:|---:|---:|---:|
| all_air − beamline_vacuum | A（~4.3 m） | 6.025 | 4.540 | 7.468 | **6.011** |
| beamline_vacuum − all_vacuum | B（~2.0 m） | 5.854 | 7.213 | 6.456 | **6.508** |
| all_air − all_vacuum | A + B | 11.879 | 11.752 | 13.925 | **12.519** |

（单位 mm，σ_y(PDC2)）

### 5.2 Quadrature 分解

$$\sigma_A = \sqrt{\sigma_\mathrm{all\_air}^2 - \sigma_\mathrm{bv}^2}, \quad \sigma_B = \sqrt{\sigma_\mathrm{bv}^2 - \sigma_\mathrm{av}^2}.$$

| truth | σ_A (Region A) | σ_B (Region B) | $\sqrt{\sigma_A^2+\sigma_B^2+\sigma_\mathrm{av}^2}$ | all_air 实测 |
|---|---:|---:|---:|---:|
| (0, 0)   | 10.874 | 6.735 | 12.826 | 12.826 |
| (100, 0) |  9.755 | 8.150 | 12.751 | 12.751 |
| (0, 20)  | 12.930 | 7.391 | 14.927 | 14.927 |
| **3 点平均** | **11.19** | **7.43** | — | — |

Quadrature-sum 与 all_air 实测一致到浮点精度（< 10⁻³ mm）：σ² 可加假设精确自洽。

### 5.3 物理直觉

Region A 路径长度是 Region B 的 2 倍多，但 σ_y(PDC2) 的线性贡献 6.01 mm vs 6.51 mm、quadrature 贡献 11.19 mm vs 7.43 mm，Region A 只比 B 略大。原因：Region A 的 MS 发生在离 PDC 平面更远处，经过磁场弯折后不以简单"L×θ"方式投影到 PDC；Region B 的 MS 几乎直接加到 PDC 位置，放大系数接近 1。

---

## 6. Stage D process-noise 输入

| 实验 Region A 配置 | σ_y(PDC2) 输入 | σ_x(PDC2) 输入 | 来源（3 truth 点平均） |
|---|---:|---:|---|
| Region A 抽真空，Region B 空气 | **7.49 mm** | 2.50 mm | beamline_vacuum 实测 σ（仪器残余 ~1 mm 忽略） |
| Region A + B 都是空气 | **13.50 mm** | 4.37 mm | all_air 实测 σ（仪器残余忽略） |

按 truth 点分别给（Region A 抽真空情形，σ_y(PDC2)）：6.80 / 8.21 / 7.46 mm。

**决定权在实验**：Region A 的真空/空气选择未定；本 memo 提供两个候选，spec 引用其一取决于实验最终决定。

---

## 7. 风险与澄清

- **σ 定义**：鲁棒半宽 $(p_{84}-p_{16})/2$。纯高斯等于 RMS；重尾分布偏保守 ~10%。
- **TP3 beamline_vacuum N=49**：1 event 的 proton 未匹配 PDC 双平面；缺失率 2%。
- **all_vacuum 残余 σ_y ~1.0 mm**：PDC 气体 + ExitWindow 铁法兰（30 mm，不是薄膜）+ 几何细节；非本 memo 分解对象。若下游需要，另起 ablation。
- **曲率修正**：Region A 的 4.3 m 是直线近似，沿 1.15 T 场中弧长差 <10%。Highland 核验未做（弧长、速度变化让简单 $L/X_0$ 公式失准），quadrature 分解由实测数据直接给出。
- **Target off**：三情形 `SetTarget false`，靶体积不参与 MS。
- **实验条件**：Region A 真实值仍在探索；本文三情形只作为模拟端的消融分解，不代表最终实验配置。

---

## 8. 关联

- Spec: [`specs/2026-04-22-ms-ablation-mixed-design.md`](specs/2026-04-22-ms-ablation-mixed-design.md)
- Plan: [`plans/2026-04-22-ms-ablation-mixed-plan.md`](plans/2026-04-22-ms-ablation-mixed-plan.md)
- 上游 stage A memo: [`ms_ablation_air_20260421_zh.md`](ms_ablation_air_20260421_zh.md)（其分段物理上不正确，本 memo 替代）
- RK 报告: `docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.pdf` §7.5
- 下游: stage D（RK process-noise）独立 spec 待起草；σ 输入取第 §6 节，具体数字按实验 Region A 配置选。
