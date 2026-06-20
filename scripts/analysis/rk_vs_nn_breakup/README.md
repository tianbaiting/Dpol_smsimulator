# RK vs NN breakup reconstruction evaluation

Head-to-head evaluation of RK (Runge-Kutta least-squares) vs NN (MLP) proton
reconstruction on the d+Sn breakup sample, plus neutron truth-vs-reco residuals.

## Data sources (consumed as-is, no re-running)

- **RK reco** (spana03, accessed via SSHFS):
  `/home/tbt/workspace/Dpol_smsimulator/data/reconstruction/dbreak_elastic/`
  - 176 files: `y_pol/phi_random/<cell>/dbreak0000_reco.root` (16) +
    `z_pol/b_discrete/<cell>/dbreakb<NN>_reco.root` (160)
  - Produced 2026-05-09 via `run_dbreak_elastic_pipeline.py stage_reco`
    (`--backend rk --rk-fit-mode three-point-free --max-iterations 40`)

- **NN reco** (local):
  `data/reconstruction/breakup_nn_20260503/`
  - 176 files: `y_pol/<cell>_reco_nn.root` (16) +
    `z_pol/<cell>_b<NN>_reco_nn.root` (160)
  - Produced 2026-05-03 via `01_run_nn_reco.sh` with the clean production model
    `formal_B115T3deg/20260420_184345`

Both reco sets are derived from the **same g4output**, so per-event entry-index
alignment is valid 1:1.

## Quick start

```bash
# 1. Ensure SSHFS mount (handled automatically by run_compare.sh)
mnt spana03 Dpol_smsimulator

# 2. Smoke test on 4-cell subset
./scripts/analysis/rk_vs_nn_breakup/run_compare.sh smoke

# 3. Full run on all 176 cells
./scripts/analysis/rk_vs_nn_breakup/run_compare.sh full
```

## Output

- `build/test_output/rk_vs_nn_breakup/<tag>/merged_events.root` — single TTree
  `events` with one row per event per cell, plus `TObjString "Provenance"`
  recording data sources and git SHA.
- `build/test_output/rk_vs_nn_breakup/<tag>/figures/*.png` — 20 PNG plots.
- `build/test_output/rk_vs_nn_breakup/<tag>/summary.json` — aggregate metrics.
- `build/test_output/rk_vs_nn_breakup/<tag>/rk_vs_nn_breakup_<tag>[_zh].tex` —
  LaTeX report (English + Chinese).

## Plot list

- `proton_residuals_{ypol,zpol}.png` — RK vs NN overlaid residual histograms
- `proton_reco_vs_truth_{ypol,zpol}.png` — 2D scatter, RK left / NN right
- `proton_per_event_winner_{ypol,zpol}.png` — |RK-truth| vs |NN-truth| scatter
- `proton_rk_vs_nn_agreement_{ypol,zpol}.png` — RK vs NN direct scatter
- `proton_per_cell_mae_{ypol,zpol}.png` — per-cell MAE bar chart
- `proton_efficiency_per_cell_{ypol,zpol}.png` — per-cell efficiency
- `proton_rk_pull_{ypol,zpol}.png` — RK pull distribution (reco-truth)/sigma
- `neutron_residuals_{ypol,zpol}.png` — neutron residual histograms
- `neutron_reco_vs_truth_{ypol,zpol}.png` — neutron 2D scatter

## Scope

- Proton: RK-vs-truth / NN-vs-truth / RK-vs-NN (per-event aligned, aggregate
  MAE/RMSE/Bias/P95)
- Neutron: truth-vs-reco only (NEBULA β-method, identical in RK and NN files
  since both backends delegate to `NEBULAReco`)
- Both ypol (16 cells) + zpol (160 b-segments) = 352 reco files

## Frame convention

Truth (`truth_proton_p4`, `truth_neutron_p4`) is in the event-generator target
frame (beam-as-Z). Both RK and NN reco are rotated to target frame at write time
via `RotateRecoResultToTargetFrame` (`apps/run_reconstruction/main.cc:629-630`).
**All comparisons in this analysis happen in the target frame; no additional
rotation in the analysis code.**

## Fulfillment

This evaluation fulfills the explicit follow-up flagged in
`nn_breakup_phys_20260503_en.tex:27,374`:
> "RK reconstruction on the same data (after running on spana03), and a
> head-to-head NN vs RK comparison report."

## Conventions followed (per AGENTS.md)

- Analysis-only: no new reconstruction features, no changes to
  `libs/analysis_pdc_reco/`, `apps/run_reconstruction/`, or
  `apps/tools/evaluate_target_momentum_reco.cc`
- No CSV intermediates: ROOT files are the storage format
- No new geometry/target-position convention: reading only already-rotated reco
  and already-target-frame truth from `*_reco.root`
