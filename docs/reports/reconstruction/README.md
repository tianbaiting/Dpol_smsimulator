# Reconstruction Documentation

Reports, design specs, and notes for the SAMURAI/DPOL reconstruction pipeline. The pipeline runs in two stages, and the folder layout mirrors that:

```
hits ──► [track_reco] ──► tracks ──► [momentum_reco] ──► p_target
```

## Subdirectories

| Path | Contents |
|------|----------|
| `methods/` | Cross-topic algorithm overviews & literature surveys (anaroot reconstruction methods, target-momentum algorithm research, **PDC reco literature & air-MS gap analysis (2026-04-26)**) |
| `references/` | External papers cited by other documents in this directory |
| `track_reco/pdc/` | PDC track-reconstruction reports (uncertainty studies) |
| `track_reco/fdc/` | FDC track-reconstruction reports |
| `momentum_reco/rk/` | Runge-Kutta solver status, bugfix notes, fluctuation figures |
| `momentum_reco/nn/` | Neural-network backend retraining notes |
| `momentum_reco/synthetic/` | Synthetic-data validation studies |
| `momentum_reco/ms_ablation/` | Multiple-scattering ablation studies (Region A / B decomposition). Has its own `README.md`, `specs/`, `plans/`, `figures/`, `data/`. |

## Where to start

- **First time here?** Read [`methods/anaroot_reconstruction_methods.zh.md`](methods/anaroot_reconstruction_methods.zh.md) for the algorithm overview.
- **Latest finding on RK error budget?** See [`momentum_reco/ms_ablation/README.md`](momentum_reco/ms_ablation/README.md).
- **Latest RK status report?** See [`momentum_reco/rk/rk_reconstruction_status_20260416.pdf`](momentum_reco/rk/rk_reconstruction_status_20260416.pdf) (or `_zh.pdf` for the Chinese version).
- **Literature survey on PDC reco + air-MS gap?** See [`methods/pdc_reco_literature_and_air_ms_gap_20260426.pdf`](methods/pdc_reco_literature_and_air_ms_gap_20260426.pdf) (or [`_zh.pdf`](methods/pdc_reco_literature_and_air_ms_gap_20260426_zh.pdf) for the Chinese version).

## Conventions

- LaTeX build artifacts (`.aux`, `.log`, `.fls`, `.fdb_latexmk`, `.nav`, `.out`, `.snm`, `.toc`, `.vrb`, `.xdv`) are gitignored at the repo root.
- Per-topic folders only contain `figures/`, `specs/`, `plans/` when those have content (see `momentum_reco/ms_ablation/` for the full pattern).
- Markdown filenames ending in `.zh.md` are Chinese-language; otherwise English.
