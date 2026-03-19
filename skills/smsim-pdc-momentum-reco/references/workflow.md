# Workflow

## Scope

- the primary `analysis_pdc_reco` runtime framework for `PDC1/PDC2 -> target momentum`
- `PDCMomentumReconstructor` solver chain (`NN -> RK -> MultiDim placeholder -> Matrix fallback`)
- `RecoConfig` and solver status contracts
- backend boundaries and dispatcher behavior
- matrix fallback env/file handling
- legacy compatibility boundaries with `TargetReconstructor`

## Entry Points

- `libs/analysis_pdc_reco/include/PDCMomentumReconstructor.hh`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructor.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorRK.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorMatrix.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorMultiDim.cc`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructorNN.cc`
- `libs/analysis_pdc_reco/include/PDCRecoTypes.hh`
- `libs/analysis_pdc_reco/src/PDCRecoFactory.cc`
- `libs/analysis/include/TargetReconstructor.hh`
- `tests/analysis/test_PDCMomentumReconstructor.cc`
- `tests/analysis/test_TargetReconstructor.cc`

## Architecture Rules

- `analysis_pdc_reco` is the preferred home for new runtime momentum-reconstruction work.
- `TargetReconstructor` is compatibility-only unless the task is explicitly about migration or regression coverage.
- The NN backend is part of the main runtime framework even if its model lifecycle lives in separate scripts.

## Focused Validation

```bash
cd build && ctest -R test_PDCMomentumReconstructor --output-on-failure
cd build && ctest -R test_TargetReconstructor --output-on-failure
```

Matrix fallback contract:

- `SAMURAI_MATRIX0TH_FILE`
- `SAMURAI_MATRIX1ST_FILE`

must both be set when validating matrix path.
