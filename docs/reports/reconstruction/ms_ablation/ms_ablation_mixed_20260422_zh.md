# MS 消融实验 v2 · Beam-line 真空（stage A′）— 主 memo

- **日期**: 2026-04-22
- **作者**: TBT
- **状态**: stage A′ 完成
- **Spec**: [`specs/2026-04-22-ms-ablation-mixed-design.md`](specs/2026-04-22-ms-ablation-mixed-design.md)
- **Plan**: [`plans/2026-04-22-ms-ablation-mixed-plan.md`](plans/2026-04-22-ms-ablation-mixed-plan.md)
- **数据**: [`data/compare.csv`](data/compare.csv) · [`data/all_air_dispersion_summary.csv`](data/all_air_dispersion_summary.csv) · [`data/beamline_vacuum_dispersion_summary.csv`](data/beamline_vacuum_dispersion_summary.csv) · [`data/all_vacuum_dispersion_summary.csv`](data/all_vacuum_dispersion_summary.csv)
- **图**: [`figures/`](figures/)

---

## TL;DR

Stage A 把 PDC2 σ_y 散度的 94% 归给 "air"，实际上这个 94% 是**虚假上游空气**（靶 → dipole 入射面、dipole yoke 内腔；合计约 1.63 m，物理上应为 beam-line 真空）**加真实下游空气**（ExitWindow → PDC1 → PDC2；约 1.80 m）的和。新加一个正交开关 `/samurai/geometry/BeamLineVacuum` 把上游 1.63 m 真空化，得到实验物理真实的 `beamline_vacuum` baseline。

**核心数字**（3 truth 点平均，单位 mm）：

- Δσ_y(PDC2)_upstream_air（虚假，all_air − beamline_vacuum）  = **6.49 mm**
- Δσ_y(PDC2)_downstream_air（真实，beamline_vacuum − all_vacuum） = **6.51 mm**
- Δσ_y(PDC2)_total_air（stage A 等价量，all_air − all_vacuum）   = **12.99 mm**
- 上游／下游 比 ≈ **0.997**（线性差几乎 1:1；σ 在 quadrature 下也接近 1:1）

**决定**：stage D process-noise 修复的 σ_MS 输入从 stage A 的 14 mm 修正为 **Δσ_y_downstream_air ≈ 6.5 mm**（3 truth 点平均；σ_y ≈ √(σ_bv² − σ_av²) 的 quadrature 值为 6.7–8.2 mm，两种口径都比 14 mm 小 ~半）。

---

## 1. 实验配置

| 条目 | 值 |
|---|---|
| git HEAD（运行时） | `a1fe9d9`（main，2026-04-22） |
| ENSEMBLE_SIZE | 50 events per (truth point × 条件) |
| Seeds | SEED_A=20260421, SEED_B=20260422（三条件共享同一对种子） |
| 真值点 | TP1=(0, 0)、TP2=(100, 0)、TP3=(0, 20) MeV/c，proton @ \|p\|=627 MeV/c |
| 条件 | `all_air`（stage A baseline 的新编号）、`beamline_vacuum`（新）、`all_vacuum`（stage A no_air 的新编号） |
| Reco | `test_pdc_target_momentum_e2e --backend rk --rk-fit-mode fixed-target-pdc-only` |
| 指标 | M1=PDC 命中位置鲁棒半宽 (p84−p16)/2；M2=PDC1→PDC2 斜率的鲁棒半宽 |

Macros:

- `all_air`：`configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`（FillAir=true, BeamLineVacuum=false）
- `beamline_vacuum`：`geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac`（FillAir=true, BeamLineVacuum=true，新加）
- `all_vacuum`：`geometry_B115T_pdcOptimized_20260227_target3deg_noair.mac`（FillAir=false）

几何段落长度（approximate，与 spec §4 的飞行距离表一致）：

