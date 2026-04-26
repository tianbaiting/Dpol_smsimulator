# MS 消融实验 v1 · Air 关闭 — 设计文档

- **日期**: 2026-04-21
- **作者**: TBT
- **状态**: 已通过 brainstorm，待生成实现计划
- **上游输入**: `docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.pdf`（§7.5 Geant4 涨落研究）
- **路线约定**: 增量（(d)）——本文档只描述第一步 "air off"；(b)/(c) 扩展路径列在末节。

---

## 1. 背景与动机

RK 重建状态报告（2026-04-16）§7.5.1 观察到：同一真值动量 p_truth = (p_x, p_y, p_z=627) MeV/c 重复投 50 次 Geant4，PDC2 命中位置的鲁棒半宽 σ_y 达到 8–15 mm，远大于 σ_x（3–4 倍），且大于 PDC 丝坐标分辨率 σ_wire ≈ 0.2 mm 的 10–75 倍。原因已定性归结为 SAMURAI 偶极磁场在 xz 平面内弯曲轨迹，y 分量完全不受磁场约束，每次多重库仑散射（multiple Coulomb scattering, MS）会在漂移长度上线性累积。

报告已完成的对照：关掉 Sn 靶后 σ_y 从 ~100 mm 塌到 ~10–15 mm。剩余 10–15 mm 里 **air / exit window / PDC gas** 三种材料的相对贡献**尚未归因**。

RK reco 端已知 Fisher σ_py 系统欠估 ~2×（报告表 16：G4/Fisher 比值 1.5–3.2），修复方向是向测量协方差注入过程噪声（Kalman/Glueckstern 做法）。要正确设定过程噪声的幅度，必须先知道每种材料层的 MS 贡献。本实验是这一修复路径的前置测量。

## 2. 研究问题

**Q1**: 报告 baseline 里剩余 σ_y ≈ 10–15 mm 中，靶点 → PDC 之间约 1 m 空气贡献多少？

**Q2**: 关掉空气后，PDC1→PDC2 的角度弥散 σ_θ_y 是否接近 Highland 公式对"仅 PDC 气体 + 窗口"的预测？

**Q3**: Δσ_y（baseline − no_air）是否大到可以下判断：air 主导 / window 主导 / PDC gas 主导？

## 3. 范围与边界

### 3.1 本次迭代做什么

- 只切换一个开关：Geant4 world 材质（`fFillAir`）由 `true` 改为 `false`（空气 → 真空）。
- 3 个真值点 × N=50 × 2 条件（baseline / no_air），共 300 events。
- 度量 M1（PDC 命中位置鲁棒半宽）+ M2（PDC1→PDC2 角度弥散）。

### 3.2 本次迭代不做什么

- **不**修改 C++ 源码（`DeutDetectorConstruction`, `ExitWindowC*`, `PDCConstruction` 保持原样）。`fFillAir` 已存在为宏开关（`/samurai/geometry/FillAir`），本次仅需新建宏文件。
- **不**关闭 exit window，**不**关闭 PDC 气体——这些属于 (b)/(c) 扩展。
- **不**重跑 RK reco 流水线（不做 step 2 的重建动量弥散比较）。Fisher 欠估是独立问题，已在主报告第 9 节列为 dE/dx + Cartesian 改造，不适合和 MS 归因混做。

### 3.3 增量路径（后续迭代，本文档不实现）

- (b): 关 exit window。需要给 `ExitWindowC2Construction` / `ExitWindowNConstruction` 增加"材质 → 真空"开关，或新建两个平行几何类。
- (c): 关 PDC 气体 MS。需要把 `PDCConstruction::fPDCMaterial` 的密度降到 1e−5 倍（保留命中能力同时消 MS），或替换成"理想薄片灵敏探测器 + 真空"方案。

## 4. 实验设计

### 4.1 真值点网格

三个点，与报告表 15 保持一致子集：

| 编号 | p_x (MeV/c) | p_y (MeV/c) | p_z (MeV/c) |
|---|---|---|---|
| TP1 | 0    | 0   | 627 |
| TP2 | +100 | 0   | 627 |
| TP3 | 0    | +20 | 627 |

理由：
- TP1 中心点与报告图 2/5 可直接对照；
- TP2 在 x 方向拉开，检验 MS 是否各向同性（x 方向应看到 σ_x 变化，y 方向不变）；
- TP3 在 y 方向拉开，保留一个非零 p_y 样本。

粒子种类：proton。Sn 靶：关闭（与报告 step 1 一致）。

### 4.2 Ensemble 大小

每个 (条件, 真值点) 组合 **N=50** event，种子 `seed ∈ {0..49}`。两条件使用相同种子序列以保严格对照。

### 4.3 指标

**M1: PDC 命中位置鲁棒半宽**

对每个 truth point 的 50 个 event，收集 PDC1 和 PDC2 命中的 lab 系 (x, y) 坐标，计算：

