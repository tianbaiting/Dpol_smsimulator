> **⚠ 修正说明（2026-04-22）**: 本 memo 的 baseline 几何**不完全物理**——靶→dipole 入射面（~0.68 m）与 dipole yoke 内腔（~0.95 m）在 Geant4 里误为空气（应为束流真空）。因此本 memo "air 贡献 94%" 的数字混合了约 1.6 m 虚假上游空气 + 1.8 m 真实下游空气。物理真实的 baseline（只真实下游空气）与完整分解见 [`ms_ablation_mixed_20260422_zh.md`](ms_ablation_mixed_20260422_zh.md)（stage A′）。本 memo 保留作为历史记录与代码 bug 的直接证据，**不再作为 stage D 的输入**。

---

# MS 消融实验 v1 · Air 关闭 — 主 memo

- **日期**: 2026-04-21
- **作者**: TBT
- **状态**: 首轮（阶段 A）实验完成
- **Spec**: [`specs/2026-04-21-ms-ablation-air-design.md`](specs/2026-04-21-ms-ablation-air-design.md)
- **Plan**: [`plans/2026-04-21-ms-ablation-air-plan.md`](plans/2026-04-21-ms-ablation-air-plan.md)
- **数据**: [`data/compare.csv`](data/compare.csv) · [`data/baseline_dispersion_summary.csv`](data/baseline_dispersion_summary.csv) · [`data/no_air_dispersion_summary.csv`](data/no_air_dispersion_summary.csv)
- **图**: [`figures/`](figures/)

---

## TL;DR

- 关掉 Geant4 世界空气后，PDC2 的命中 σ_y 从 **11–15 mm 塌到 ~1 mm**，ΔσY(PDC2)/σY(baseline) = **91–94%**。
- 空气是 §7.5 报告残留 σ_y 的**主导来源**，按 spec §8.2 判据（Δ > 0.5× baseline）→ 进入 RK process-noise 修复路径。
- Highland 手算空气对 θ_MS 的贡献 1.7–3 mrad，测得 Δσ_θ_y（二次相减）= 5.0–7.5 mrad，量级一致，剩余部分与空气路径 >1 m、PDC 气体/窗口等残余源不矛盾。

---

## 1. 实验配置

| 条目 | 值 |
|---|---|
| git HEAD (运行时) | `cd53db2` (main, 2026-04-21) |
| 几何宏 baseline | `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac` (FillAir=true) |
| 几何宏 no_air | `geometry_B115T_pdcOptimized_20260227_target3deg_noair.mac` (FillAir=false，其余逐字节相同) |
| 真值点 | TP1=(0,0,627)、TP2=(100,0,627)、TP3=(0,20,627) MeV/c，proton |
| Ensemble | N=50 per (truth point, 条件)；两条件共享种子序列 (SEED_A=20260421, SEED_B=20260422) |
| 靶 | 关闭（`SetTarget false`，PDC 研究惯例） |
| Reco | `test_pdc_target_momentum_e2e --backend rk --rk-fit-mode fixed-target-pdc-only` |
| 指标 | M1=PDC 命中位置鲁棒半宽 (p84−p16)/2；M2=PDC1→PDC2 斜率的鲁棒半宽 |

> TP1 baseline 只有 N=49 有效（一个 event 的 proton 未到达两个 PDC 平面，被 hit-matching 自然排除）。其余全部 50/50。缺失率 ≤ 2%，远低于 spec §10.3 的 20% 阈值。

---

## 2. Δσ_y 绝对值（Q1）

原始鲁棒半宽（单位均为 mm / mrad）：

| truth (px,py) | N | σ_y(PDC1) | σ_y(PDC2) | σ_θ_y |
|---|---:|---:|---:|---:|
| baseline (0, 0) | 49 | 11.29 | 15.33 | 7.96 |
| baseline (100, 0) | 50 | 9.55 | 11.77 | 5.36 |
| baseline (0, 20) | 50 | 9.19 | 11.45 | 6.49 |
| no_air (0, 0) | 50 | 0.52 | 0.95 | 2.56 |
| no_air (100, 0) | 50 | 0.36 | 1.00 | 1.96 |
| no_air (0, 20) | 50 | 0.41 | 1.00 | 2.30 |