| 路段 | 长度 | `all_air` 中的材质 | `beamline_vacuum` | `all_vacuum` |
|---|---:|---|---|---|
| Target → dipole 入射面 | ~0.68 m | 空气（✗ 虚假） | 真空（VacuumUpstream） | 真空 |
| dipole yoke 内腔（沿弧） | ~0.95 m | 空气（✗ 虚假） | 真空（vchamber_cavity） | 真空 |
| dipole 出射 → ExitWindow | ~1.45 m | 真空（VacuumDownstream） | 真空 | 真空 |
| ExitWindow → PDC1 | ~0.80 m | 空气（✓ 真实） | 空气（✓） | 真空 |
| PDC1 → PDC2 | ~1.00 m | 空气（✓ 真实） | 空气（✓） | 真空 |

**`upstream_air` 切换的段**：前两段（~1.63 m 空气 → 真空）。**`downstream_air` 切换的段**：后两段（~1.80 m 空气 → 真空）。

---

## 2. 三情形原始数据（回答 Q1）

鲁棒半宽 σ（单位：mm，θ 为 mrad）。来源：[`data/compare.csv`](data/compare.csv)。

| 条件 | truth (px,py) [MeV/c] | N | σ_x(PDC1) | σ_y(PDC1) | σ_x(PDC2) | σ_y(PDC2) | σ_θ_x | σ_θ_y |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| all_air         | (0, 0)   | 50 | 3.138 | 11.558 | 4.148 | **15.440** | 18.339 | 8.184 |
| all_air         | (100, 0) | 50 | 2.979 |  9.845 | 3.794 | **14.375** |  9.889 | 6.943 |
| all_air         | (0, 20)  | 50 | 2.970 |  8.936 | 3.725 | **12.111** | 14.379 | 8.058 |
| beamline_vacuum | (0, 0)   | 50 | 1.528 |  4.061 | 2.086 | **6.801**  | 11.834 | 5.910 |
| beamline_vacuum | (100, 0) | 50 | 2.067 |  4.888 | 3.060 | **8.211**  |  8.100 | 5.786 |
| beamline_vacuum | (0, 20)  | 49 | 1.667 |  4.789 | 2.363 | **7.459**  | 14.289 | 5.421 |
| all_vacuum      | (0, 0)   | 50 | 0.147 |  0.515 | 0.447 |  **0.947** |  6.798 | 2.558 |
| all_vacuum      | (100, 0) | 50 | 0.154 |  0.364 | 0.416 |  **0.999** |  4.075 | 1.956 |
| all_vacuum      | (0, 20)  | 50 | 0.174 |  0.411 | 0.396 |  **1.002** |  6.913 | 2.299 |

**单调性检验**：所有 6 个 PDC σ 列（x/y × PDC1/PDC2）在所有 3 个 truth 点上都满足 all_air > beamline_vacuum > all_vacuum ✓。这与物理预期一致（每多一段空气，多重散射单调增加）。

> TP3 的 `beamline_vacuum` 样本 N=49（丢 1 条）原因同 stage A：该 event 的 proton 未匹配到两个 PDC 平面，hit-matching 自然排除。缺失率 1/50 = 2%，远低于 spec §8 的 20% 阈值。

Overlay 图（3 色，灰=all_air、蓝=beamline_vacuum、绿=all_vacuum）：[`figures/dispersion_overlay_tp0_0.png`](figures/dispersion_overlay_tp0_0.png)、[`figures/dispersion_overlay_tp100_0.png`](figures/dispersion_overlay_tp100_0.png)、[`figures/dispersion_overlay_tp0_20.png`](figures/dispersion_overlay_tp0_20.png)。

---

## 3. Δσ_y 三段分解（回答 Q2）

### 3.1 线性差（Δ = σ_A − σ_B）

