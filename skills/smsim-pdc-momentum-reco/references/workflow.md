# Workflow

## Scope

- `PDCMomentumReconstructor` solver chain (`RK -> MultiDim placeholder -> Matrix fallback`)
- `RecoConfig` and solver status contracts
- matrix fallback env/file handling
- Target-level momentum fit APIs

## Entry Points

- `libs/analysis_pdc_reco/include/PDCMomentumReconstructor.hh`
- `libs/analysis_pdc_reco/src/PDCMomentumReconstructor.cc`
- `libs/analysis_pdc_reco/include/PDCRecoTypes.hh`
- `libs/analysis_pdc_reco/src/PDCRecoFactory.cc`
- `libs/analysis/include/TargetReconstructor.hh`
- `tests/analysis/test_PDCMomentumReconstructor.cc`
- `tests/analysis/test_TargetReconstructor.cc`

## Focused Validation

```bash
cd build && ctest -R test_PDCMomentumReconstructor --output-on-failure
cd build && ctest -R test_TargetReconstructor --output-on-failure
```

Matrix fallback contract:

- `SAMURAI_MATRIX0TH_FILE`
- `SAMURAI_MATRIX1ST_FILE`

must both be set when validating matrix path.
