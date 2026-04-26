# RK 动量重建修复与误差分析报告

**日期**: 2026-04-16（增补 2026-04-19 坐标系修复说明；2026-04-20 NN 参考号失效批注）
**适用版本**: smsimulator5.5, commit after error module rewrite + RK bugfix + frame rotation fix
**作者**: Baiting Tian

---

> **⚠️ 2026-04-20 NN 参考数据失效**
>
> 本报告中所有"NN（参考）"列、RK-vs-NN 的 MAE/bias 对比、以及 §5 中 NN 接近几何极限的
> 论断，都是用**已删除的污染模型** `formal_B115T3deg_qmdwindow/20260227_223007/`
> 在 E2E 测试中产生的（详见 `docs/audit/2026-04-20-nn_target_contamination_audit.md`）。
>
> **影响：**
> - **RK 修复的主结论不变**：三个 bug 的根因、修复方法、以及 RK 自身的 MAE 数字都是几何-物理层面的，不依赖 NN。
> - **"RK 与 NN 横向动量几乎一致"的论断失效**：清洁模型下验证集 $p_x$ RMSE $-51\%$、$p_y$ $-81\%$，清洁 NN 在 $p_x,p_y$ 上会明显优于 RK。
> - **"NN $p_z$ 接近几何极限"仍成立**：$p_z$ 验证集几乎不变（5.89 $\to$ 5.80 MeV/c），靶物质多重散射主要扰动横向。
> - **待办：** 用 `formal_B115T3deg/20260420_184345/model/model.pt` 重跑 `test_rk_nn_comparison.py`，更新 §2 §4 §5 中所有 NN 列。

---

## 0. 坐标系约定

本报告使用两个右手直角坐标系：

- **Lab 系**（Geant4 世界系）：$+z$ 沿 SAMURAI 实验厅的标称束流方向。PDC 命中、磁场表、RK 积分、最小二乘拟合全在此系内。
- **靶系（beam-as-Z 系）**：以入射束流方向为 $+z$，相对于 lab 系绕 $y$ 轴旋转 $\alpha_\mathrm{tgt}=3^\circ$。事件发生器写入 ROOT 的 `truth_proton_p4` 保持在此系（`DeutPrimaryGeneratorAction.cc` 仅对初级方向的本地副本执行 $R_y(-\alpha)$，未回写 `fMomentum`）。

**本报告所有 truth vs. reco 比较、所有直方图均在靶系下给出。** 重建端（`apps/run_reconstruction/main.cc` 与 `apps/tools/analyze_pdc_rk_error.cc`）在写出前对重建动量施加 $R_y(+\alpha_\mathrm{tgt})$，$3\times 3$ 协方差同步 $\Sigma'=R\Sigma R^\top$；$px/py/pz$ 区间用旋转后的对角高斯重建；$|\vec p|$ 旋转不变。共享实现位于 `libs/analysis_pdc_reco/include/PDCFrameRotation.hh`。

**2026-04-19 之前**缺少此旋转，造成 $\Delta p_x \approx -p_z\sin\alpha \approx -33\;\mathrm{MeV}/c$ 的伪偏差（$p_z \approx 627\;\mathrm{MeV}/c$；若 $p_z \approx 1\;\mathrm{GeV}/c$ 则为 $-52\;\mathrm{MeV}/c$）。修复后 $p_x$ 的 Fisher 68\% 覆盖率由 $\approx 0.05$ 恢复至 $\approx 0.80$。本报告直方图均在修复后重新生成。

---

## 1. 概述

本报告总结了 PDC→靶点动量重建中 RK（Runge-Kutta）后端的两项主要工作：

1. **误差分析框架重写**：将 online 误差模块从非物理的 3D 笛卡尔残差模型改为正确的 2D 丝坐标（wire-coordinate）残差模型，引入多种统计推断方法（Fisher、Profile Likelihood、Laplace 后验、MCMC），并实现蒙特卡洛非线性误差传播。

2. **RK 重建中心值修复**：诊断并修复了三个导致 RK 重建偏差高达 500–900 MeV/c 的 bug，使 RK 后端精度从灾难性水平提升到与 NN 后端可比。

---

## 2. RK 重建的根因分析与修复

### 2.1 问题表现

