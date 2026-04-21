# MS 消融实验系列

追踪 PDC 命中位置的多重库仑散射（multiple Coulomb scattering，MS）来源，逐层把 Geant4 世界里的材质换成真空，量化各层对 σ_y(PDC2) 的贡献，为 RK 重建的 process-noise 修复提供噪声幅度输入。

上游背景：`docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.pdf` §7.5.1。

## 迭代索引

| 阶段 | 切换 | 状态 | 文档 |
|---|---|---|---|
| A | 世界空气 → 真空 | ✅ 2026-04-21 完成（几何不物理，见 A′）| [`ms_ablation_air_20260421_zh.md`](ms_ablation_air_20260421_zh.md) |
| A′ | + 靶→dipole/yoke 内腔真空（实验真实 baseline） | ✅ 2026-04-22 完成 | [`ms_ablation_mixed_20260422_zh.md`](ms_ablation_mixed_20260422_zh.md) |
| B | + exit window → 真空 | 规划中 | — |
| C | + PDC 气体 MS 消除 | 规划中 | — |
| D | RK process-noise 修复 | 规划中（输入来自 A′）| — |

## 目录结构

```
ms_ablation/
├── README.md                          # 本索引
├── ms_ablation_air_20260421_zh.md     # 阶段 A 主 memo（顶部有修正说明指向 A′）
├── ms_ablation_mixed_20260422_zh.md   # 阶段 A′ 主 memo（3-way 真空分解）
├── specs/                             # 设计文档
│   ├── 2026-04-21-ms-ablation-air-design.md
│   └── 2026-04-22-ms-ablation-mixed-design.md
├── plans/                             # 实现计划
│   ├── 2026-04-21-ms-ablation-air-plan.md
│   └── 2026-04-22-ms-ablation-mixed-plan.md
├── data/                              # 从 build/ 拷贝的产物快照
│   ├── compare.csv                               # 3-way（A′ 覆盖）
│   ├── all_air_dispersion_summary.csv            # A′
│   ├── beamline_vacuum_dispersion_summary.csv    # A′（新条件）
│   ├── all_vacuum_dispersion_summary.csv         # A′
│   ├── baseline_dispersion_summary.csv           # A（历史，保留）
│   └── no_air_dispersion_summary.csv             # A（历史，保留）
└── figures/                           # overlay 散点图（A′ 覆盖为 3-color）
    ├── dispersion_overlay_tp0_0.png
    ├── dispersion_overlay_tp100_0.png
    └── dispersion_overlay_tp0_20.png
```

## 代码入口

- 脚本：`scripts/analysis/ms_ablation/{run_ablation.sh, compute_metrics.py, compare_ablation.py}`
- 单元测试：`scripts/analysis/ms_ablation/tests/`
- 几何宏：`configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg{,_noair}.mac`

## 快速复现（阶段 A′，当前）

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/analysis/ms_ablation/run_ablation.sh
# 产物: build/test_output/ms_ablation_air/{all_air,beamline_vacuum,all_vacuum}/
#       + compare.csv + figures/
```

阶段 A 关键结果（历史，几何含虚假上游空气）：air 贡献 PDC2 σ_y 的 91–94%。

阶段 A′ 关键结果（当前有效）：下游真实空气（~1.8 m）贡献 PDC2 σ_y ≈ **6.5 mm（线性 Δ 口径）／7.4 mm（σ quadrature 口径）**（3 truth 点平均）；stage D 的 σ_MS 输入从 stage A 的 14 mm 修正为该值。上游虚假空气（~1.6 m）与下游真实空气对 σ_y(PDC2) 的贡献量级相近（线性 Δ 比 ≈ 1:1）。
