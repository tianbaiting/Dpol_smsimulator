# NN Training Optimization Plan

## Goal

- 长期优化目标：优先压低验证集 `val_loss`，并重点提升 `px` / `py` 重建精度。
- 项目级主排名指标：`px_py_rmse_mean = (px_rmse + py_rmse) / 2`。
- 守门指标：`pz_rmse` 不允许相对当前 best 恶化超过 `10%`，并且候选模型必须可导出为 `model_cpp.json`。

## Current Baseline

- 基线模型：`data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/`
- `best_val_loss = 124.761978`
- `val px_rmse = 13.597350`
- `val py_rmse = 12.439825`
- `val pz_rmse = 5.886319`
- `test |p| rmse = 5.879779`

## Artifact Rules

- 长期项目文档放在 `docs/nn_training/`
- 每个训练 run 产物放在 `data/nn_target_momentum/optimization_runs/<run_id>/`
- 每个 run 至少保留：
  - `model/model.pt`
  - `model/model_meta.json`
  - `model/training_history.csv`
  - `model/model_cpp.json`

## Experiment Waves

### Wave 0: Project Setup

- 建立 `plan.md`、`statu.md`、`work.md`
- 把当前默认模型登记为长期项目基线
- 直接复用现有 qmd-window 数据集，不从 Geant4 全链条重新起跑

### Wave 1: px/py Weighted Loss

- 固定数据集：`data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/dataset/`
- 固定 hidden dims：`128,128,64`
- 固定训练上限：`epochs=120`
- 固定调度器：`LR_SCHEDULER=plateau`
- 先跑第一组权重：`LOSS_WEIGHTING=manual`, `LOSS_WEIGHTS=2,2,1`
- 如果首组有正收益，再扩展到：
  - `1.5,1.5,1`
  - `3,3,1`

### Wave 2: Target Normalization

- 仅在 Wave 1 最优权重上比较：
  - `TARGET_NORMALIZATION=none`
  - `TARGET_NORMALIZATION=zscore`

### Wave 3: Capacity Sweep

- 仅在当前最优 loss 配置上比较：
  - `128,128,64`
  - `256,128,64`
  - `256,256,128`

### Wave 4: Data-Domain Refinement

- 只有当前三波连续两波主指标提升都小于 `1%` 时才进入
- 候选路径：
  - 更窄 qmd-window
  - `run_domain_matched_retrain.sh`

## Promotion Rules

- 新 run 晋级为 current best 的条件：
  - `px_py_rmse_mean` 相比当前 best 改善至少 `3%`
  - `pz_rmse` 恶化不超过 `10%`
  - `model_cpp.json` 成功导出
  - NN 相关单测通过

## Operational Discipline

- `statu.md` 记录当前阶段、当前 best、正在跑的实验、下一批待跑
- `work.md` 只按时间顺序追加，不覆盖历史
- `plan.md` 只有在目标、阈值或实验矩阵变化时才更新
