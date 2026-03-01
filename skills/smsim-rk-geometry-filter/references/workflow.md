# Workflow

## Scope

- RK4 trajectory propagation in magnetic field
- detector acceptance check for PDC/NEBULA
- target-frame -> lab-frame momentum transform before hit checks
- event classification and ratio evolution in QMD geometry filtering

## Entry Points

- `libs/analysis/include/ParticleTrajectory.hh`
- `libs/analysis/src/ParticleTrajectory.cc`
- `libs/geo_accepentce/include/DetectorAcceptanceCalculator.hh`
- `libs/geo_accepentce/src/DetectorAcceptanceCalculator.cc`
- `libs/qmd_geo_filter/include/QMDGeoFilter.hh`
- `libs/qmd_geo_filter/src/QMDGeoFilter.cc`
- `libs/qmd_geo_filter/src/RunQMDGeoFilter.cc`

## Focused Validation

```bash
cd build && ctest -R test_ParticleTrajectory --output-on-failure
./build/bin/RunQMDGeoFilter --field 1.2 --angle 5 --target Pb208 --pol zpol --fixed-pdc --pdc-macro configs/simulation/geometry/5deg_1.2T.mac --px-range 100
```