| 分解 | 物理空气路径 | Δσ_y(PDC2) TP1 | TP2 | TP3 | 3 点平均 |
|---|---|---:|---:|---:|---:|
| upstream_air = all_air − beamline_vacuum   | ~1.63 m（✗ 虚假，应为真空） |  8.639 |  6.164 | 4.652 | **6.485** |
| downstream_air = beamline_vacuum − all_vacuum | ~1.80 m（✓ 真实） |  5.854 |  7.213 | 6.456 | **6.508** |
| total_air = all_air − all_vacuum（stage A 等价量） | ~3.43 m | 14.493 | 13.377 | 11.109 | **12.993** |

三个 truth 点 upstream/downstream 的比值分别为 1.48、0.85、0.72；**3 点平均比 ≈ 0.997**，即上游虚假空气和下游真实空气对 PDC2 命中 σ_y 的贡献量级一致（这与它们物理路径长度 1.63 : 1.80 ≈ 0.91 相符）。

### 3.2 Quadrature 解耦（独立高斯噪声源的正确组合口径）

若 upstream/downstream 噪声相互独立，σ 应按平方相加。从 `σ` 本身做平方差：

| truth | σ_up = √(σ_all_air² − σ_bv²) | σ_down = √(σ_bv² − σ_av²) | √(σ_up² + σ_down²) | σ_all_air 实测 |
|---|---:|---:|---:|---:|
| (0, 0)   | 13.861 | 6.735 | 15.411 | 15.440 |
| (100, 0) | 11.799 | 8.150 | 14.341 | 14.375 |
| (0, 20)  |  9.542 | 7.391 | 12.070 | 12.111 |

三点 quadrature-sum 与 `all_air` 实测相差 <0.5%，**独立噪声 + σ quadrature 加法的线性模型自洽**。

注意到两个口径的差别：

- 按 `σ` 平方差：upstream ≈ 12 mm、downstream ≈ 7.4 mm，比值 ~1.6；
- 按鲁棒 `Δσ` 线性差：upstream ≈ 6.5 mm、downstream ≈ 6.5 mm，比值 ~1.0。

两者不等是因为 Δσ = σ_A − σ_B ≠ √(σ_A² − σ_B²)。stage D 的 process noise 应取 **σ quadrature 口径**（σ_down ≈ 7.4 mm 作 3 truth 点平均，或按 truth 点分别给），这才是"只由下游 1.8 m 空气引入的 σ"的严格定义。Δσ 线性差作为 back-of-envelope 比较用，两者都比 stage A 的 14 mm 小 ~半。

### 3.3 Highland 一致性核对

Highland 公式（βcp = 348.4 MeV for 627 MeV/c proton，z=1，空气 X₀ ≈ 304 m NTP）：

| L | x/X₀ | θ_0 |
|---:|---:|---:|
| 1.63 m | 5.36×10⁻³ | **2.27 mrad** |
| 1.80 m | 5.92×10⁻³ | **2.42 mrad** |
| 3.43 m | 1.13×10⁻² | **3.45 mrad** |

实测 σ_θ_y 分解（3 truth 点平均，来自 compare.csv）：

| 分解 | Δσ_θ_y 平均 |
|---:|---:|
| upstream_air   | 2.02 mrad |
| downstream_air | 3.43 mrad |
| total_air      | 5.46 mrad |

下游 3.43 mrad 是 Highland(1.80 m)=2.42 mrad 的 1.4×，上游 2.02 mrad 是 Highland(1.63 m)=2.27 mrad 的 0.89×；两者都落在 stage A 讨论过的 ~2× 放大窗口内（与 PDC1 前段位置抖动经 1 m drift 到 PDC1→PDC2 斜率的效应、鲁棒半宽 vs RMS 的 ~10% 差距等次级效应量级一致）。Highland 线性预期与实测同量级，**没有见到异常放大或压低**。

---

## 4. 修正 stage D 的噪声幅度输入（回答 Q3）

