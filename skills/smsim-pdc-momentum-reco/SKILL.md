---
name: smsim-pdc-momentum-reco
description: Workflow for SMSimulator momentum reconstruction from PDC track points. Use when modifying PDCMomentumReconstructor solver logic (RK, matrix fallback), TargetReconstructor interfaces, reconstruction configuration, or momentum-related tests.
---

# SMSimulator PDC Momentum Reconstruction

Use this skill for track-to-momentum solving stages.

## Workflow

1. Open `references/workflow.md` first.
2. Edit only module-owner files from `references/path-map.md`.
3. Validate with momentum-focused tests.
4. Run `scripts/check_sync.sh`.

## When Structure Changes

1. Update moved/renamed paths in `references/path-map.md`.
2. Mirror those path updates inside `scripts/check_sync.sh`.
3. Re-run sync script and validation.

## Resources

- `references/workflow.md`
- `references/path-map.md`
- `scripts/check_sync.sh`
