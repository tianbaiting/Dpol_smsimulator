# Workflow

## Scope

- Build training datasets from Geant4 simulation ROOT with `PDCSimAna` track points (PDC1/PDC2).
- Train PyTorch MLP for target proton momentum `(px, py, pz)`.
- Export PyTorch checkpoint to C++ runtime JSON format.
- Run forward-only NN reconstruction in C++ (`reconstruct_sn_nn`) while keeping neutron logic unchanged.
- Evaluate reconstruction efficiency and momentum error metrics.

## Data Flow

1. `g4input/*.root` -> Geant4 (`sim_deuteron`) -> `g4output/*.root`
2. `g4output/*.root` + geometry macro -> `build_dataset.C` -> dataset CSV/ROOT
3. dataset CSV -> `train_mlp.py` -> `model.pt` + `model_meta.json`
4. `model.pt` + `model_meta.json` -> `export_model_for_cpp.py` -> `model_cpp.json`
5. `g4output/*.root` + geometry + `model_cpp.json` -> `reconstruct_sn_nn` -> `*_reco.root`
6. reconstructed ROOT dir -> `evaluate_reconstruct_sn_nn` -> accuracy/efficiency summary

## Entry Points

- Training data build:
  - `scripts/reconstruction/nn_target_momentum/build_dataset.C`
  - `scripts/reconstruction/nn_target_momentum/generate_tree_input_disk_pz.C`
- Training/inference/export:
  - `scripts/reconstruction/nn_target_momentum/train_mlp.py`
  - `scripts/reconstruction/nn_target_momentum/infer_mlp.py`
  - `scripts/reconstruction/nn_target_momentum/evaluate_mlp.py`
  - `scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py`
  - `scripts/reconstruction/nn_target_momentum/run_formal_training.sh`
  - `scripts/reconstruction/nn_target_momentum/run_domain_matched_retrain.sh`
- C++ runtime NN inference:
  - `libs/analysis_pdc_reco/include/PDCNNMomentumReconstructor.hh`
  - `libs/analysis_pdc_reco/src/PDCNNMomentumReconstructor.cc`
  - `libs/analysis_pdc_reco/src/PDCMomentumReconstructor.cc`
  - `apps/tools/reconstruct_sn_nn.cc`
  - `apps/tools/evaluate_reconstruct_sn_nn.cc`
- End-to-end production pipeline:
  - `scripts/analysis/run_sn124_nn_reco_pipeline.sh`

## Focused Validation

```bash
cd build && make -j$(nproc) reconstruct_sn_nn evaluate_reconstruct_sn_nn
python3 scripts/reconstruction/nn_target_momentum/train_mlp.py --help
python3 scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py --help
scripts/analysis/run_sn124_nn_reco_pipeline.sh --dry-run
```

If `anaroot-env` exists, prefer:

```bash
micromamba run -n anaroot-env python scripts/reconstruction/nn_target_momentum/train_mlp.py --help
```

## Contract Checks

- `train_mlp.py` input features must stay exactly 6D (`pdc1_xyz`, `pdc2_xyz`).
- Exported JSON must remain `format: smsimulator_pdc_mlp_v1` with `x_mean/x_std/layers`.
- `PDCNNMomentumReconstructor::Forward` must apply hidden-layer ReLU exactly like training.
- Runtime model path can come from `RecoConfig::nn_model_json_path` or `PDC_NN_MODEL_JSON`.