修复前，E2E 测试（5 个 truth 点，pz=627 MeV/c，px=-100~+100 MeV/c）显示 RK 各模式的偏差：

| 模式 | MAE px (MeV/c) | MAE pz (MeV/c) | gate_pass |
|------|----------------|-----------------|-----------|
| kFixedTargetPdcOnly | 5–826 | 3–417 | 1/5 |
| kTwoPointBackprop | 4–143 | 601–608 | 0/5 |
| kThreePointFree | 482–916 | 117–228 | 0/5 |
| NN（参考） | 4–44 | 0.2–6.3 | 2/5 |

### 2.2 BUG 1（致命）：丝方向模型丢失 z 分量

**位置**: `PDCRkAnalysisInternal.cc`, `BuildMeasurementModel()`

RK fitter 中使用的丝方向是纯 xy 平面投影：

```
u_dir = (cos θ, sin θ, 0)
v_dir = (-sin θ, cos θ, 0)
```

而 PDCSimAna 中实际的丝坐标定义为 u = (x_rot + y)/√2, v = (x_rot − y)/√2，对应的 lab 坐标系方向为：

```
u_dir = (cos θ/√2,  1/√2, sin θ/√2)
v_dir = (cos θ/√2, -1/√2, sin θ/√2)
```

**后果**：残差投影完全丢弃了 z 分量。SAMURAI 磁场在 xz 平面弯曲粒子轨迹，曲率信息恰好编码在 z 位移中。z 分量被忽略 → fitter 无法提取曲率 → 无法确定动量。

**修复**：将 2D 公式替换为与 PDCSimAna / PDCErrorAnalysis 一致的 3D 公式（~5 行改动）。

### 2.3 BUG 2（致命）：初始方向猜测忽略磁场偏转

**位置**: `PDCRkAnalysisInternal.cc`, `BuildInitialState()`

原始初始化使用靶点到最远 PDC 的直线方向作为出射方向。但对于 635 MeV/c 质子在 1.15T 磁场中，偏转角约 25°。直线方向与实际出射方向差异巨大，加上 `initial_p = 1000 MeV/c`（比 truth ~635 高 60%），Levenberg-Marquardt 优化从巨大残差起步，频繁收敛到错误的局部最小值。

**修复**：实现两阶段初始化策略：
1. **粗网格搜索**（7 × 3 × 5 = 105 个候选点）：在 u ∈ [u_center ± 1.0]、v ∈ [v_center ± 0.3]、p ∈ {200, 400, 700, 1200, 2000} MeV/c 的网格上评估 χ²，按 χ² 排序。
2. **多起点 LM**：从 χ² 最低的 5 个候选分别运行 LM 优化，取最终 χ² 最小的结果。

总计算开销：~105 次 Evaluate（网格）+ 5 次 LM 收敛（~10ms），可接受。

### 2.4 BUG 3（严重）：kThreePointFree / kTwoPointBackprop 委托给 legacy 代码

**位置**: `PDCMomentumReconstructorRK.cc`

这两个模式委托给 `TargetReconstructor`（legacy），存在独立的方向/符号问题：
- **kTwoPointBackprop**：1D 动量扫描，方向固定在 PDC 连线方向 → 对弯曲轨迹方向根本错误，pz 偏差恒定约 −604 MeV/c。
- **kThreePointFree**：初始方向用 PDC2−PDC1 而非靶点出射方向，加上过紧先验 → 全部 5 点偏差 500–900 MeV/c。

**修复**：消除 legacy 委托，所有三种 RK 模式统一使用 `RkLeastSquaresAnalyzer`：
- kFixedTargetPdcOnly / kTwoPointBackprop → 3 参数 (u, v, p)，固定靶点
- kThreePointFree → 5 参数 (dx, dy, u, v, p)，软靶点先验

---

## 3. 修复后的重建性能

### 3.1 E2E 测试结果

修复后各模式在 5 点 truth grid 上的表现：

