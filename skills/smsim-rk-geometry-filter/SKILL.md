---
name: smsim-rk-geometry-filter
description: Workflow for SMSimulator Runge-Kutta trajectory propagation and geometry acceptance filtering. Use when modifying ParticleTrajectory integration, DetectorAcceptanceCalculator hit logic, QMDGeoFilter classification, fixed/optimized PDC geometry setup, or geometry acceptance analysis commands.
---

# SMSimulator RK Geometry Filter

Use this skill for acceptance/filter pipelines based on RK trajectory propagation.

## Workflow

1. Read `references/workflow.md` for algorithm/dataflow.
2. Confirm owner paths in `references/path-map.md`.
3. Change only required files.
4. Run focused trajectory/filter validations.
5. Run `scripts/check_sync.sh`.

## When Structure Changes

1. Update `references/path-map.md` with current source layout.
2. Update patterns and required paths in `scripts/check_sync.sh`.
3. Re-run sync check until no missing anchors.

## Resources

- `references/workflow.md`
- `references/path-map.md`
- `scripts/check_sync.sh`