**Δ = baseline − no_air（线性差）**：

| truth | Δσ_y(PDC1) [mm] | Δσ_y(PDC2) [mm] | Δσ_y(PDC2) / σ_y(baseline) |
|---|---:|---:|---:|
| (0, 0)   | 10.77 | **14.38** | **93.8%** |
| (100, 0) | 9.19  | **10.77** | **91.5%** |
| (0, 20)  | 8.78  | **10.44** | **91.2%** |

三个点一致地显示：PDC2 命中 σ_y 里 **~92% 来自世界空气**。x 方向的 Δσ_x(PDC2) 也同比下降（3.6–4.4 mm → 0.40–0.45 mm），但绝对值比 y 小 3–4 倍，与报告 §7.5 关于偶极场在 xz 平面内弯曲、y 方向完全由 MS 累积的物理图像一致。

Overlay 散点图：[`figures/dispersion_overlay_tp0_0.png`](figures/dispersion_overlay_tp0_0.png)、[`figures/dispersion_overlay_tp100_0.png`](figures/dispersion_overlay_tp100_0.png)、[`figures/dispersion_overlay_tp0_20.png`](figures/dispersion_overlay_tp0_20.png)（每张含 PDC1 / PDC2 两子图）。

### 2.1 与 2026-04-16 报告 §7.5 基线的一致性

报告表 15 给出 PDC2 σ_y = 8–15 mm 区间，本次 baseline 三点分别为 15.33 / 11.77 / 11.45 mm，落在区间内。偏差 < 10%，符合 spec §10.5 的 ±15% 合理窗口，未发现代码或常数显著漂移。

---

## 3. 与 Highland 公式对照（Q2）

Highland（PDG 推荐形式）：

$$\theta_0 = \frac{13.6~\text{MeV}}{\beta c p}\,z\,\sqrt{x/X_0}\,\bigl[1 + 0.038\,\ln(x/X_0)\bigr]$$

对 p=627 MeV/c proton（E=1128.5 MeV，βcp=348.4 MeV，z=1），空气 X₀≈304 m（NTP）：

| 路径长度 L | x/X₀ | Highland θ₀ |
|---:|---:|---:|
| 1.0 m | 3.3×10⁻³ | **1.75 mrad** |
| 2.0 m | 6.6×10⁻³ | **2.56 mrad** |
| 3.0 m | 9.9×10⁻³ | **3.20 mrad** |

测得 air 贡献的 σ_θ_y（从 baseline 与 no_air 做**平方差**，把独立噪声源正确解耦）：

| truth | σ_θ_y(base) | σ_θ_y(no_air) | √(b² − n²) [mrad] |
|---|---:|---:|---:|
| (0, 0)   | 7.96 | 2.56 | **7.53** |
| (100, 0) | 5.36 | 1.96 | **4.99** |
| (0, 20)  | 6.49 | 2.30 | **6.07** |

**解读**：

- Highland 的 1 m 空气给 1.75 mrad；PDC1 前后的总空气路径估计在 2–3 m 量级（靶—PDC1 约 4 m（beam 前段）+ PDC1—PDC2 之间 1 m 分段），若按 3–4 m 有效路径估，Highland 给 3.2–3.7 mrad。
- TP1（直飞，最长飞行路径）实测 7.5 mrad 是 Highland(3 m) 的 ~2.3×；TP2 实测 5.0 mrad 接近 Highland(2 m) 的 2×；TP3 夹在中间。
- 偏大的系数与以下两点一致：(a) "σ_θ_y" 是 PDC1→PDC2 线段的角度弥散，累积了 PDC1 前段的位置抖动经由 1 m 漂移放大的分量；(b) 鲁棒半宽 (p84−p16)/2 对尾部稍有放大，对纯高斯约等于 1σ 但非严格相等。

