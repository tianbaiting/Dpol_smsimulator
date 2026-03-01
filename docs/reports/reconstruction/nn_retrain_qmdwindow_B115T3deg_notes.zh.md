# NN 重建（QMD 窄窗口模型）在修正靶点后的执行说明

## 1. 本次采用的模型与几何
- 模型（不重新用 ypol/zpol 训练）：
  - `data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model.pt`
  - `data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model_meta.json`
  - `data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model_cpp.json`
- 几何（已写入正确靶点位置和角度）：
  - `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
  - 靶点参数：
    - `/samurai/geometry/Target/Position -1.2488 0.0009 -106.9458 cm`
    - `/samurai/geometry/Target/Angle 3.0 deg`

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
评估命令：
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
- 修正靶点几何后，重建链路在全量全事件下稳定运行，匹配效率显著提升到高统计可用水平（overall 67.09%）。
- Y-pol 误差更稳定，|p| RMSE 约 7.90 MeV/c。
- Z-pol 在 px/pz 上仍更难，但匹配效率达到 71.78%，可用于后续针对性优化。
