# ypol-new 20260413 — Geant4 full production sim notes

## Goal

Run the 20260413 IMQMD ypol dataset (d+Sn112 + d+Sn124, E=190 MeV/u) through
the Geant4 simulation in two material configurations:

1. **`all_air`** — world air, dipole cavity air, downstream pipe air, exit-window
   hole air. Matches the experimental reality at the SAMURAI beam line.
2. **`mixed`** — world still air, but the connected beam-line volume
   (upstream pipe + dipole cavity + downstream pipe + exit-window hole)
   switched to `G4_Galactic`. Hypothetical upgrade configuration.

Required a small physics-consistency fix to `ExitWindowC2Construction`
before launching (§ EW fix below).

## Inputs

- Raw QMD data: `data/qmdrawdata/qmdrawdata/20260413ypol/d+Sn{112,124}E190/d+Sn{112,124}E190g{050,...,085}{ypn,ynp}-RP360/`
- Converted g4input trees: `data/simulation/g4input/20260413ypol/d+Sn{112,124}E190/.../geminiout.root`
  - 32 ROOT files total (16 per Sn isotope), 1,000,000 events each
  - Conversion binary: `bin/GenInputRoot_qmdrawdata`
  - Raw-data layout didn't match the converter's `y_pol/phi_random` /
    `allevent` expectations, so conversion ran through a symlink shim
    under `data/qmdrawdata/qmdrawdata/20260413ypol_shim/allevent/`.

## Geometry macros

- **`all_air`**: `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
  - `/samurai/geometry/FillAir true`
  - `BeamLineVacuum` left at default (false)
- **`mixed`**: `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac`
  - `/samurai/geometry/FillAir true`
  - `/samurai/geometry/BeamLineVacuum true`

Target position & angle (both macros): pos `(-1.2488, 0.0009, -106.9458) cm`, angle `3.0°`.

## EW-hole material coupling fix

Before the run, `ExitWindowC2Hole_log` was hardcoded to `G4_Galactic`
(`libs/smg4lib/src/devices/ExitWindowC2Construction.cc:123`). The flange
hole is physically connected to the dipole downstream pipe + magnet cavity,
which already track the `BeamLineVacuum` switch. In `all_air` mode the
hardcoded vacuum created a 30 mm vacuum slab in an otherwise air beam-line —
not physical.

Fix: threaded `fBeamLineVacuum` through `ExitWindowC2Construction` (same
pattern as `DipoleConstruction` / `VacuumDownstreamConstruction`), so the
hole material follows `BeamLineVacuum`: `G4_Galactic` when true, parent hall
material (air in the present macros) when false.

Commit: `c4a0a7f fix(geometry): gate ExitWindowC2 hole material on BeamLineVacuum`

Files:
- `libs/smg4lib/src/devices/ExitWindowC2Construction.hh` — add `fBeamLineVacuum` + setter/getter.
- `libs/smg4lib/src/devices/ExitWindowC2Construction.cc` — gate `holeMat` on the flag.
- `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc` — wire the setter before `ConstructSub()`.

## Runner

`scripts/simulation/run_g4input_batch_parallel.sh` — xargs-based parallel
driver added for this run. One macro per input ROOT, `xargs -P 16` for
16-way concurrency. See script header for env-var interface.

## Outputs

- **all_air**: `data/simulation/g4output/ypol_new_20260413_allair/d+Sn{112,124}E190/.../geminiout0000.root`
- **mixed**:   `data/simulation/g4output/ypol_new_20260413_mixed/d+Sn{112,124}E190/.../geminiout0000.root`

| config  | files | evts/file | total evts | FAIL | wall t (avg) | wall t (max) | disk  |
| ------- | ----- | --------- | ---------- | ---- | ------------ | ------------ | ----- |
| all_air | 32    | 1,000,000 | 32 M       | 0    | 7.73 h       | 9.46 h       | 29 GB |
| mixed   | 32    | 1,000,000 | 32 M       | 0    | 7.17 h       | 8.53 h       | 26 GB |

Throughput under 16-way parallel: ~37 events/s per process × 16 ≈ 590 evt/s
aggregate. Mixed is ~7% faster than all_air on average — consistent with
fewer secondary generations in the vacuumed beam line.

Tree name in output ROOT: `tree` (entries = 1,000,000 verified on
representative files for both configs).

## Flight distances (Target → PDCs)

From the same geometry macros used for the runs (target at
`(-1.2488, 0.0009, -106.9458) cm`, 3° tilt, PDCs at their pdc-optimized
20260227 positions):

| segment                | distance |
| ---------------------- | -------- |
| target → exit-window   | ≈ 3.11 m |
| exit-window → PDC1     | ≈ 0.81 m |
| PDC1 → PDC2            | ≈ 1.00 m |

Sub-segments within the first 3.11 m: target → dipole yoke ≈ 0.68 m,
yoke cavity arc ≈ 0.95 m, downstream pipe ≈ 1.45 m, EW hole ≈ 0.03 m.

## Not done here

- No reconstruction run over the new output yet.
- No `all_vacuum` configuration — user asked for two; all-vacuum is only a
  reference in the MS-ablation study, not an experimentally accessible state.
- No rerun of the MS-ablation 3-condition ensemble; the EW-hole fix changes
  stage A' by at most a 30 mm slab swap, which doesn't move any qualitative
  conclusion there.
