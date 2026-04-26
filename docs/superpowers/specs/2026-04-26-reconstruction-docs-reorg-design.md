# Reorganize `docs/reports/reconstruction/` by Algorithm/Topic

**Date:** 2026-04-26
**Status:** Design approved, awaiting implementation plan
**Scope:** `docs/reports/reconstruction/` only (no source code changes)

## Problem

`docs/reports/reconstruction/` has accumulated documents for many distinct topics (PDC tracking, FDC tracking, RK solver, NN backend, synthetic-data tests, MS ablation, methods overviews, external references) all sitting flat at the top level. ~50+ LaTeX build artifacts (`.aux`, `.fdb_latexmk`, `.fls`, `.log`, `.nav`, `.out`, `.snm`, `.toc`, `.vrb`, `.xdv`) clutter the directory; 3 of them (`.xdv`) are even tracked in git. There's also no entry-point README, so a reader has to grep filenames to figure out what belongs together.

## Goals

1. Group documents by algorithm/topic so related material lives together.
2. Mirror the two-stage structure of the reconstruction pipeline (track reco → momentum reco) at the folder level — same shape as `libs/analysis_pdc_reco/` vs. the upstream tracking libs.
3. Eliminate LaTeX build-artifact noise from both the working tree and git.
4. Preserve git history (use `git mv`, not delete+add).
5. Don't break any existing figure references.

## Non-Goals

- No changes to `.tex`/`.md` document content (other than path-only adjustments if a `.md` link breaks).
- No changes to `figures/`, `data/`, `specs/`, or `plans/` inside `ms_ablation/` — that subtree only gets re-parented.
- No changes to other `docs/reports/*` siblings (`simulation/`, `imqmd_analysis/`, etc.) — out of scope.
- No code or config changes outside `docs/reports/reconstruction/`.

## Design

### Target Folder Layout

```
docs/reports/reconstruction/
├── README.md                     # NEW: entry point, pipeline diagram, navigation
│
├── methods/                      # cross-topic algorithm overviews
│   ├── anaroot_reconstruction_methods.zh.md
│   └── 靶点动量重建算法研究.md
│
├── references/                   # external papers
│   └── 2019孙亚洲.pdf
│
├── track_reco/                   # Stage 1: detector hits → tracks
│   ├── pdc/
│   │   ├── pdc_reconstruction_uncertainty_detailed.tex
│   │   └── pdc_reconstruction_uncertainty_detailed.pdf
│   └── fdc/
│       ├── fdc_reconstruction_detailed.tex
│       └── fdc_reconstruction_detailed.pdf
│
└── momentum_reco/                # Stage 2: tracks → target momentum
    ├── rk/
    │   ├── rk_reconstruction_status_20260416.tex
    │   ├── rk_reconstruction_status_20260416.pdf
    │   ├── rk_reconstruction_status_20260416_zh.tex
    │   ├── rk_reconstruction_status_20260416_zh.pdf
    │   ├── rk_reconstruction_bugfix_and_error_analysis_20260416.md
    │   ├── figures/
    │   │   ├── g4_fluctuation/
    │   │   │   ├── pdc_hits/
    │   │   │   └── reco_momentum/
    │   │   └── geometry/
    │   │       └── samurai_geometry_xz.png
    │   └── archive/
    │       └── g4_fluctuation_per_truth.png
    │
    ├── nn/
    │   ├── nn_retrain_qmdwindow_B115T3deg.tex
    │   ├── nn_retrain_qmdwindow_B115T3deg.pdf
    │   └── nn_retrain_qmdwindow_B115T3deg_notes.zh.md
    │
    ├── synthetic/
    │   ├── synthetic_reco_B115T3deg.tex
    │   └── synthetic_reco_B115T3deg.pdf
    │
    └── ms_ablation/              # whole subtree moved verbatim
        ├── README.md
        ├── ms_ablation_air_20260421_zh.md
        ├── ms_ablation_mixed_20260422_zh.{md,tex,pdf}
        ├── specs/
        ├── plans/
        ├── figures/
        └── data/
```

Per-topic structure follows the lightweight rule: only create `figures/`, `specs/`, `plans/`, `README.md` when there is content for them. `ms_ablation/` keeps its rich structure unchanged.

### Topic Mapping Rationale

Mapping was derived from `\includegraphics` grep results across all `.tex` files:

| File | Target | Why |
|------|--------|-----|
| `pdc_reconstruction_uncertainty_detailed.{tex,pdf}` | `track_reco/pdc/` | PDC hit-level uncertainty study |
| `fdc_reconstruction_detailed.{tex,pdf}` | `track_reco/fdc/` | FDC track reconstruction |
| `rk_reconstruction_status_20260416.{tex,pdf}` (EN + zh) | `momentum_reco/rk/` | RK solver status |
| `rk_reconstruction_bugfix_and_error_analysis_20260416.md` | `momentum_reco/rk/` | RK solver debug log |
| `nn_retrain_qmdwindow_B115T3deg.{tex,pdf,md}` | `momentum_reco/nn/` | NN backend retraining |
| `synthetic_reco_B115T3deg.{tex,pdf}` | `momentum_reco/synthetic/` | Synthetic-data study |
| `ms_ablation/` (whole subtree) | `momentum_reco/ms_ablation/` | RK error decomposition |
| `anaroot_reconstruction_methods.zh.md` | `methods/` | Cross-topic algorithm overview |
| `靶点动量重建算法研究.md` | `methods/` | Cross-topic algorithm overview |
| `2019孙亚洲.pdf` | `references/` | External thesis |
| `archive/g4_fluctuation_per_truth.png` | `momentum_reco/rk/archive/` | Stray PNG, RK-related per filename |
| `figures/g4_fluctuation/` | `momentum_reco/rk/figures/` | Only referenced by RK `.tex` |
| `figures/geometry/samurai_geometry_xz.png` | `momentum_reco/rk/figures/geometry/` | Only referenced by RK `.tex` |

