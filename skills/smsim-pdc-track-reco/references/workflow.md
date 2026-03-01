# Workflow

## Scope

- Parse PDC hits from `FragSimData`
- Build U/V hit groups for PDC1/PDC2
- Apply smearing
- Reconstruct two PDC global points
- Fill `RecoEvent::tracks`

## Entry Points

- `libs/analysis/include/PDCSimAna.hh`
- `libs/analysis/src/PDCSimAna.cc`
- `libs/analysis/include/RecoEvent.hh`
- `libs/analysis/include/GeometryManager.hh`
- `tests/analysis/test_TargetReconstructor_RealData.cc`

## Focused Validation

```bash
cd build && ctest -R test_TargetReconstructor_RealData --output-on-failure
```

Optional diagnostics:

```bash
cd build && SM_TEST_VISUALIZATION=ON ctest -R test_TargetReconstructor_RealData --output-on-failure
```