$$\sigma^{\text{robust}} = \frac{p_{84} - p_{16}}{2}$$

输出列：`sigma_x_pdc1_mm`, `sigma_y_pdc1_mm`, `sigma_x_pdc2_mm`, `sigma_y_pdc2_mm`。

**M2: PDC1→PDC2 角度弥散**

每个 event 算角度：

$$\theta_x = \frac{x_{\text{PDC2}} - x_{\text{PDC1}}}{z_{\text{PDC2}} - z_{\text{PDC1}}}, \qquad \theta_y = \frac{y_{\text{PDC2}} - y_{\text{PDC1}}}{z_{\text{PDC2}} - z_{\text{PDC1}}}$$

对 50 event 取同样的鲁棒半宽：`sigma_theta_x_mrad`, `sigma_theta_y_mrad`。

可以直接与 Highland 公式比较（用 637 MeV/c proton 通过 ~1 m 空气 + 窗口 + PDC 气体路径的 X/X0 估值）。

## 5. 架构（方案 B: 3×2 矩阵，两条件都重跑）

在 `build/test_output/ms_ablation_air/` 下并排跑 `baseline/` 与 `no_air/` 两个子目录，唯一差异是传给 `sim_deuteron` 的几何宏。

```
geometry_*.mac (air on/off)
        ↓ sim_deuteron (既有, 不动)
reco_<seed>.root × 50
        ↓ compute_metrics.py
pdc_hits.csv + angles.csv + dispersion_summary.csv
        ↓ compare_ablation.py
compare.csv + overlay PNG + memo
```

理由：
- 两条件同种子、同 build、同机器，唯一差异即 fFillAir；
- 脚手架可直接复用到 (b)/(c)——后续只要多加 `no_window/`, `no_pdc_gas/` 子目录；
- 避免"从别的实验摘基线"带来的漂移风险。

## 6. 目录结构

```
configs/simulation/DbeamTest/detailMag1to1.2T/
   geometry_B115T_pdcOptimized_20260227.mac         # 既有: FillAir true
   geometry_B115T_pdcOptimized_20260227_noair.mac   # 新建: FillAir false

scripts/analysis/ms_ablation/                       # 新建目录
   run_ablation.sh
   compute_metrics.py
   compare_ablation.py

build/test_output/ms_ablation_air/                  # gitignored, 运行产物
   baseline/
      reco_seed*.root            # 50 份
      pdc_hits.csv               # 3 truth points × 50 events = 150 行
      angles.csv                 # 同上
      dispersion_summary.csv     # 3 行 (每 truth point 一行)
   no_air/
      (同结构)
   compare.csv                   # 并排差分 (6 行: 3 truth × 2 条件)

docs/reports/reconstruction/ms_ablation/            # 新建, 交付物
   specs/
      2026-04-21-ms-ablation-air-design.md   # 本文档
   README.md                                 # 文件夹索引 (实现阶段创建)
   ms_ablation_air_20260421_zh.md            # 主 memo (实现阶段创建)
   figures/                                   # PNG (实现阶段创建)
   data/                                      # (实现阶段创建)
      compare.csv                            # 从 build/ 快照复制
```

## 7. 文件清单

### 7.1 新建

| 文件 | 内容 |
|---|---|
| `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_noair.mac` | 与 baseline 宏逐字节相同，唯一差异: `/samurai/geometry/FillAir true` → `false` |
| `scripts/analysis/ms_ablation/run_ablation.sh` | bash 驱动: 对两条件 × 3 truth points × 50 seeds 循环调 `sim_deuteron`；跑完调 `compute_metrics.py` 和 `compare_ablation.py` |
| `scripts/analysis/ms_ablation/compute_metrics.py` | 读 reco.root, 按 (truth_px, truth_py) 分组, 算 M1+M2, 写 CSV。**应复用既有 `scripts/analysis/extract_pdc_hits_from_reco.py` 的提取逻辑**（import 或参考），不另写 TTree 解析 |
| `scripts/analysis/ms_ablation/compare_ablation.py` | 读两组 summary, 出 compare.csv 和 per-truth-point overlay PNG |
| `docs/reports/reconstruction/ms_ablation/specs/2026-04-21-ms-ablation-air-design.md` | 本文档 |

### 7.2 实现阶段创建（运行完实验后）

| 文件 | 内容 |
|---|---|
| `docs/reports/reconstruction/ms_ablation/README.md` | 索引：指向 specs/ 和 memo |
| `docs/reports/reconstruction/ms_ablation/ms_ablation_air_20260421_zh.md` | 主 memo（见 §8 模板） |
| `docs/reports/reconstruction/ms_ablation/figures/*.png` | overlay 散点图 |
| `docs/reports/reconstruction/ms_ablation/data/compare.csv` | 快照 |

### 7.3 不改

- 所有 C++ 源码（`libs/smg4lib/`, `libs/sim_deuteron_core/`, `libs/analysis_pdc_reco/`）。
- 既有的 `scripts/analysis/run_ensemble_coverage.sh`。本次新脚本独立存在，便于后续扩展。