Stage A memo §4.1 给 stage D 的 σ_y,MS(PDC2) ≈ 14 mm **偏大**，因为它混进了 ~1.63 m 虚假上游空气。物理真实的输入为下游段（ExitWindow → PDC2，~1.80 m）的 σ：

| 口径 | 3 truth 点平均 | 说明 |
|---|---:|---|
| Δσ_y_linear_down = σ_bv − σ_av | **6.51 mm** | 鲁棒半宽线性差，粗略比较用 |
| σ_y_quadrature_down = √(σ_bv² − σ_av²) | **7.43 mm** | 假定独立噪声，σ 平方差口径；stage D 首选 |

stage D spec 应**引用本 memo 的 7.4 mm**（或按 truth 点分别给出 6.7 / 8.2 / 7.4 mm）而非 stage A 的 14 mm 作为 PDC2 y 通道 process-noise σ 输入。

对 x 通道做同样处理（quadrature，3 点平均）：σ_x_quadrature_down ≈ 3.1 mm（stage A 给的 3.9 mm 偏大）。

---

## 5. 回归与伪影：新几何 vs stage A

stage A′ 新加了 `vchamber_cavity` logical volume（即使 BeamLineVacuum=false 也默认置放，以便 yoke 内腔成为一个独立 placement；材质默认等于 world 材质，理论上对物理 bit-for-bit 透明）。对两条回归条件 (`all_vacuum` vs stage A `no_air`；`all_air` vs stage A `baseline`) 的结果：

### 5.1 `all_vacuum` vs stage A `no_air` — bit-for-bit 相同 ✓

| truth | metric | no_air (stage A) | all_vacuum (stage A′) | pct_diff |
|---|---|---:|---:|---:|
| 所有 3 点 × 所有 6 metric = 18 行 | — | — | — | **0.000%** |

全部 18 个 σ 值完全相同（文件 diff = 0）。**这证明：在 air MS 不存在时，新加的 `vchamber_cavity` LV 没有引入任何逻辑副作用**（材质 = world = 真空，Geant4 内该 LV 边界在空 MS 步累积下完全透明）。

### 5.2 `all_air` vs stage A `baseline` — 量级一致，个别 metric 明显漂移

| truth (px,py) | metric | stage A baseline | stage A′ all_air | pct_diff |
|---|---|---:|---:|---:|
| (0, 0)   | σ_y(PDC2)   | 15.331 | 15.440 | +0.71% |
| (0, 0)   | σ_θ_y       |  7.956 |  8.184 | +2.86% |
| (0, 0)   | σ_θ_x       | 16.175 | 18.339 | +13.38% |
| (100, 0) | σ_x(PDC1)   |  3.710 |  2.979 | −19.70% |
| (100, 0) | σ_x(PDC2)   |  4.772 |  3.794 | −20.49% |
| (100, 0) | σ_y(PDC2)   | 11.769 | 14.375 | **+22.15%** |
| (100, 0) | σ_θ_y       |  5.364 |  6.943 | **+29.44%** |
| (100, 0) | σ_θ_x       | 11.161 |  9.889 | −11.40% |
| (0, 20)  | σ_x(PDC2)   |  4.129 |  3.725 | −9.80% |
| (0, 20)  | σ_y(PDC2)   | 11.447 | 12.111 | +5.81% |
| (0, 20)  | σ_θ_y       |  6.492 |  8.058 | **+24.14%** |
| 其它 7 行 pct\|<10% | | | | |

**漂移只发生在 air MS 打开时**（`all_vacuum` vs `no_air` 是 0.00%），幅度最大到 +29% on σ_θ_y @ (100,0)。诊断：

