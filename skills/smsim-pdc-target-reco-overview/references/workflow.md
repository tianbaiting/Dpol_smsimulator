# Workflow

## Scope

This overview skill covers the whole PDC target-reconstruction stack:

- target geometry and target-position assumptions
- `FragSimData` to reconstructed PDC track points
- the primary `analysis_pdc_reco` runtime framework
- NN dataset generation, training, export, and C++ runtime inference for the NN backend
- corrected-target production runs and batch evaluation
- legacy compatibility boundaries
- routing between the narrower PDC / NN skills

## Core Mental Model

Current reconstruction should be read as one main runtime framework plus one attached NN backend lifecycle.

The intended architecture in this repo is:

1. `PDCSimAna` reconstructs two 3D PDC points from Geant4 detector hits
2. `analysis_pdc_reco::PDCMomentumReconstructor` is the primary runtime façade for `PDC1/PDC2 -> target momentum`
3. runtime backends live under that framework:
   - RK
   - matrix fallback
   - reserved multidim interface
   - NN backend
4. the NN scripts and pipelines exist to train, export, and evaluate that NN backend

The active NN task in this repo is still:

1. reconstruct two 3D PDC points from Geant4 detector hits
2. use those 6 coordinates as features
3. regress proton momentum `(px, py, pz)` at the target
4. export the trained MLP into JSON for C++ forward-only inference

Legacy note:

- `TargetReconstructor` is not the preferred place for new work
- keep it as compatibility-only code until migration is complete

Treat requests accordingly. If the user says "target reconstruction", confirm whether they mean:

- target geometry / target position
- momentum at the target through the primary runtime framework
- the NN backend lifecycle
- the full chain from geometry through momentum inference

## End-To-End Data Flow

1. Geant4 input ROOT (`g4input`) is simulated by `sim_deuteron`
2. output ROOT (`g4output`) stores `FragSimData` and `beam`
3. `PDCSimAna` reconstructs PDC track points from detector responses
4. the primary runtime path goes through `PDCMomentumReconstructor`, which can dispatch to:
   - NN
   - RK
   - multidim placeholder
   - matrix fallback
5. the NN backend lifecycle uses the same 3D PDC points:
   - `build_dataset.C` extracts feature rows
   - `train_mlp.py` trains the MLP
   - `export_model_for_cpp.py` writes `model_cpp.json`
   - `reconstruct_sn_nn` is a backend-specific wrapper that runs the main framework in NN-only mode
6. `evaluate_reconstruct_sn_nn` computes reconstruction efficiency and momentum errors from `*_reco.root`

NN dataset details:

- features: `pdc1_x/y/z`, `pdc2_x/y/z`
- labels: `px_truth`, `py_truth`, `pz_truth`

The NN pipeline is not a separate reconstruction architecture. It is the model lifecycle for one backend of the main runtime framework.

Legacy compatibility path:

- old scripts and tests may still call `TargetReconstructor`
- do not interpret that as a second preferred architecture

## Default Anchors For Current Corrected-Target Flow

Use these as the current project anchors unless the user points to a different campaign:

- corrected-target geometry macro:
  - `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
- primary runtime framework:
  - `libs/analysis_pdc_reco/`
- NN production pipeline:
  - `scripts/analysis/run_sn124_nn_reco_pipeline.sh`
- corrected-target run note:
  - `docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg_notes.zh.md`

## Routing To Sub-Skills

### `smsim-pdc-track-reco`

Use when the issue is about:

- `FragSimData` parsing
- U/X/V layer usage
- smearing
- how `RecoEvent::tracks` gets filled
- wrong or missing PDC1 / PDC2 points

### `smsim-pdc-momentum-reco`

Use when the issue is about:

- the primary runtime framework in `analysis_pdc_reco`
- RK back-propagation
- matrix fallback contracts
- solver dispatch order or `RecoConfig`
- `PDCMomentumReconstructor` behavior
- compatibility boundaries with `TargetReconstructor`

### `smsim-nn-target-momentum`

Use when the issue is about:

- dataset generation from Geant4 output
- training split / hyper-parameters / metrics
- `model.pt`, `model_meta.json`, `model_cpp.json`
- `PDCNNMomentumReconstructor` runtime behavior
- backend-specific wrapper tools like `reconstruct_sn_nn`
- NN batch evaluation and corrected-target production runs

## Fast Validation Commands

```bash
python3 scripts/reconstruction/nn_target_momentum/train_mlp.py --help
python3 scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py --help
scripts/analysis/run_sn124_nn_reco_pipeline.sh --dry-run
```

If the environment exists, prefer:

```bash
micromamba run -n anaroot-env python scripts/reconstruction/nn_target_momentum/train_mlp.py --help
```

## Common Misreads To Avoid

- Do not treat the NN scripts as a separate reconstruction architecture; they support one backend of `analysis_pdc_reco`.
- Do not add new target-momentum runtime features to `TargetReconstructor`; it is compatibility-only.
- Do not assume target position can be recomputed ad hoc when the repo already has geometry and acceptance tooling defining it.
- Do not compare training metrics across campaigns without checking geometry consistency first.
- Do not mix corrected-target production evaluation with older zero-degree or placeholder-target setups.