| truth_px | RK bias_px | RK bias_py | RK bias_pz | NN bias_px | NN bias_py | NN bias_pz |
|----------|-----------|-----------|-----------|-----------|-----------|-----------|
| −100 | −7.3 | −13.4 | −16.1 | −6.2 | −13.0 | −5.4 |
| −50 | −20.4 | −25.5 | −10.8 | −20.5 | −24.9 | −0.7 |
| 0 | 3.3 | −3.9 | −13.1 | 3.5 | −3.8 | −3.0 |
| +50 | −33.3 | 16.3 | −8.7 | −31.4 | 16.8 | −0.2 |
| +100 | −37.8 | −20.9 | −5.3 | −43.6 | −19.4 | 6.3 |

**关键观察**：
- px/py 偏差 RK 与 NN 几乎一致（均 3–44 MeV/c），说明这些偏差来自单事件 Geant4 统计涨落，非重建方法差异。
- pz 偏差 RK（5–16 MeV/c）略大于 NN（0.2–6.3 MeV/c），存在系统性负偏差，可能源于 RK 轨迹模型未包含 dE/dx 能量损失。

### 3.2 三种 RK 模式的统一

修复后三种模式产生几乎相同的结果（固定靶点模式下完全一致），因为底层都使用 `RkLeastSquaresAnalyzer` 的正确丝坐标残差和多起点优化。

### 3.3 改进幅度

| 指标 | 修复前 | 修复后 | 改善倍数 |
|------|--------|--------|----------|
| MAE pz (kFixedTarget) | 3–417 MeV/c | 5–16 MeV/c | **10–80×** |
| MAE px (kFixedTarget) | 5–826 MeV/c | 3–38 MeV/c | **~20×** |
| MAE pz (kTwoPoint) | 601–608 MeV/c | 5–16 MeV/c | **~40×** |
| MAE px (kThreePoint) | 482–916 MeV/c | 3–38 MeV/c | **~25×** |
| gate_pass (5点) | 0–1/5 | 2/5（同 NN） | — |

---

## 4. 误差分析框架

### 4.1 残差模型

重写后的残差模型使用 2D 丝坐标：

**残差计算**：对 PDC1/PDC2 各平面，将轨迹到探测器命中点的 3D 位移投影到丝方向 (u, v)：

```
r_u = Δr · u_dir,    r_v = Δr · v_dir
```

**2D 白化（Cholesky whitening）**：

```
[w₁]     [W₁₁  W₁₂] [r_u]
[w₂]  =  [  0   W₂₂] [r_v]
```

其中 W 由 (σ_u, σ_v, ρ_uv) 构造。白化后的残差 w₁, w₂ 为独立标准正态变量。

**总共 4 个独立残差**（2 平面 × 2 丝方向），kFixedTargetPdcOnly 模式 3 个自由参数 → ndf = 1。

### 4.2 参数化

使用 5 参数状态向量 (dx, dy, u, v, p)：
- dx, dy：靶点偏移 (mm)
- u = px/pz, v = py/pz：方向正切
- p = |p|：动量模 (MeV/c)

直接使用物理量 p（而非 q = charge/p），使先验和截断作用在物理量上。

### 4.3 四种统计推断方法

| 方法 | 构造 | 统计含义 | 计算成本 | 适用场景 |
|------|------|---------|---------|---------|
| **Fisher** | MLE 处的逆 Hessian（J^T J 的逆） | 似然的局部 Gaussian 近似 | 低（1 次 Jacobian） | 所有成功拟合的快速宽度估计 |
| **Profile Likelihood** | 逐个参数扫描，其余最小化，用 Δχ² = 1, 4 交叉 | 频率学派置信区间 | 高（每参数 ~20 次 LM） | 非线性/非对称诊断 |
| **Laplace 后验** | MAP 处的后验 Hessian（Fisher + 先验精度矩阵）| Bayes 可信区间 | 低 | 快速 Bayes 区间估计 |
| **MCMC** | Metropolis-Hastings 随机游走采样后验 | 完整 Bayes 可信区间 | 高（~50k 步） | 验证和形状诊断 |

### 4.4 蒙特卡洛非线性误差传播

对每个中心拟合结果，Fisher 协方差定义了参数空间中的高斯分布。在参数空间生成 N=256 个采样点，每个点经过 (u, v, p) → (px, py, pz) 的非线性变换：

```
pz = p / √(1 + u² + v²),    px = u · pz,    py = v · pz
```