结论：实验测得的 air → σ_θ_y 量级（5–7 mrad）与 Highland 在合理空气路径（2–3 m）下的预测（2–3 mrad）差 **~2×**，在 Highland 公式的 ±11% 精度和 PDC1 前段抖动放大之外是合理的残差；**没有发现"air 贡献比 Highland 预期大一个量级"的异常**。这意味着 RK 端的过程噪声项若按 Highland 式估计，应能贴合实验。

---

## 4. 下一步判决（Q3）

Spec §8.2 阈值：

| 判据 | 行动 |
|---|---|
| Δσ_y > 0.5 × σ_y(baseline) | 空气主导 → 进入 RK process-noise 修复（阶段 D） |
| Δσ_y < 0.2 × σ_y(baseline) | 空气贡献小 → 下一迭代关 exit window（阶段 B） |
| 中间情形 | 仍按 (B) 验证 |

**本次数据**：Δσ_y(PDC2) / σ_y(baseline) = 0.91–0.94，三点一致 **>> 0.5**。

**→ 决定：直接进入阶段 D，RK process-noise 修复。** 无需跑阶段 B/C。

### 4.1 给阶段 D 的直接输入

- RK 测量协方差 Σ_meas 目前只含 PDC 丝坐标分辨率 σ_wire ≈ 0.2 mm。需要为每个 PDC 平面加上 MS 过程噪声：
  - y 方向主导项：σ_y,MS(PDC2) ≈ 14 mm（即本表 Δσ_y(PDC2) 中位数）；
  - x 方向：σ_x,MS(PDC2) ≈ 3.9 mm。
- 更一般地：过程噪声应建模为"从靶点经过空气累积到当前 PDC 平面"的 Gaussian σ(L)，而非一个常数。工程落地可用 Kalman / Glueckstern 式的分层模型（飞行路径按 SAMURAI 磁场积分）。
- 目标：报告表 16 的 G4/Fisher σ_py 比值 1.86 → ~1.0。

阶段 D 拆到独立 spec，不在本 memo 范围。本 memo 仅提供噪声幅度的 G4 测量输入。

---

## 5. 数据产物与复现

- 脚本入口：`bash scripts/analysis/ms_ablation/run_ablation.sh`（默认 ENSEMBLE_SIZE=50；smoke 可用 `ENSEMBLE_SIZE=2 bash …`）。
- 产物目录：`build/test_output/ms_ablation_air/{baseline,no_air,figures}` + `compare.csv`。
- docs 快照（本目录 `data/`、`figures/`）由运行后手动拷贝，便于与 git 一起追踪数值。
- 代码改动仅限：两个新 `.mac` 宏 + `scripts/analysis/ms_ablation/` 三个脚本 + 单元测试；C++ 未改。

---

## 6. 风险与澄清

- **Robust half-width ≠ std**。本 memo 所有 σ 都是 (p84−p16)/2。对纯高斯两者一致，但对偏态或双峰分布，robust 值偏保守。若阶段 D 的 RK 噪声要求严格 RMS，需要在 `compute_metrics.py` 加一列 `sigma_y_pdc2_rms_mm`。
- **TP1 baseline N=49**。漏 1 event 是因为有 proton 未被匹配到两个 PDC 平面；不影响本 memo 结论量级，但在公开报告表中应注明。
- **真空 ≠ 完全无 MS**。`FillAir false` 切的是世界材质，PDC 气体本身和 exit window 仍在。no_air 里残留的 σ_y(PDC2) ~1 mm、σ_θ_y ~2 mrad 就是这些"非空气源"的上限估计。若将来要把这 ~1 mm 也消掉，走 spec §3.3 的 (b)/(c) 路径。

---

## 7. 关联

- 上游报告：`docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.pdf` §7.5
- Spec: [`specs/2026-04-21-ms-ablation-air-design.md`](specs/2026-04-21-ms-ablation-air-design.md)
- Plan: [`plans/2026-04-21-ms-ablation-air-plan.md`](plans/2026-04-21-ms-ablation-air-plan.md)
- 脚手架: `scripts/analysis/ms_ablation/{run_ablation.sh, compute_metrics.py, compare_ablation.py}`
- 后续: 阶段 D（RK process-noise），独立 spec 待起草。