## 8. 运行流程与 memo 模板

### 8.1 运行

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/analysis/ms_ablation/run_ablation.sh
# 预计耗时 ~3 分钟 (3 truth × 50 seeds × 2 条件, 单 event 亚秒级)
```

产物落地在 `build/test_output/ms_ablation_air/`。手动/脚本末尾把 `compare.csv` 和 PNG 拷贝到 `docs/reports/reconstruction/ms_ablation/`。

### 8.2 Memo 应回答的三个问题

memo `ms_ablation_air_20260421_zh.md` 必须包含以下三节：

1. **Δσ_y 绝对值**：对 3 个 truth points 报告 Δσ_y (PDC1) 和 Δσ_y (PDC2)。
2. **与 Highland 预测比较**：用 Highland 公式估 637 MeV/c proton 在 ~1 m 空气中的 θ_MS ≈ ?（简单手算即可），和 Δσ_θ_y 对照。
3. **下一步判决**：
   - 若 Δσ_y > 0.5 × σ_y(baseline) → 空气主导，可以直接进入 RK process-noise 修复。
   - 若 Δσ_y < 0.2 × σ_y(baseline) → 空气贡献小，下一迭代扩展到 (b) 关 exit window。
   - 中间情形 → 仍按 (b) 继续验证。

## 9. 成功判据（spec 层面）

本 spec 完成的定义：

- [ ] 两条宏文件存在、内容仅差一行；
- [ ] `run_ablation.sh` 一次性跑完 baseline + no_air；
- [ ] `compute_metrics.py` 在 reco.root schema 检查通过后，正确输出 CSV；
- [ ] `compare_ablation.py` 生成 `compare.csv` 和 3 张 overlay PNG；
- [ ] memo markdown 存在，包含 §8.2 三节完整回答。

## 10. 风险与边界情况

### 10.1 对照严格性

- 两条件使用**相同种子序列** `seed ∈ {0..49}`。在 `run_ablation.sh` 中显式传递 `/run/initialize` 前的 `/random/setSeeds <seed> 0` 宏命令。
- 两条件用同一 build（同一 CMake cache）。`run_ablation.sh` 入口先打印 `git rev-parse HEAD` 和 build 日期作证据。

### 10.2 schema 校验

`compute_metrics.py` 启动时校验 reco.root 的分支存在：
- `truth_proton_p4` (truth 四动量)
- `RecoEvent.tracks[].pdc1_x`, `pdc1_y`, `pdc1_z`, `pdc2_x`, `pdc2_y`, `pdc2_z`（PDC1/PDC2 命中，lab 系）

缺任一分支 → 脚本 `sys.exit(1)` 并打印分支列表；**禁止**零填或跳过。

### 10.3 未到达 PDC 的事件

50 event 里可能有个别质子因为磁偏转极端或次级反应没到达 PDC2。用 robust half-width `(p_84 - p_16) / 2` 而非 std，自然免疫 ≤16% 外点；若 missing 率 > 20% 应在 memo 里单独列出。

### 10.4 RK reco 可用性（不在本次范围，但记录）

RK 反演模型本身不显式建模空气 MS（方程只有 Lorentz + B-field），所以关掉空气等于减少 G4 端噪声；reco 代码不需要改。这是一个干净的"只动噪声源"的实验。

### 10.5 基线数字与报告偏差

重跑的 baseline 数字可能与 2026-04-16 报告表 15 有 ±5% 量级偏差（不同 seed / 不同 build 日期 / PDC 丝位置可能有微调）。memo 必须在表格下方注明："本实验 baseline 为 2026-04-21 重跑，与 20260416 报告值偏差 X%, Y%, Z%"，列出具体数字。

如果偏差 > 15%，说明中间有不可忽略的代码或常数改动，需要先定位再跑消融。

## 11. 后续扩展（仅规划，本 spec 不实现）

**阶段 B (关 exit window)**: 给 `ExitWindowC2Construction` 和 `ExitWindowNConstruction` 各加一个 `fMaterial` 成员和对应 Messenger 命令，材质默认不变，新建 `_novacuum` 宏把它们改为 `G4_Galactic`。产物目录 `build/test_output/ms_ablation_air/no_window/`。

**阶段 C (关 PDC 气体 MS)**: 两种方案——(C1) 把 `mat_PDC` 密度降到 1e−5 倍，保留命中逻辑；(C2) 插入平行的"薄片灵敏探测器 + 真空 enclosure"，只在 MS 消融时启用。产物目录 `build/test_output/ms_ablation_air/no_pdc_gas/`。

**阶段 D (RK process-noise 修复)**: 基于 A/B/C 三阶段的 MS 归因结果，给 `PDCErrorAnalysis` 的测量协方差添加过程噪声项 Σ_MS，目标让 G4/Fisher 比值从 1.86 降到 ~1.0。独立设计文档。