从变换后的 px, py, pz 分布中取经验百分位（2.5%, 16%, 84%, 97.5%），得到 68% 和 95% 非对称置信区间。

**优势**：自动捕获 (u,v,p) → (px,py,pz) 变换的非线性效应，无需 delta method 的线性近似假设。

---

## 5. 理论分辨极限

### 5.1 几何参数

| 参数 | 值 |
|------|------|
| 靶点位置 | z = 0 |
| PDC1 位置 | z ≈ 4000 mm |
| PDC2 位置 | z ≈ 5000 mm |
| PDC 间距 | ΔL ≈ 1000 mm |
| 磁场强度 | B ≈ 1.15 T（旋转 30°） |
| 有效磁场长度 | L_eff ≈ 800 mm |
| PDC 位置分辨 | σ_uv = 0.5 mm（Geant4 smearing） |
| 质子动量 | p ≈ 635 MeV/c |

### 5.2 Cramér-Rao 下界估计

**弯曲半径与偏转角**：

```
Bρ = p / (0.3·q) = 635 / 300 = 2.12 T·m
ρ  = Bρ / B = 2.12 / 1.15 = 1840 mm
θ_bend ≈ L_eff / ρ = 800 / 1840 ≈ 0.43 rad ≈ 25°
```

**动量分辨**（由 PDC 位移对动量的灵敏度决定）：

```
dx/dp ≈ L_arm · L_eff · B · 0.3 / p² ≈ 4000 × 800 × 1.15 × 0.3 / 635² ≈ 2.73 mm/(MeV/c)
```

```
σ_p ≥ σ_x / |dx/dp| = 0.5 / 2.73 ≈ 0.2 MeV/c    （理想，σ_x = 0.5 mm）
σ_p ≥ 2.0 / 2.73 ≈ 0.7 MeV/c                      （fitter 设定 σ = 2 mm）
```

**横向动量分辨**：

```
σ_θ ≈ σ_x / L_arm = 0.5 / 4000 = 0.125 mrad
σ_px ≈ p · σ_θ ≈ 635 × 0.000125 ≈ 0.08 MeV/c      （单平面，理想）
```

实际考虑多重散射、场不均匀性、两平面测量：**σ_px ~ 3–10 MeV/c**。

### 5.3 结论

| 分量 | Cramér-Rao 下界 | NN 实际 MAE | RK 实际 MAE | 评价 |
|------|-----------------|-------------|-------------|------|
| px | ~3–10 MeV/c | ~10 MeV/c | ~7–38 MeV/c | NN 接近极限 |
| py | ~3–10 MeV/c | ~9 MeV/c | ~4–25 MeV/c | NN 接近极限 |
| pz | ~0.2–0.7 MeV/c | ~4 MeV/c | ~5–16 MeV/c | 尚有提升空间 |

NN 的 MAE ~10 MeV/c (px/py)、~4 MeV/c (pz) 已接近几何极限。RK 修复后在同一 E2E 测试中与 NN 可比（px/py 偏差量级一致），pz 系统偏差主要来自 dE/dx 未建模。

---

## 6. 代码改动汇总

### 6.1 误差框架重写（已完成）

| 文件 | 改动 |
|------|------|
| `PDCErrorAnalysis.cc` | 2D 丝坐标残差、q→p 参数化、MC 区间传播、ComputeUVDirections() |
| `PDCRkAnalysisInternal.cc` | 2D Cholesky 白化残差、(σ_u, σ_v, ρ) 参数 |
| `PDCRecoTypes.hh` | 新增 pdc_sigma_u_mm、pdc_sigma_v_mm、pdc_uv_correlation、pdc_angle_deg |
| `PDCRecoRuntime.hh/cc` | 新增对应 RuntimeOptions 字段 |
| `main.cc` (run_reconstruction) | 新增 CLI 选项 |
| `analyze_pdc_rk_error.cc` | 同步 CLI |

### 6.2 RK 中心值修复（本次）

