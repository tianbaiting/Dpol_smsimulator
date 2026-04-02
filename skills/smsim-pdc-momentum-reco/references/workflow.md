# Workflow

## Scope

- the primary `analysis_pdc_reco` runtime framework for `PDC1/PDC2 -> target momentum`
- `PDCMomentumReconstructor` solver chain (`NN -> RK -> MultiDim placeholder`)
- `RecoConfig` and solver status contracts
- backend boundaries and dispatcher behavior
- RK mode selection via `--rk-fit-mode`
- legacy compatibility boundaries with `TargetReconstructor`

## Entry Points

- `libs/analysis_pdc_reco/include/PDCMomentumReconstructor.hh`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructor.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorRK.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorMultiDim.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorNN.cc`
- `libs/analysis_pdc_reco/include/PDCRecoTypes.hh`
- `libs/analysis_pdc_reco/include/PDCRecoRuntime.hh`
- `libs/analysis_pdc_reco/src/PDCRecoRuntime.cc`
- `libs/analysis_pdc_reco/src/PDCRecoFactory.cc`
- `apps/run_reconstruction/main.cc`
- `apps/run_reconstruction/CMakeLists.txt`
- `apps/tools/CMakeLists.txt`
- `libs/analysis/include/TargetReconstructor.hh`
- `scripts/analysis/legacy_target_reco/`
- `tests/analysis/test_PDCMomentumReconstructor.cc`
- `tests/analysis/test_TargetReconstructor.cc`

## Architecture Rules

- `analysis_pdc_reco` is the preferred home for new runtime momentum-reconstruction work.
- RK remains a single backend, with public mode selection through `two-point-backprop`, `fixed-target-pdc-only`, and `three-point-free`.
- `TargetReconstructor` is compatibility-only unless the task is explicitly about migration or regression coverage.
- legacy step scans and synthetic TargetReconstructor studies belong under `scripts/analysis/legacy_target_reco/`.
- The NN backend is part of the main runtime framework even if its model lifecycle lives in separate scripts.

## Focused Validation

```bash
cd build && make -j$(nproc) reconstruct_target_momentum reconstruct_sn_nn
cd build && ctest -R PDCMomentumReconstructorTest --output-on-failure
cd build && ctest -R 'PdcTargetMomentumE2E\\.RK\\.(TwoPointBackprop|FixedTargetPdcOnly|ThreePointFree)' --output-on-failure
cd build && ctest -R test_TargetReconstructor --output-on-failure
```
