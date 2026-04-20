# NN target-contamination audit — 2026-04-20

## Background

`DeutDetectorConstruction::fSetTarget` previously defaulted to `true`, silently
constructing an 80×80×30 mm Sn target into the simulation geometry even when
no `/samurai/geometry/Target/SetTarget` command appeared in the macro. For NN
training runs that fed tree-gun input (already post-target IMQMD/Faddeev
secondaries), this caused silent double scattering: particles passed through
the Sn target a second time inside Geant4, corrupting the (PDC hit) → (true
target momentum) mapping that the network learns.

The contaminating geometry macro is
`configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac`,
which contains no `SetTarget` command and therefore relied on the old default.
The fix — flipping the code default to `false`, making all bare macros
explicit, and adding a runtime warning — landed in branch
`fix/target-default-false`. A companion spec is at
`docs/superpowers/specs/2026-04-20-target-config-audit-design.md`.

## Classification rules

| Geometry path suffix                             | Verdict      |
|--------------------------------------------------|--------------|
| `*_target3deg.mac`                               | SAFE         |
| bare `geometry_B115T_pdcOptimized_20260227.mac`  | CONTAMINATED |
| any other                                        | UNKNOWN      |

## Findings

1 manifest audited — 0 SAFE, 1 CONTAMINATED, 0 UNKNOWN.

| Manifest                                                                          | Geometry basename                                    | Verdict      |
|-----------------------------------------------------------------------------------|------------------------------------------------------|--------------|
| `data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/manifest.json` | `geometry_B115T_pdcOptimized_20260227.mac`           | CONTAMINATED |

## Recommended actions

- **SAFE** runs: no action.
- **CONTAMINATED** runs: retrain using the fixed geometry
  (`geometry_B115T_pdcOptimized_20260227_target3deg.mac`). Do not consume
  their predictions for physics results until retrained.
- **UNKNOWN** runs: manually inspect the referenced `.mac` for a
  `Target/SetTarget` command and reclassify before use.

## Follow-up

Retraining itself is out of scope for this audit; it will be scheduled as a
separate task referencing this report.
