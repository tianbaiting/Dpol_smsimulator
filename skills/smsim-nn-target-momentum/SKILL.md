---
name: smsim-nn-target-momentum
description: Workflow for the NN backend lifecycle of SMSimulator target-momentum reconstruction. Use when building datasets from Geant4 ROOT, training/exporting MLP models, wiring C++ forward inference for the analysis_pdc_reco NN backend, running reconstruct_target_momentum in NN mode or its reconstruct_sn_nn compatibility wrapper, or evaluating reconstruction accuracy with the canonical evaluator and its compatibility wrapper.
---

# SMSimulator NN Target Momentum

Use this skill for the end-to-end lifecycle of the NN backend only.

If the task also depends on target-geometry correction, cross-stage routing, or choosing among PDC sub-skills, start with `skills/smsim-pdc-target-reco-overview/` first.

## Workflow

1. Read `references/workflow.md` first for the authoritative data flow.
2. Edit only owner files listed in `references/path-map.md`.
3. Keep training and C++ inference model format aligned (`model_meta.json` + `model_cpp.json`).
4. Validate with focused commands in `references/workflow.md`.
5. Run `scripts/check_sync.sh` before finishing.

## When Structure Changes

1. Update moved/renamed paths in `references/path-map.md`.
2. Mirror those path updates in `scripts/check_sync.sh`.
3. Re-run sync script until it reports `[OK]`.

## Resources

- `references/workflow.md`
- `references/path-map.md`
- `scripts/check_sync.sh`
