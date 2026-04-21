# NN 重建（QMD 窄窗口模型）在修正靶点后的执行说明

> **⚠️ 2026-04-20 更新 —— 旧 qmd-window 模型因"靶物质污染"已作废**
>
> 原来的 `formal_B115T3deg_qmdwindow/20260227_223007/` 训练使用的 Geant4 模拟依赖 `DeutDetectorConstruction::fSetTarget` 的旧默认值 `true`，而输入是 Tree-gun 已经穿过靶的 IMQMD 次级粒子 —— 导致 Sn 靶被 Geant4 二次散射，训练数据被污染。详见 `docs/audit/2026-04-20-nn_target_contamination_audit.md`。
>
> **污染被删除的训练（8 个）：** 旧 `formal_B115T3deg_qmdwindow/20260227_223007/` + 全部 7 个 `optimization_runs/*`。
>
> **清洁版重训结果（`data/nn_target_momentum/formal_B115T3deg/20260420_184345/`，同样采样 R=100、pz [560,680]）：**
>
> | Val RMSE (MeV/c) | 污染旧版 | 清洁新版 | 改善 |
> |------|---------|---------|------|
> | px   | 13.60   | **6.70**| −51% |
> | py   | 12.44   | **2.40**| −81% |
> | pz   | 5.89    | 5.80    | −2%  |
> | \|p\| | 5.88    | 6.08    | +3% (pz 主导) |
>
> 横向动量 px/py 大幅改善，符合物理直觉（Sn 靶多次散射主要扰动横向分量）。
>
> **下方第 4 节的 `evaluate_reconstruct_sn_nn` 数字基于已作废的污染模型，需用新模型重新做一次 `eval_20260227_235751/` 的推理再更新。** 几何链路本身（target3deg.mac）是干净的，只是权重文件需替换。

## 1. 本次采用的模型与几何
- **最新可用清洁模型（2026-04-20 重训）：**
  - `data/nn_target_momentum/formal_B115T3deg/20260420_184345/model/model.pt`
  - `data/nn_target_momentum/formal_B115T3deg/20260420_184345/model/model_meta.json`
  - ~~`data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/`（已删除：污染）~~
- 几何（已写入正确靶点位置和角度）：
  - `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
  - 靶点参数：
    - `/samurai/geometry/Target/Position -1.2488 0.0009 -106.9458 cm`
    - `/samurai/geometry/Target/Angle 3.0 deg`
  - **`SetTarget false`** —— 关键修复，在清洁数据流下靶物质必须关闭。

## 2. 数据流程（Data Flow）
1. 复用已有的 Geant4 输入（QMD 转换结果）：
   - `data/simulation/g4input/y_pol/phi_random/...`
   - `data/simulation/g4input/z_pol/b_discrete/...`
2. 运行脚本：
   - `scripts/analysis/run_sn124_nn_reco_pipeline.sh`
3. 脚本为每个输入 root 生成 Geant4 宏：
   - `data/simulation/g4output/eval_20260227_235751/_macros/*.mac`
4. 每个宏执行目标几何：
   - `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
5. 输出 Geant4 模拟与重建结果，并执行评估。

## 3. 写入位置（写的东西分别在哪里）
- Geant4 输出（覆盖后的正式目录）：
  - `data/simulation/g4output/eval_20260227_235751/`
- 重建输出（覆盖后的正式目录）：
  - `data/reconstruction/eval_20260227_235751/`
- 生成的 Geant4 运行宏：
  - `data/simulation/g4output/eval_20260227_235751/_macros/`
- 文档 beamer：
  - `docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg.tex`
  - `docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg.pdf`

## 4. 评估结果（全文件 + 全事件，beamOn=0）

> **⚠️ 以下数字已作废 —— 由污染模型产生**
>
> 本节的 `evaluate_reconstruct_sn_nn` RMSE 数字是用**已删除的污染模型**
> (`formal_B115T3deg_qmdwindow/20260227_223007/model/`) 对干净的
> `eval_20260227_235751/` 重建文件推理得到的。`eval_*` 几何链路本身（target3deg.mac，
> 显式 `SetTarget false`）是干净的 —— 但用于推理的权重文件是在污染数据集上训练的，
> 所以这些 px RMSE（38.7 / 37.9 / 53.2 MeV/c）同时反映了**训练污染 + 推理样本分布**。
>
> **待办：** 用新清洁模型 `data/nn_target_momentum/formal_B115T3deg/20260420_184345/model/model.pt`
> 重新跑一次 `data/reconstruction/eval_20260227_235751/` 下的全量推理 + evaluate，
> 再更新下面三张 Overall / Y-pol / Z-pol 表。预期 px、py RMSE 会有明显改善
> （验证集上 px −51%、py −81%），pz、|p| 量级不变。

评估命令（将来重跑时仍沿用）：
- `build/bin/evaluate_reconstruct_sn_nn --input-dir data/reconstruction/eval_20260227_235751`
- `build/bin/evaluate_reconstruct_sn_nn --input-dir data/reconstruction/eval_20260227_235751/y_pol`
- `build/bin/evaluate_reconstruct_sn_nn --input-dir data/reconstruction/eval_20260227_235751/z_pol`

### Overall
- files=156
- events_total=5,413,747
- matched_events=3,632,185
- reco_efficiency_vs_truth=67.0919%
- RMSE:
  - px=38.7268 MeV/c
  - py=11.2901 MeV/c
  - pz=11.9982 MeV/c
  - |p|=8.42973 MeV/c

### Y-pol
- files=16
- events_total=5,178,328
- matched_events=3,463,201
- reco_efficiency_vs_truth=66.8787%
- RMSE:
  - px=37.8827 MeV/c
  - py=11.2611 MeV/c
  - pz=11.1051 MeV/c
  - |p|=7.90284 MeV/c

### Z-pol
- files=140
- events_total=235,419
- matched_events=168,984
- reco_efficiency_vs_truth=71.7801%
- RMSE:
  - px=53.1511 MeV/c
  - py=11.8694 MeV/c
  - pz=23.8083 MeV/c
  - |p|=15.7297 MeV/c

## 5. 重建效果结论
- 修正靶点几何后，重建链路在全量全事件下稳定运行，匹配效率显著提升到高统计可用水平（overall 67.09%）—— **匹配效率和第 4 节 RMSE 数字无关，这部分结论仍成立**。
- Y-pol / Z-pol 的 RMSE 对比结论（|p|、px、pz 谁更好谁更差）**依赖污染模型**，在重新推理后再更新。
- 2026-04-20 清洁重训后的验证集 RMSE 改善见顶部表：px −51%、py −81%、pz / |p| 基本不变。全量 evaluate 预期与该趋势一致。
