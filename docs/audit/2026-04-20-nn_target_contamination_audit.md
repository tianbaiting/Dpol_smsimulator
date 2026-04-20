# NN target-contamination audit — 2026-04-20

## Background

`DeutDetectorConstruction::fSetTarget` previously defaulted to `true`, silently
constructing an 80×80×30 mm Sn target into the simulation geometry even when
no `/samurai/geometry/Target/SetTarget` command appeared in the macro. For NN
training runs that fed tree-gun input (already post-target IMQMD/Faddeev
secondaries), this caused silent double scattering: particles passed through
the Sn target a second time inside Geant4, corrupting the (PDC hit) → (true
target momentum) mapping that the network learns.

The contaminating geometry macro is
`configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac`,
which contains no `SetTarget` command and therefore relied on the old default.
The fix — flipping the code default to `false`, making all bare macros
explicit, and adding a runtime warning — landed in branch
`fix/target-default-false`. A companion spec is at
`docs/superpowers/specs/2026-04-20-target-config-audit-design.md`.

## Method

1. Enumerate every g4output run dir under `data/simulation/g4output/`;
   inspect archived `_macros/*.mac` for the `/control/execute` geometry
   include.
2. Enumerate every NN training run dir under `data/nn_target_momentum/`;
   trace its dataset lineage back to a g4output (via
   `split_files/*.txt`, `split_files/*split.json`, `manifest.json`, or
   `model/model_meta.json`).
3. Classify by geometry basename.

## Classification rules

| Geometry include resolved to                          | Verdict      |
|-------------------------------------------------------|--------------|
| `*_target3deg.mac` (explicit `SetTarget false` inside)| SAFE         |
| bare `geometry_B115T_pdcOptimized_20260227.mac`       | CONTAMINATED |

## G4 output classification

| g4output dir                                  | Geometry include                                          | Verdict      |
|-----------------------------------------------|-----------------------------------------------------------|--------------|
| `eval_20260227_235751/`                       | `..._target3deg.mac`                                      | SAFE         |
| `eval_target3deg_smoke_20260228_ypol/`        | `..._target3deg.mac`                                      | SAFE         |
| `eval_target3deg_smoke_20260228_zpol/`        | `..._target3deg.mac`                                      | SAFE         |
| `polarization_validation_sn124_zpol_g060/`    | `..._target3deg.mac`                                      | SAFE         |
| `sn124_nn_B115T/`                             | `build/bin/.../geometry_B115T_pdcOptimized_20260227.mac`  | CONTAMINATED |

`sn124_nn_B115T/` is the feed for `formal_B115T3deg_qmdwindow/20260227_223007/`
— that formal run's `sim_root/nntrain_B115T_3deg_*.root` files and the derived
`dataset/dataset_{train,val,test}.csv` are all downstream of the bare geometry
(the build-output copy had no `SetTarget` line and relied on the old default).

## NN training-run classification

15 run dirs audited → **8 CONTAMINATED**, **6 SAFE**, **1 EMPTY**.

### CONTAMINATED (8)

All consume `formal_B115T3deg_qmdwindow/20260227_223007/dataset/*.csv`.

| Run dir                                                                            | Type              |
|------------------------------------------------------------------------------------|-------------------|
| `data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/`              | formal training   |
| `data/nn_target_momentum/optimization_runs/asym_20260307_120933_px431_zscore_256_256_128/` | hyper-param sweep |
| `data/nn_target_momentum/optimization_runs/cap_20260307_013843_pxpy331_zscore_256_128_64/` | hyper-param sweep |
| `data/nn_target_momentum/optimization_runs/cap_20260307_020406_pxpy331_zscore_256_256_128/` | hyper-param sweep |
| `data/nn_target_momentum/optimization_runs/lossw_20260307_013104_pxpy221/`         | hyper-param sweep |
| `data/nn_target_momentum/optimization_runs/lossw_20260307_013319_pxpy151/`         | hyper-param sweep |
| `data/nn_target_momentum/optimization_runs/lossw_20260307_013505_pxpy331/`         | hyper-param sweep |
| `data/nn_target_momentum/optimization_runs/lossw_20260307_013652_pxpy331_zscore/`  | hyper-param sweep |

### SAFE (6)

All consume `eval_20260227_235751/` (target3deg geometry).

| Run dir                                                           |
|-------------------------------------------------------------------|
| `data/nn_target_momentum/domain_matched_retrain/20260228_002611/` |
| `data/nn_target_momentum/domain_matched_retrain/20260228_002757/` |
| `data/nn_target_momentum/domain_matched_retrain/20260319_185940/` |
| `data/nn_target_momentum/domain_matched_retrain/20260319_185953/` |
| `data/nn_target_momentum/domain_matched_retrain/20260319_190004/` |
| `data/nn_target_momentum/domain_matched_retrain/20260319_215804/` |

### EMPTY (1)

Placeholder dir with no dataset and no trained model — nothing to classify.

| Run dir                                                           |
|-------------------------------------------------------------------|
| `data/nn_target_momentum/domain_matched_retrain/20260228_002554/` |

## Recommended actions

- **SAFE** runs: no action.
- **CONTAMINATED** runs: regenerate `sn124_nn_B115T/` (or the
  formal run's `sim_root`) with the fixed default / with the
  `_target3deg.mac` geometry, then retrain. Do not consume predictions from
  any of the 8 flagged models for physics results until retrained.
- The 7 `optimization_runs/*` hyper-parameter comparisons are also invalid
  until the underlying dataset is regenerated — re-running the sweep on the
  clean dataset may pick a different winner.

## Follow-up

Retraining and the hyper-parameter sweep re-run are out of scope for this
audit; schedule them as separate tasks referencing this report.