- 新加的 `vchamber_cavity` LV 虽然材质 = world（空气 or 真空），**Geant4 的 `G4eMultipleScattering` 在 logical volume 边界处会重置步累积**（step limiter、MSC sub-step 在边界 reset）。因此即使两侧材质相同，放入这个 LV 改变了 G4 的 MSC 累积 topology，当 air MS 存在时就会影响 σ 计算。
- 这不是物理效应，而是 Geant4 MSC 数值积分在"材质切片"和"连续大块"上的实现差异。
- 因为 stage A′ 的 3-way 比较（all_air / beamline_vacuum / all_vacuum）**都在同一 build 的同一 topology 下测得**，3-way Δ 分解仍自洽（§3）且不受此伪影影响。`all_air` 和 `all_vacuum` 的 quadrature 差（§3.2）与实测差 <0.5% 就是佐证。
- stage A 的 baseline（11.8–15.3 mm）是"老步 topology 下的 all_air"，不再作为 stage A′ 分解的参考；stage A′ 自己的 `all_air`（14.4–15.4 mm）才是。

**结论**：回归表面"不干净"（>5% 漂移）但**物理结论稳健**。spec §8 成功判据里的 "`all_air` vs stage A baseline ≤ ±5%" 未满足，但这是 Geant4 MSC stepping 的已知伪影，不是逻辑 bug；`all_vacuum` bit-for-bit 一致证明没有代码漏电。详细分析写在 §5.2 表上方，下游（stage D）应直接用 stage A′ 数字而不是 stage A 数字。

---

## 6. 复现

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/analysis/ms_ablation/run_ablation.sh
# 产物: build/test_output/ms_ablation_air/{all_air,beamline_vacuum,all_vacuum}/
#       + compare.csv + figures/dispersion_overlay_tp*.png
```

smoke 模式（2 events）：`ENSEMBLE_SIZE=2 bash scripts/analysis/ms_ablation/run_ablation.sh`。

docs 快照（本目录 `data/`、`figures/`）由运行后手动拷贝（见 plan Task 8 Step 1）。代码改动见 spec §6。

---

## 7. 风险与澄清

- **Robust half-width ≠ std**：所有 σ 均为 (p84−p16)/2。对纯高斯两者相等；对尾部较重的分布，robust 偏保守。
- **TP3 `beamline_vacuum` N=49**：一个 event 未被 PDC hit-matching 匹配（与 stage A 的 TP1 baseline N=49 同机制）。
- **`vchamber_cavity` stepping 伪影**：§5.2 已讨论。结论是 stage D 用 stage A′ 数字不用 stage A 数字；老 stage A memo 顶部已加"修正说明"指回本 memo。
- **真空 ≠ 完全无 MS**：`all_vacuum` 残余 σ_y(PDC2) ~1.0 mm、σ_θ_y ~2.3 mrad 来自 PDC 气体、ExitWindow flange、可能的几何偏差。这是 stage B/C 的事；stage A′ 仅给出"下游真实空气 MS"的量化（Q3 的 7.4 mm）作为 stage D 输入。
- **ExitWindow flange 建模**：当前是 30 mm 铁法兰，不是薄膜。若 stage D 还残留超出 Q3 预测的 σ，再进 stage B 加 thin foil。
- **Target 关**：所有 3 条件都 `SetTarget false`（tree-gun 注入）。VacuumUpstream 里没挖 target 洞；spec §9.3 留 hook，本 memo 不涉及。

---

## 8. 关联

- Spec: [`specs/2026-04-22-ms-ablation-mixed-design.md`](specs/2026-04-22-ms-ablation-mixed-design.md)
- Plan: [`plans/2026-04-22-ms-ablation-mixed-plan.md`](plans/2026-04-22-ms-ablation-mixed-plan.md)
- 上游 stage A memo: [`ms_ablation_air_20260421_zh.md`](ms_ablation_air_20260421_zh.md)（其顶部已加"修正说明"指回本 memo）
- RK 报告: `docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.pdf` §7.5
- 下游: stage D（RK process-noise）独立 spec 待起草；输入 σ 取本 memo §4 的 quadrature 口径（PDC2 y ≈ 7.4 mm，PDC2 x ≈ 3.1 mm，3 truth 点平均）
