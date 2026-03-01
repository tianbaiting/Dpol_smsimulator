---
name: smsim-pdc-track-reco
description: Workflow for SMSimulator PDC hit-to-track reconstruction. Use when modifying PDCSimAna hit parsing, U/V layer reconstruction, smearing logic, RecoEvent track filling, or tests that validate PDC track points before momentum reconstruction.
---

# SMSimulator PDC Track Reconstruction

Use this skill for hit decoding and track-point reconstruction stages.

## Workflow

1. Start with `references/workflow.md`.
2. Confirm path ownership using `references/path-map.md`.
3. Apply code changes in `libs/analysis` track-reco files.
4. Run focused tests listed in `references/workflow.md`.
5. Run `scripts/check_sync.sh`.

## When Structure Changes

1. Update `references/path-map.md` with new source paths.
2. Update `scripts/check_sync.sh` path checks and module patterns.
3. Re-run sync script and fix missing anchors.

## Resources

- `references/workflow.md`
- `references/path-map.md`
- `scripts/check_sync.sh`
