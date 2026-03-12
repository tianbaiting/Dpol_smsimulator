---
name: smsim-nn-target-momentum
description: Workflow for SMSimulator NN-based proton target-momentum reconstruction. Use when building datasets from Geant4 ROOT, training/exporting MLP models, wiring C++ forward inference (JSON model), running reconstruct_sn_nn pipelines, or evaluating NN reconstruction accuracy.
---

# SMSimulator NN Target Momentum

Use this skill for end-to-end NN target-momentum work only.

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
