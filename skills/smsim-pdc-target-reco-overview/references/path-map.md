# Path Map

## Source Of Truth Paths

### Geometry And Target Anchors

- `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
- `docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg_notes.zh.md`
- `docs/note_log/task.md`

### PDC Track Reconstruction

- `libs/analysis/include/PDCSimAna.hh`
- `libs/analysis/src/PDCSimAna.cc`
- `skills/smsim-pdc-track-reco/`

### Primary Runtime Reconstruction Framework

- `libs/analysis_pdc_reco/include/PDCMomentumReconstructor.hh`
- `libs/analysis_pdc_reco/include/PDCRecoTypes.hh`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructor.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorRK.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorMultiDim.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorNN.cc`
- `skills/smsim-pdc-momentum-reco/`

### NN Backend Lifecycle

- `scripts/reconstruction/nn_target_momentum/`
- `apps/run_reconstruction/main.cc`
- `apps/run_reconstruction/CMakeLists.txt`
- `apps/tools/CMakeLists.txt`
- `apps/tools/evaluate_target_momentum_reco.cc`
- `libs/analysis_pdc_reco/include/PDCRecoRuntime.hh`
- `libs/analysis_pdc_reco/src/PDCRecoRuntime.cc`
- `libs/analysis_pdc_reco/include/PDCNNMomentumReconstructor.hh`
- `libs/analysis_pdc_reco/src/PDCNNMomentumReconstructor.cc`
- `skills/smsim-nn-target-momentum/`

### Legacy Compatibility

- `libs/analysis/include/TargetReconstructor.hh`
- `tests/analysis/test_TargetReconstructor.cc`
- `scripts/analysis/legacy_target_reco/`

### Production Pipeline

- `scripts/analysis/run_target_momentum_reco_pipeline.sh`
- `scripts/analysis/run_sn124_nn_reco_pipeline.sh`

## Update Rule

If any anchor path above moves or stops being the current source of truth, update this file and `scripts/check_sync.sh` together.