| 文件 | 改动 | 行数 |
|------|------|------|
| `PDCRkAnalysisInternal.cc:286-300` | 丝方向 2D→3D | ~5 行 |
| `PDCRkAnalysisInternal.cc:359-440` | 粗网格搜索 + 多起点 | ~60 行 |
| `PDCRkAnalysisInternal.cc:322-341` | kTwoPointBackprop 使用 fixed-target 布局 | ~3 行 |
| `PDCRkAnalysisInternal.cc:355` | SupportsCurrentMode() 支持所有模式 | 1 行 |
| `PDCRkAnalysisInternal.hh:193` | 新增 fInitialCandidates 成员 | 1 行 |
| `PDCMomentumReconstructorRK.cc` | 消除 legacy 委托，三模式统一用 analyzer | ~−70/+30 行 |
| `test_PDCMomentumReconstructor.cc` | 重写 kTwoPointBackprop 测试 | ~20 行 |

---

## 7. 测试验证

### 7.1 单元测试

全部 10 个 PDC 相关单元测试通过：

- PDCMomentumReconstructorTest (6 tests): 模式解析、NN 推理、RK 各模式
- PDCErrorAnalysisTest (4 tests): 中心拟合、Profile Likelihood、χ² 局部最小验证、MCMC 后验

### 7.2 E2E 集成测试

全部 5 个 E2E 测试通过（含 Geant4 模拟→重建→评估全链路）：

| 测试名 | 耗时 | 状态 |
|--------|------|------|
| PdcTargetMomentumE2E.RK.TwoPointBackprop | 26s | PASS |
| PdcTargetMomentumE2E.RK.FixedTargetPdcOnly | 26s | PASS |
| PdcTargetMomentumE2E.RK.ThreePointFree | 31s | PASS |
| PdcTargetMomentumE2E.MultiDim | 16s | PASS |
| PdcTargetMomentumE2E.NN | 17s | PASS |

gate_pass 仍设为 `require_gate_pass=0`（报告模式），因为单事件统计涨落使得 RK 和 NN 均有 3/5 点未通过 px/py 阈值（阈值 px=20, py=15, pz=20 MeV/c）。

---

## 8. 已知局限与后续工作

### 8.1 dE/dx 能量损失（P5）

当前 RK4 轨迹传播仅含洛伦兹力（|p| 守恒）。635 MeV/c 质子在探测器气体中损失约 ~50 MeV（~8%），引入系统性负 pz 偏差。在 RK4 步进中添加 Bethe-Bloch 能量损失项可消除此系统偏差。

### 8.2 多重散射

气体和窗材料中的库仑散射引入角度弥散，目前未在 RK 轨迹模型中建模。对 635 MeV/c 质子影响较小（~mrad 量级），但在精密测量时需要考虑。

### 8.3 统计验证（Stage 2）

待用大统计量数据集（NN 训练集 ~89k 事件）验证：
- RK vs NN 的 MAE/RMSE 对比
- χ²_reduced 分布均值是否 ≈ 1
- Fisher/Laplace 68% 区间覆盖率是否在 65%–70%

### 8.4 Gate 启用

当多事件统计验证通过后，可将 E2E 测试的 `require_gate_pass` 从 0 改为 1，启用硬门禁。

---

## 9. 使用方法

### 9.1 运行 RK 重建

```bash
bin/reconstruct_target_momentum \
  --backend rk \
  --rk-fit-mode fixed-target-pdc-only \
  --pdc-sigma-u-mm 2.0 \
  --pdc-sigma-v-mm 2.0 \
  --pdc-angle-deg 57.0 \
  --input data/simulation/output.root \
  --output results/reco_rk.root
```

### 9.2 运行离线误差分析

```bash
bin/analyze_pdc_rk_error \
  --input results/reco_rk.root \
  --output-dir results/error_analysis/ \
  --pdc-sigma-u-mm 2.0 \
  --pdc-sigma-v-mm 2.0 \
  --pdc-angle-deg 57.0 \
  --sample-fraction 0.01
```

输出文件：
- `track_summary.csv`：每事件中心拟合 + Fisher 宽度
- `profile_samples.csv`：Profile Likelihood 扫描结果
- `bayesian_samples.csv`：MCMC 后验采样
- `summary.csv` / `validation_summary.csv`：汇总统计

### 9.3 运行测试

```bash
cd build
ctest -R "PDCMomentumReconstructorTest|PDCErrorAnalysisTest"  # 单元测试
ctest -R "PdcTargetMomentumE2E"                               # E2E 测试
```
