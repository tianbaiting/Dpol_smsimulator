---
name: smsim-ips-veto-position-scan
description: Workflow for SMSimulator IPS veto geometry placement and Geant4 position scanning. Use when modifying IPS geometry, target-reference-only placement, QMD dbreak/geminiout conversion, no-forward small-b scoring, scan scripts under scripts/analysis, WRL example export, or interpreting elastic leakage versus small-b IPS hit rate for the 3deg_1.15T Sn124 study.
---

# SMSimulator IPS Veto Position Scan

Use this skill for IPS-only geometry, scan, and visualization tasks.

## Workflow

1. Read `references/workflow.md` before changing scan logic, event selection, or WRL export.
2. Edit only the owning files listed in `references/path-map.md`.
3. Keep IPS studies on the `3deg_1.15T` geometry unless the task explicitly changes the field/angle setup.
4. Keep `Target/SetTarget false` for IPS scan and WRL export macros. In this workflow the target is a position and direction reference only, not material.
5. Treat the current official IPS optimization as `no-forward`:
   - signal: all-event entries with `bimp <= 7`
   - background: cleaned `dbreak`
   - score: IPS hit rate subject to elastic leakage constraint
6. Run the relevant scripts from `references/workflow.md` and update the mechanic doc if the scan meaning or result changes.
7. Run `scripts/check_sync.sh` before finishing.

## Resources

- `references/workflow.md`
- `references/path-map.md`
- `scripts/check_sync.sh`
