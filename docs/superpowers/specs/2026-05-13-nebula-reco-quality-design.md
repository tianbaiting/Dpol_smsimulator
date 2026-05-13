# NEBULA reco quality — Δp_x 双峰根因追溯 + (px,py) 效率扫描

**Date**: 2026-05-13
**Author**: Baiting Tian
**Status**: design

## 1. 背景

`nn_breakup_phys_20260503_zh.tex` §5.4 报告了 NEBULA neutron 重建的 Δp_x 出现明显的双峰（中央 dip），并初步把成因指向 NEBULA bar 沿 X 方向 12 cm 步长的离散结构与下游 `n_reco_neutrons==1` filter 的叠加。同份报告也观察到 NEBULA 在 tight cut 子集上的命中率仅 18–22% (ypol) / 22–32% (zpol)，但没有系统量化命中率随 (p_x, p_y) 的变化，这使得下游 R = N(Δp_x>0)/N(Δp_x<0) 的 px 不对称 cut 难以判断是物理信号还是探测器接受度偏置。

本设计把这两个遗留问题作为一个独立调查闭环，**只读不改**（不改 NEBULAReconstructor / Converter），产出一份 zh 报告。

## 2. 产出物

单一报告：
`docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.tex` + `.pdf`，附 figures 子目录。

报告包含 Part A（双峰根因）和 Part B（效率扫描）两部分，章节明确独立但共享 §1 introduction。

不改任何重建代码；不重跑 ypol/zpol 物理分析；不动 RK 路径。

## 3. Part A：Δp_x 双峰根因追溯

### 3.1 数据来源
复用 nn_breakup_phys 已在 spana03 上跑好的 4.59M ypol breakup CSV + `joined.parquet`（如已存在则直接读；如缺，从 `scripts/analysis/nn_breakup_phys/03_analyze_r_breakup.py` 重生成）。

如需要新字段 `n_hits_in_cluster`（按 cluster 的 bar 数），扩展 `02_extract_observables.C` 增加该列，重跑一次 extraction。新增字段属于纯统计输出，不动重建。

### 3.2 报告章节
1. **现象复现**：取 NEBULA-hit 子集，画 Δp_x（rotated reaction-plane frame 和 lab frame 各一张），重现 §5.4 双峰。
2. **链路逐级展开**（每级一张图 + 一段源码引用）：
   - L1 — Geant4 SimData 层 (`fPrePosition`)：simdata-level x 分布连续。引用 `libs/smg4lib/src/data/src/NEBULASimDataConverter_TArtNEBULAPla.cc:80-86`（缓存 simdata.pos）。
   - L2 — Converter 把 X 替换为 bar 中心：hit-level x 分布出现 60 根 12 cm 步长的离散峰。引用 `NEBULASimDataConverter_TArtNEBULAPla.cc:156`（`x = prm->fPosition.x() + NEBULAParameter->fPosition.x()`）。
   - L3 — `NEBULAReconstructor::ApplyPositionSmearing` 5 mm 高斯，远小于 60 mm 半 bar 宽，离散性几乎没被抹掉。引用 `libs/analysis/src/NEBULAReconstructor.cc:295-305`。
   - L4 — Cluster 多 bar 合并：能量加权质心把边界事件拉回真值附近。引用 `NEBULAReconstructor.cc:206-218`（加权质心计算）。
   - L5 — 下游 `n_reco_neutrons==1` filter：砍掉多 bar 群体 → 中央 dip。引用 `scripts/analysis/nn_breakup_phys/03_analyze_r_breakup.py` 中的 NEBULA-hit 子集定义。
3. **关键拆分图**：把 Δp_x 按 `n_hits_in_cluster` (1 vs ≥2) 分开画，验证：
   - 单 bar cluster (n=1)：双峰，落在 ±60/L·|p| ≈ ±5 MeV/c 两端
   - 多 bar cluster (n≥2)：单峰，集中在 0 附近
4. **结论与未来选项**：
   - 当前不改的理由：保 ypol / zpol R 值与 elastic 报告的连续性
   - 列 3 种未来可选 fix 及代价：
     - (a) X smearing 改到 ≥ 60 mm — 简单但抹掉了 bar 的位置信息
     - (b) 保留 multi-bar cluster + 改 filter 为 `n_reco_neutrons≥1` — 改下游分析而非重建
     - (c) 用 bar 内沉积能量做亚 bar 重建 — 需要 simdata 级别信息，工作量大

### 3.3 不做的事
- 不调 NEBULAReconstructor 任何参数
- 不重跑 ypol/zpol 物理 R 表

## 4. Part B：truth-only (px,py) 效率扫描

### 4.1 执行环境
在 `labenpg` (96 cores, ENPG host case 已在 commit `83c7503` 处理, `~/workspace/dpol/smsimulator5.5` 与 main 同步) 上跑 Geant4 扫描。本地 PC 只负责生成 tree-gun 输入和分析。

**已在 2026-05-13 完成的环境核查**（dry run 10 neutron @200 MeV/c 端到端通过，41 KB 输出 root 含 tree + FragParameter + NEBULAParameter + RunParameter）：

- 1.15T 磁场表 `configs/simulation/geometry/filed_map/180703-1,15T-3000.table` (854 MB) 已在
- ROOT 6.36 + Geant4 11.3.2 在 conda env `anaroot-env` 中
- 几何构建正确（120 Neut bars + 24 Veto bars + PDC + Dipole 磁场全部成功放置）
- 已知良性 warning：`3deg_1.15T.mac:51-52` 用相对路径覆盖了 `geometry_B115T.mac` 的绝对路径，第二次 Update 时 `geometry/NEBULA_*.csv` 找不到打红色 warning，但首次 Update 已正确填充缓存的 map，几何最终正确。可选清理（删 51-52 行）作为单独 cosmetic commit。