### Link Integrity

Grep over all `.tex` files for `\includegraphics`:

- `rk_reconstruction_status_20260416{,_zh}.tex` reference `figures/g4_fluctuation/...` and `figures/geometry/samurai_geometry_xz.png` (relative to the `.tex` file). Since both `.tex` and the `figures/` subtree move together into `momentum_reco/rk/`, **relative paths are unchanged** — no edits needed.
- `synthetic_reco_B115T3deg.tex` references `../../../results/synthetic_reco/B115T_3deg/plots/...` (an out-of-tree `results/` directory). Moving the `.tex` from depth 3 (`docs/reports/reconstruction/`) to depth 5 (`docs/reports/reconstruction/momentum_reco/synthetic/`) means `../../../` becomes wrong by 2 levels. **Path must be fixed to `../../../../../results/synthetic_reco/...`**.
- `pdc_reconstruction_uncertainty_detailed.tex` uses `\LargeRunRoot/plots/reco_p_distribution.png` via a custom macro. The macro definition must be checked: if it's an absolute path, no change; if it's relative, it must be updated.
- `fdc_reconstruction_detailed.tex` and `nn_retrain_qmdwindow_B115T3deg.tex` have no `\includegraphics` calls — safe.
- `.md` files: implementation phase will grep for relative links and image references; fix any that break due to depth change.

### LaTeX Build-Artifact Cleanup

The repo root `.gitignore` (lines 56–71) already covers all relevant LaTeX patterns: `*.aux`, `*.log`, `*.nav`, `*.snm`, `*.toc`, `*.vrb`, `*.out`, `*.fls`, `*.fdb_latexmk`, `*.xdv` (plus `*.synctex.gz`, `*.bbl`, `*.blg`, `*.lof`, `*.lot`). The 3 tracked `.xdv` files are tracked because they predate the gitignore rule (gitignore doesn't apply retroactively to tracked files).

So the cleanup is:

- 3 tracked `.xdv` files: `git rm` them (regenerable from `.tex` via `latexmk`).
- All untracked `.aux`/`.fdb_latexmk`/`.fls`/`.log`/`.nav`/`.out`/`.snm`/`.toc`/`.vrb`/`.xdv` and stray `missfont.log`: `rm` from working tree.
- No new `.gitignore` file needed. Optionally append `missfont.log` to the root `.gitignore` LaTeX block (one line) since it's not currently covered.

### Top-Level README

New `docs/reports/reconstruction/README.md` — short, navigational:

- One-paragraph summary of what this directory contains.
- Mermaid-or-ASCII diagram of the two-stage pipeline (track_reco → momentum_reco).
- Bullet list of subdirectories with one-line purpose statements.
- Pointer to `methods/` for newcomers and to `ms_ablation/README.md` as the deepest current case study.

## Implementation Stages

Three commits, each independently revertable:

1. **Stage 1 — Clean LaTeX artifacts.** `git rm` the 3 tracked `.xdv` files; `rm` all untracked sidecar files in `docs/reports/reconstruction/`. (Optionally append `missfont.log` to root `.gitignore`.) No file moves.
2. **Stage 2 — Move files into new structure.** Use `git mv` for every move so rename detection preserves blame. Fix `synthetic_reco_B115T3deg.tex` `..` depth and any `.md` relative links broken by the depth change. Confirm `latexmk` rebuilds cleanly for at least one `.tex` per topic folder.
3. **Stage 3 — Add top-level README.** New `README.md` at `docs/reports/reconstruction/`.

## Risks and Trade-offs

- **Path edits beyond just file moves**: `synthetic_reco_B115T3deg.tex` definitely needs depth fix; `\LargeRunRoot` macro needs verification. Mitigation: rebuild `.tex` files post-move as part of Stage 2.
- **Markdown relative links may break**: e.g. cross-document links inside `.md` files. Mitigation: grep for `](` patterns in all `.md` files post-move; fix what broke.
- **Reader bookmarks / external references**: anyone with a bookmark to e.g. `docs/reports/reconstruction/rk_reconstruction_status_20260416.pdf` will get a 404. Acceptable — internal docs, no external publication URL.
- **Git history blame**: `git mv` preserves rename detection; `git log --follow` continues to work across the move.

## Success Criteria

- `docs/reports/reconstruction/` matches the target layout exactly.
- `git status` is clean of LaTeX build artifacts after a fresh `latexmk` run anywhere in the tree.
- Every `.tex` in the tree compiles successfully post-move (sanity check, not full CI).
- Every `\includegraphics` resolves; every `.md` relative link resolves.
- `git log --follow` returns the pre-move history for at least one moved file (smoke test).
- Top-level `README.md` exists and links to all subdirectories.
