---
name: smsim-geant4-io
description: Workflow for SMSimulator Geant4 simulation data input/output. Use when modifying or debugging macro commands, tree-based beam input, ROOT event output, g4input generation, or branch-level data compatibility across apps/sim_deuteron, libs/smg4lib, scripts/event_generator, scripts/simulation, and libs/analysis EventDataReader.
---

# SMSimulator Geant4 IO

Use this skill for simulation input/output pipeline issues only.

## Workflow

1. Read `references/workflow.md` to locate the current IO entry point.
2. Edit only the owning files listed in `references/path-map.md`.
3. Run focused command(s) in `references/workflow.md`.
4. Run `scripts/check_sync.sh` before finishing.

## When Structure Changes

If files are moved/renamed and this skill becomes stale:

1. Update file paths in `references/path-map.md`.
2. Update path checks and change patterns in `scripts/check_sync.sh`.
3. Re-run `scripts/check_sync.sh` until no missing anchors are reported.

## Resources

- `references/workflow.md`
- `references/path-map.md`
- `scripts/check_sync.sh`
