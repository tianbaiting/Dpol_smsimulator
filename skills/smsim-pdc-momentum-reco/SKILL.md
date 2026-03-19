---
name: smsim-pdc-momentum-reco
description: Workflow for SMSimulator momentum reconstruction from PDC track points. Use when modifying the primary analysis_pdc_reco runtime framework, solver dispatch/backends (RK, matrix fallback, NN hook points, multidim placeholder), reconstruction configuration, or compatibility boundaries with TargetReconstructor.
---

# SMSimulator PDC Momentum Reconstruction

Use this skill for track-to-momentum solving stages in the primary runtime framework.

If the request spans corrected-target geometry, PDC stage routing, or NN pipeline selection, start with `skills/smsim-pdc-target-reco-overview/` first.

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
