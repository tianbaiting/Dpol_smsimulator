# NN Training Work Log

## 2026-03-07

### Project bootstrap

- 建立长期项目目录：`docs/nn_training/`
- 建立三份项目文件：`plan.md`、`statu.md`、`work.md`
- 把当前默认 qmd-window 模型登记为长期项目基线

### Baseline registration

- Baseline model path: `data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/`
- Recorded baseline metrics:
  - `best_val_loss = 124.761978`
  - `val px_rmse = 13.597350`
  - `val py_rmse = 12.439825`
  - `val pz_rmse = 5.886319`
  - `test |p| rmse = 5.879779`

### Experiment launch

- Run ID: `lossw_20260307_013104_pxpy221`
- Plan:
  - reuse existing train/val/test CSV split
  - run weighted loss with `LOSS_WEIGHTING=manual`
  - use `LOSS_WEIGHTS=2,2,1`
  - keep `TARGET_NORMALIZATION=none`
  - enable `LR_SCHEDULER=plateau`

### Expected outputs

- `data/nn_target_momentum/optimization_runs/lossw_20260307_013104_pxpy221/model/model.pt`
- `data/nn_target_momentum/optimization_runs/lossw_20260307_013104_pxpy221/model/model_meta.json`
- `data/nn_target_momentum/optimization_runs/lossw_20260307_013104_pxpy221/model/training_history.csv`
- `data/nn_target_momentum/optimization_runs/lossw_20260307_013104_pxpy221/model/model_cpp.json`

### Experiment result

- Run ID: `lossw_20260307_013104_pxpy221`
- Result summary:
  - `best_val_loss = 237.429550`
  - `val px_rmse = 13.531949`
  - `val py_rmse = 12.451602`
  - `val pz_rmse = 5.998051`
  - `val px_py_rmse_mean = 12.991775`
  - `test |p| rmse = 5.977987`
- Comparison to baseline:
  - `px_py_rmse_mean`: `13.018587 -> 12.991775`
  - improvement: about `0.2%`
  - verdict: not enough to replace baseline, but worth continuing weighted-loss sweep

### Next experiment

- Run ID: `lossw_20260307_013319_pxpy151`
- Plan:
  - same dataset and split
  - `LOSS_WEIGHTING=manual`
  - `LOSS_WEIGHTS=1.5,1.5,1`
  - `TARGET_NORMALIZATION=none`
  - `LR_SCHEDULER=plateau`

### Experiment result

- Run ID: `lossw_20260307_013319_pxpy151`
- Result summary:
  - `best_val_loss = 181.785599`
  - `val px_rmse = 13.619540`
  - `val py_rmse = 12.375848`
  - `val pz_rmse = 6.113642`
  - `val px_py_rmse_mean = 12.997694`
  - `test |p| rmse = 6.109436`
- Comparison to baseline:
  - `px_py_rmse_mean`: `13.018587 -> 12.997694`
  - improvement: about `0.16%`
  - verdict: py improved, but px regressed and pz worsened; still not enough to replace baseline

### Next experiment

- Run ID: `lossw_20260307_013505_pxpy331`
- Plan:
  - same dataset and split
  - `LOSS_WEIGHTING=manual`
  - `LOSS_WEIGHTS=3,3,1`
  - `TARGET_NORMALIZATION=none`
  - `LR_SCHEDULER=plateau`

### Experiment result

- Run ID: `lossw_20260307_013505_pxpy331`
- Result summary:
  - `best_val_loss = 346.500549`
  - `val px_rmse = 13.482588`
  - `val py_rmse = 12.364382`
  - `val pz_rmse = 5.960477`
  - `val px_py_rmse_mean = 12.923485`
  - `test |p| rmse = 5.964884`
- Comparison to baseline:
  - `px_py_rmse_mean`: `13.018587 -> 12.923485`
  - improvement: about `0.73%`
  - verdict: currently the best weighted-loss candidate, but still below the `3%` promotion threshold

### Next experiment

- Run ID: `lossw_20260307_013652_pxpy331_zscore`
- Plan:
  - same dataset and split
  - reuse best Wave 1 weight setting
  - `LOSS_WEIGHTING=manual`
  - `LOSS_WEIGHTS=3,3,1`
  - `TARGET_NORMALIZATION=zscore`
  - `LR_SCHEDULER=plateau`

### Experiment result

- Run ID: `lossw_20260307_013652_pxpy331_zscore`
- Result summary:
  - `best_val_loss = 0.289762`
  - `val px_rmse = 13.364451`
  - `val py_rmse = 12.286305`
  - `val pz_rmse = 5.407196`
  - `val px_py_rmse_mean = 12.825378`
  - `test |p| rmse = 5.371828`
- Comparison to baseline:
  - `px_py_rmse_mean`: `13.018587 -> 12.825378`
  - improvement: about `1.48%`
  - verdict: current best candidate so far; still below `3%` promotion threshold, but clearly better than pure weighted-loss runs without target normalization

### Next experiment

- Run ID: `cap_20260307_013843_pxpy331_zscore_256_128_64`
- Plan:
  - same dataset and split
  - keep current best loss setup
  - `LOSS_WEIGHTING=manual`
  - `LOSS_WEIGHTS=3,3,1`
  - `TARGET_NORMALIZATION=zscore`
  - `LR_SCHEDULER=plateau`
  - `HIDDEN_DIMS=256,128,64`

### Experiment result

- Run ID: `cap_20260307_013843_pxpy331_zscore_256_128_64`
- Result summary:
  - `best_val_loss = 0.289754`
  - `val px_rmse = 13.371129`
  - `val py_rmse = 12.286591`
  - `val pz_rmse = 5.383754`
  - `val px_py_rmse_mean = 12.828860`
  - `test |p| rmse = 5.323196`
- Comparison to current leader:
  - leader `px_py_rmse_mean = 12.825378`
  - current run `px_py_rmse_mean = 12.828860`
  - verdict: main metric slightly worse, but `pz` and test norm improved; keep as useful reference, not new leader

### Next experiment

- Run ID: `cap_20260307_020406_pxpy331_zscore_256_256_128`
- Plan:
  - same dataset and split
  - keep current best loss setup
  - `LOSS_WEIGHTING=manual`
  - `LOSS_WEIGHTS=3,3,1`
  - `TARGET_NORMALIZATION=zscore`
  - `LR_SCHEDULER=plateau`
  - `HIDDEN_DIMS=256,256,128`

### Experiment result

- Run ID: `cap_20260307_020406_pxpy331_zscore_256_256_128`
- Result summary:
  - `best_val_loss = 0.289612`
  - `val px_rmse = 13.349086`
  - `val py_rmse = 12.285331`
  - `val pz_rmse = 5.414924`
  - `val px_py_rmse_mean = 12.817209`
  - `test |p| rmse = 5.343966`
- Comparison to previous leader:
  - previous leader `px_py_rmse_mean = 12.825378`
  - current run `px_py_rmse_mean = 12.817209`
  - improvement: about `0.06%` over the previous leader
  - verdict: new best candidate, but capacity sweep gain is now very small

### Next experiment

- Run ID: `asym_20260307_120933_px431_zscore_256_256_128`
- Plan:
  - same dataset and split
  - keep current best architecture and normalization
  - switch to asymmetric px-heavy weighting
  - `LOSS_WEIGHTING=manual`
  - `LOSS_WEIGHTS=4,3,1`
  - `TARGET_NORMALIZATION=zscore`
  - `LR_SCHEDULER=plateau`
  - `HIDDEN_DIMS=256,256,128`
