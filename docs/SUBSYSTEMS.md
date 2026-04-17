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
