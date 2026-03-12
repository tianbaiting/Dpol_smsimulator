---
name: smsim-pdc-target-reco-overview
description: Overview and routing skill for SMSimulator PDC target reconstruction. Use when you need the full picture across target geometry, PDC track reconstruction, target-momentum solving, NN dataset/training/export/C++ inference, corrected-target production pipelines, or when you must decide which PDC/NN sub-skill to use first.
---

# SMSimulator PDC Target Reconstruction Overview

Use this skill first when the request crosses multiple PDC reconstruction stages or when the correct geometry and target-position assumptions matter.

This is a routing and principles skill. It should reduce wrong assumptions before deeper edits.

## Workflow

1. Read `references/workflow.md` first for the authoritative end-to-end map.
2. Read `references/principles.md` before proposing geometry, target-position, or NN changes.
3. Use `references/path-map.md` to find the current source-of-truth files.
4. Route to the owning sub-skill once the task type is clear:
   - `skills/smsim-pdc-track-reco/` for `FragSimData -> PDC1/PDC2 track points`
   - `skills/smsim-pdc-momentum-reco/` for classical target-momentum solvers (RK / matrix fallback / solver contracts)
   - `skills/smsim-nn-target-momentum/` for dataset build, PyTorch training, export, C++ forward inference, and NN evaluation
5. Run `scripts/check_sync.sh` before finishing when this overview skill changes.

## Routing Guide

Use this overview skill to answer these questions before editing code:

- Is the problem caused by wrong target geometry or target-position assumptions?
- Is the task about PDC hit decoding, track-point reconstruction, classical solver logic, or NN forward inference?
- Is the reported regression likely due to geometry mismatch rather than model quality?
- Does the user want a project map, not a local implementation detail?

## When Structure Changes

1. Update `references/path-map.md` when source-of-truth paths move.
2. Update `references/workflow.md` if the production pipeline or formal training flow changes.
3. Update `references/principles.md` if project-level reconstruction rules change.
4. Update `scripts/check_sync.sh` and `agents/openai.yaml` if the overview stops matching the repo layout.

## Resources

- `references/workflow.md`
- `references/principles.md`
- `references/path-map.md`
- `scripts/check_sync.sh`
