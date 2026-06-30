# Sn112/Sn124 Consistency Audit for SAMURAI Workshop 2026

Date: 2026-06-30

Purpose: decide whether `112Sn` and `124Sn` can be shown as a direct
quantitative isotope comparison in the main SAMURAI Workshop 2026 talk.

## Current Same-Chain Evidence

| item | Sn112 | Sn124 | status |
| --- | --- | --- | --- |
| source production | NEBULA-Plus joint production is described as including Sn112 + Sn124 in the gamma-constraint report | same | described, but local exported tables are Sn124-only |
| geometry | SAMURAI + NEBULA + NEBULA Plus intended | same | common in production description |
| reconstruction backend | PDC NN proton + NEBULA/NEBULA Plus neutron intended | same | common in production description |
| polarization | y-pol intended | y-pol | common in production description |
| gamma grid | 0.5, 0.6, 0.7, 0.8 intended | 0.5, 0.6, 0.7, 0.8 | common in production description |
| event-quality selection | not exported locally for Sn112 tight px60 folded table | truth-assisted tight cut exported locally for Sn124 | not verified for direct comparison |
| px fiducial | not exported locally for Sn112 reco px60 table | reco `|p_x,n^reco| < 60 MeV/c` | not verified for direct comparison |
| reaction-plane definition | not exported locally for Sn112 reco-plane table | reconstructed event plane | not verified for direct comparison |
| normalization/statistics | not exported locally | current MC statistics in Sn124 CSV | not verified for direct comparison |

## Local Files Checked

- `docs/reports/gamma_constraint_20260611/figures/sn124_tight_px60_r_table.csv`
- `docs/reports/gamma_constraint_20260611/figures/sn124_tight_px60_gamma_separation.csv`
- `scripts/reconstruction/nn_target_momentum/make_gamma_constraint_report_assets.py`
- `scripts/reconstruction/nn_target_momentum/make_cut_diagnostics_figures.py`
- `docs/reports/gamma_constraint_20260611/gamma_constraint_report_zh.md`
- `docs/reports/gamma_constraint_20260611/gamma_constraint_report_en.tex`
- local older ROOT files under `data/reconstruction/breakup_nn_20260503/`

## Decision for the Main Talk

The main talk should not present a direct quantitative Sn112/Sn124 folded
comparison, because the locally available exported tight-px60 full-reco table is
Sn124-only.  The older local `breakup_nn_20260503` Sn112 files are not the same
NEBULA Plus joint production and should not be plotted directly against the
current Sn124 NEBULA Plus closure result.

Use Sn112 in the main text as the isotope-reference target and state that the
same-chain Sn112 folded closure remains a required next step.

## Required Next Step

Generate a common-chain isotope table from the same NEBULA Plus production:

```text
target = Sn112, Sn124
pol = y-pol
gamma = 0.5, 0.6, 0.7, 0.8
stage = truth-side fiducial reference, reco-plane folded result
cut = truth-assisted tight event-quality selection
fiducial = |p_x,n^reco| < 60 MeV/c
event plane = reconstructed p+n transverse momentum
```

Only after that can the talk show a direct quantitative isotope comparison in
the main result section.
