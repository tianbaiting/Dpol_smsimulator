# MS 消融实验系列

追踪 PDC 命中位置的多重库仑散射（multiple Coulomb scattering，MS）来源，逐层把 Geant4 世界里的材质换成真空，量化各层对 σ_y(PDC2) 的贡献，为 RK 重建的 process-noise 修复提供噪声幅度输入。

上游背景：`docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.pdf` §7.5.1。

## 迭代索引

| 阶段 | 切换 | 状态 | 文档 |
|---|---|---|---|
| A | 世界空气 → 真空 | ✅ 2026-04-21 完成 | [`ms_ablation_air_20260421_zh.md`](ms_ablation_air_20260421_zh.md) |
| B | + exit window → 真空 | 规划中 | — |
| C | + PDC 气体 MS 消除 | 规划中 | — |
| D | RK process-noise 修复 | 规划中 | — |

## 目录结构

```
ms_ablation/
├── README.md                          # 本索引
├── ms_ablation_air_20260421_zh.md     # 阶段 A 主 memo
├── specs/                             # 设计文档
│   └── 2026-04-21-ms-ablation-air-design.md
├── plans/                             # 实现计划
│   └── 2026-04-21-ms-ablation-air-plan.md
├── data/                              # 从 build/ 拷贝的产物快照
│   ├── compare.csv
│   ├── baseline_dispersion_summary.csv
│   └── no_air_dispersion_summary.csv
└── figures/                           # overlay 散点图
    ├── dispersion_overlay_tp0_0.png
    ├── dispersion_overlay_tp100_0.png
    └── dispersion_overlay_tp0_20.png
```

## 代码入口

- 脚本：`scripts/analysis/ms_ablation/{run_ablation.sh, compute_metrics.py, compare_ablation.py}`
- 单元测试：`scripts/analysis/ms_ablation/tests/`
- 几何宏：`configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg{,_noair}.mac`

## 快速复现（阶段 A）

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/analysis/ms_ablation/run_ablation.sh
# 产物: build/test_output/ms_ablation_air/{baseline,no_air,figures,compare.csv}
```

阶段 A 关键结果：air 贡献 PDC2 σ_y 的 **91–94%** → 下一步直接进入阶段 D（RK process-noise 修复）。
