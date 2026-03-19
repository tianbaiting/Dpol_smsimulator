# Path Map

## Source Of Truth Paths

- `scripts/reconstruction/nn_target_momentum/build_dataset.C`
- `scripts/reconstruction/nn_target_momentum/generate_tree_input_disk_pz.C`
- `scripts/reconstruction/nn_target_momentum/train_mlp.py`
- `scripts/reconstruction/nn_target_momentum/infer_mlp.py`
- `scripts/reconstruction/nn_target_momentum/evaluate_mlp.py`
- `scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py`
- `scripts/reconstruction/nn_target_momentum/run_formal_training.sh`
- `scripts/reconstruction/nn_target_momentum/run_pipeline.sh`
- `scripts/reconstruction/nn_target_momentum/run_domain_matched_retrain.sh`
- `scripts/analysis/run_sn124_nn_reco_pipeline.sh`
- `libs/analysis_pdc_reco/include/PDCNNMomentumReconstructor.hh`
- `libs/analysis_pdc_reco/src/PDCNNMomentumReconstructor.cc`
- `libs/analysis_pdc_reco/include/PDCMomentumReconstructor.hh`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructor.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorNN.cc`
- `apps/tools/reconstruct_sn_nn.cc`
- `apps/tools/evaluate_reconstruct_sn_nn.cc`
- `tests/analysis/test_PDCMomentumReconstructor.cc`

## Update Rule

If any path above moves, update this map first, then update `scripts/check_sync.sh`.
