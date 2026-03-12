# NN Training Status

## Current Phase

- Phase: Wave 1 `px/py` weighted loss
- Status: running second weighted experiment on existing qmd-window dataset
- Started: 2026-03-07

## Current Best

- Baseline source: `data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/`
- `best_val_loss = 124.761978`
- `val px_rmse = 13.597350`
- `val py_rmse = 12.439825`
- `val pz_rmse = 5.886319`
- `px_py_rmse_mean = 13.018587`
- Best non-baseline candidate so far: `cap_20260307_020406_pxpy331_zscore_256_256_128`
- Candidate metrics:
  - `val px_rmse = 13.349086`
  - `val py_rmse = 12.285331`
  - `val pz_rmse = 5.414924`
  - `px_py_rmse_mean = 12.817209`
  - `test |p| rmse = 5.343966`
  - decision: current optimization leader; improvement about `1.55%`, capacity gains are present but small

## Running Now

- Run ID: `asym_20260307_120933_px431_zscore_256_256_128`
- Dataset: `formal_B115T3deg_qmdwindow/20260227_223007`
- Previous run result: `cap_20260307_020406_pxpy331_zscore_256_256_128`
- Previous run metrics:
  - `best_val_loss = 0.289612`
  - `val px_rmse = 13.349086`
  - `val py_rmse = 12.285331`
  - `val pz_rmse = 5.414924`
  - `px_py_rmse_mean = 12.817209`
  - decision: best result so far; capacity sweep now considered nearly saturated
- Objective: `LOSS_WEIGHTING=manual`, `LOSS_WEIGHTS=4,3,1`
- Scheduler: `plateau`
- Target normalization: `zscore`
- Hidden dims: `256,256,128`

## Next Queue

1. Compare asymmetric `4,3,1` against the symmetric `3,3,1` leader
2. If `px_py_rmse_mean` still moves by less than `1%`, stop loss/width tuning
3. Then switch to data-domain refinement or stronger px-focused objective
4. Keep the current leader as deployment candidate until a better run appears

## Blockers

- No dedicated GPU entry detected; current plan assumes local CPU batches
- Current repo has unrelated dirty changes outside this tracking directory and NN files; avoid reverting them