**SSH 路由**: 必须用 `labenpg-hk`（ProxyJump 经 `tex.enpg.cn`），直连 labenpg 掉包。所有 ssh/scp 命令以 `labenpg-hk` 为 target。

**非交互环境激活 preamble**（脚本里硬编码，因 direnv 在非交互 ssh 中不触发）：
```bash
export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
eval "$($MAMBA_EXE shell hook --shell bash --root-prefix $MAMBA_ROOT_PREFIX)"
micromamba activate anaroot-env
source setup.sh
```

**G4 mac 命令规范**（dry run 期间踩坑确认）：
- gun 类型用 `/action/gun/Type Tree`（生产）或 `Pencil`（测试）
- 位置命令是 `/action/gun/Position x y z mm`，**不是** `SetBeamPosition`
- 动量用 `/action/gun/Energy + AngleX/AngleY`，**不是** `SetBeamMomentum`
- Tree gun: `/action/gun/tree/InputFileName <file>` + `/action/gun/tree/beamOn <N>`

执行流程（双向 rsync）：
1. 本机 → labenpg-hk：tree-gun 输入 root 文件 + macros
2. labenpg：批量 sim_deuteron + reconstruct
3. labenpg-hk → 本机：summary parquet（不传 g4output 大文件）
4. 本机：分析 + 出图 + 写 tex

### 4.2 Gun 配置
- 粒子：单 neutron，从 target (0,0,0) 出射
- 网格：`px ∈ [-200, 200]` step 20 MeV/c, `py ∈ [-100, 100]` step 10 MeV/c → 21 × 21 = 441 (px,py) 点
- pz 边缘化：每点对 `pz ∈ {550, 600, 627, 650, 700}` MeV/c 等权重采样（5 子点），每子点 2000 事件 → 每 (px,py) 点共 1×10^4 事件，全网共 4.41×10^6 事件
- Tree-gun 输入文件由 Python 生成（参考 `data/simulation/ips_wrl/sn124_3deg_1.15T_offset0/inputs/neutron_px_scan_pz627_offset0.root` 的 schema）
- 几何：复用 `configs/simulation/geometry/3deg_1.15T.mac` + offset0，但 `/samurai/geometry/Target/SetTarget false`（无 target 散射，保证 ε_geom 只取决于 NEBULA 接受度）

### 4.3 三层效率定义
- **ε_geom(px, py)** = N(truth ray 与 NEBULA Neut active 体积有交) / N_total
  - 用纯几何 ray-cast（Python + NEBULA_Detectors_Dayone.csv），**不走 Geant4**。单独脚本算一遍作 baseline。
- **ε_det(px, py)** = N(NEBULASimData 至少有一个 hit 且 ΣE > 1 MeV) / N_total
  - 从 sim_deuteron 输出的 NEBULASimData 直接统计。
- **ε_reco(px, py)** = N(reco neutrons ≥ 1) / N_total
  - 从 reconstruct_target_momentum / 直接调 NEBULAReconstructor 后的 reco root 统计。

### 4.4 条件效率与瓶颈识别
- ε_det|geom = ε_det / ε_geom — Geant4 物理探测效率
- ε_reco|det = ε_reco / ε_det — 重建链效率
- 通过条件效率定位 px 非对称的主导层

### 4.5 报告章节
1. 网格、gun 设定、几何配置摘要
2. ε_geom 二维热图 + py=0 切片 + px 边缘化曲线
3. ε_det / ε_reco 同样的二维热图 + 切片
4. 条件效率，识别瓶颈层
5. **把 (px,py) → ε 映射回 breakup 物理分布**：用现有 truth CSV 加权，估算 ypol/zpol R cut 在不同 cut 子集下的有效接受不对称
6. 结论：哪一段 px 范围的 NEBULA 接受度足以信任 R cut，哪一段需要 acceptance correction

### 4.6 探测器细节
- 只统计 Neut 主层（Layer 1 + 2）的 hit；Veto 层不计入 ε_det，但在 §4.2 同时记录 veto 命中数作 sanity check（用于后续 charged background 估算，但本报告不深入）

## 5. 工作量与排期
- Part A：1 个工作日（现有数据 + 脚本扩展）
- Part B：2–3 个工作日（gun 输入生成 + labenpg 排队跑 sim + 分析）

总计：3–4 个工作日。

## 6. 不做的事（明示）
- 不改 NEBULAReconstructor / Converter 任何代码
- 不重跑 ypol/zpol 物理分析
- 不动 RK 路径
- 不做 Veto 详细分析
- 不做 charged background / cross-talk 分析

## 7. 参考
- `nn_breakup_phys_20260503_zh.tex` §5.4 — 双峰现象与初步成因
- `libs/smg4lib/src/data/src/NEBULASimDataConverter_TArtNEBULAPla.cc` — bar 中心赋值逻辑
- `libs/analysis/src/NEBULAReconstructor.cc` — cluster + smearing 重建逻辑
- `configs/simulation/geometry/NEBULA_Detectors_Dayone.csv` — bar 位置表
- `data/simulation/ips_wrl/sn124_3deg_1.15T_offset0/macros/neutron_px_scan_pz627_offset0.mac` — 已有 neutron gun 扫描参考
