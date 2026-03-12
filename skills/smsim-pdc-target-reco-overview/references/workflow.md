# Workflow

## Scope

This overview skill covers the whole PDC target-reconstruction stack:

- target geometry and target-position assumptions
- `FragSimData` to reconstructed PDC track points
- classical target-momentum solving
- NN dataset generation, training, export, and C++ runtime inference
- corrected-target production runs and batch evaluation
- routing between the narrower PDC / NN skills

## Core Mental Model

Current NN reconstruction does not solve the target vertex directly.

The active NN task in this repo is:

1. reconstruct two 3D PDC points from Geant4 detector hits
2. use those 6 coordinates as features
3. regress proton momentum `(px, py, pz)` at the target
4. export the trained MLP into JSON for C++ forward-only inference

Treat requests accordingly. If the user says "target reconstruction", confirm whether they mean:

- target geometry / target position
- momentum at the target
- the full chain from geometry through momentum inference

## End-To-End Data Flow

1. Geant4 input ROOT (`g4input`) is simulated by `sim_deuteron`
2. output ROOT (`g4output`) stores `FragSimData` and `beam`
3. `PDCSimAna` reconstructs PDC track points from detector responses
4. `build_dataset.C` extracts feature rows:
   - features: `pdc1_x/y/z`, `pdc2_x/y/z`
   - labels: `px_truth`, `py_truth`, `pz_truth`
5. `train_mlp.py` trains a PyTorch MLP and writes:
   - `model.pt`
   - `model_meta.json`
   - `training_history.csv`
6. `export_model_for_cpp.py` converts the checkpoint into `model_cpp.json`
7. `reconstruct_sn_nn` loads the JSON model and performs C++ forward inference
8. `evaluate_reconstruct_sn_nn` computes reconstruction efficiency and momentum errors from `*_reco.root`

## Default Anchors For Current Corrected-Target NN Flow

Use these as the current project anchors unless the user points to a different campaign:

- corrected-target geometry macro:
  - `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
- production pipeline:
  - `scripts/analysis/run_sn124_nn_reco_pipeline.sh`
- corrected-target run note:
  - `docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg_notes.zh.md`

## Routing To Sub-Skills

## `smsim-pdc-track-reco`

Use when the issue is about:

- `FragSimData` parsing
- U/X/V layer usage
- smearing
- how `RecoEvent::tracks` gets filled
- wrong or missing PDC1 / PDC2 points

## `smsim-pdc-momentum-reco`

Use when the issue is about:

- RK back-propagation
- matrix fallback contracts
- target constraint and solver status
- `PDCMomentumReconstructor` behavior
- classical momentum reconstruction tests

## `smsim-nn-target-momentum`

Use when the issue is about:

- dataset generation from Geant4 output
- training split / hyper-parameters / metrics
- `model.pt`, `model_meta.json`, `model_cpp.json`
- C++ NN inference path in `reconstruct_sn_nn`
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

- Do not assume NN replaces the classical solver architecture everywhere; it is currently a separate forward-only backend.
- Do not assume target position can be recomputed ad hoc when the repo already has geometry and acceptance tooling defining it.
- Do not compare training metrics across campaigns without checking geometry consistency first.
- Do not mix corrected-target production evaluation with older zero-degree or placeholder-target setups.
