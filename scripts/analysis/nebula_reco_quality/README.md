# nebula_reco_quality

Reproducible pipeline for the NEBULA reco quality investigation
(`docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.tex`).

## Goal

Two questions about NEBULA reconstruction:

- **Part A**: trace the source of the Δp_x two-peak observed in
  `nn_breakup_phys_20260503_zh.tex` §5.4 to specific lines of code.
- **Part B**: measure NEBULA acceptance ε(px,py) as the product
  geometric × Geant4-detection × cluster-reco, quantify each layer's
  contribution to the ypol R observable.

Both are read-only — no NEBULAReconstructor / Converter code is modified.

## File map

| File | Role |
|------|------|
| `geom_acceptance.py` | Pure-Python ray-cast against NEBULA bar AABBs (single source of truth for ε_geom) |
| `test_geom_acceptance.py` + `conftest.py` | 8 pytest tests, run from repo root or this dir |
| `partA_two_peak.py` | Part A figures (fig_A1/A2/A3) from joined.parquet |
| `gen_pxpy_grid_input.py` | Sharded TBeamSimData tree-gun inputs (4.41M events / 32 shards) |
| `gen_scan_macros.py` | Per-shard sim_deuteron macros (Tree gun + 3deg_1.15T.mac) |
| `compute_geom_acceptance.py` | ε_geom grid → `eps_geom_grid.parquet` |
| `run_scan_on_labenpg.sh` | Parallel sim_deuteron driver on labenpg-hk (xargs -P) |
| `run_nebula_reco.C` + `run_reco_on_labenpg.sh` | Standalone NEBULA-only reco + parallel driver |
| `summarize_scan_outputs.py` | Concatenate per-shard reco summary roots → `scan_summary.parquet` |
| `analyze_efficiency.py` | Three-layer + conditional efficiencies → `efficiency_grid.parquet` |
| `make_efficiency_figures.py` | Part B figures (fig_B1/B2/B3) |
| `apply_to_breakup.py` | Weight ε by breakup truth → `R_acceptance_table.csv` |

## Where outputs live

All produced under `build/test_output/nebula_reco_quality_20260513/` (gitignored):

- `figs/fig_A*.pdf`, `figs/fig_B*.pdf` — figures
- `inputs/grid_shard*.root` — tree-gun inputs (32 shards)
- `g4output/pxpy_scan_shard*.root` — sim_deuteron outputs
- `summary/pxpy_scan_shard*.root` — per-shard NEBULA reco summary trees
- `scan_summary.parquet` — all 4.41M events with truth + NEBULA reco
- `eps_geom_grid.parquet` — ε_geom over 21×21×5 grid
- `efficiency_grid.parquet` — final three-layer + conditional ε per (px,py)
- `R_acceptance_table.csv` — R asymmetry table (Part B §3.5)

The compiled report PDF and figures are committed under
`docs/reports/reconstruction/nebula/` so they survive build cleans.

## Pipeline order

### Part A (local)

1. `02_extract_observables.C` (in `../nn_breakup_phys/`) emits `hit_mult_n`
   column. Re-run the extraction + analyze pipeline:
   ```bash
   bash ../nn_breakup_phys/run_extract_all.sh
   python ../nn_breakup_phys/03_analyze_r_breakup.py --pol ypol ...
   ```
2. ```bash
   python partA_two_peak.py --joined ... --out-dir ...
   ```
   → 3 PDF figures (fig_A1/A2/A3).

### Part B (labenpg-hk for compute, local for plotting/report)

All compute lives at `/data/tian/workspace/dpol/smsimulator5.5/` on labenpg.

1. **Generate inputs + macros** (on labenpg or local):
   ```bash
   python gen_pxpy_grid_input.py --out-prefix ...inputs/grid --n-shards 32 ...
   python gen_scan_macros.py --input-prefix ... --n-shards 32 ...
   ```
2. **Precompute ε_geom** (local or remote, takes seconds):
   ```bash
   python compute_geom_acceptance.py --out ...eps_geom_grid.parquet
   ```
3. **Sim** (on labenpg, ~12 min wallclock at N=24 parallel):
   ```bash
   bash run_scan_on_labenpg.sh 24
   ```
4. **NEBULA reco** (on labenpg, ~10 min wallclock):
   ```bash
   bash run_reco_on_labenpg.sh 24
   ```
5. **Summarize → parquet** (on labenpg):
   ```bash
   python summarize_scan_outputs.py --summary-glob "...summary/pxpy_scan_shard*.root" --out scan_summary.parquet
   ```
6. **Analyze + plot + table** (on labenpg, rsync results back):
   ```bash
   python analyze_efficiency.py --scan-summary ... --geom-grid ... --out efficiency_grid.parquet
   python make_efficiency_figures.py --efficiency ... --out-dir figs
   python apply_to_breakup.py --joined ... --efficiency ... --out R_acceptance_table.csv
   ```
7. **rsync PDFs + CSV back to local**, then update tex sections.

## Tests

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
pytest scripts/analysis/nebula_reco_quality/ -v
```
Expected: 8 passed.

## Key findings (for future-me)

- **Two-peak source** is NEBULA bar X-discreteness (12 cm pitch) + 5 mm
  smearing too small. The `n_reco_neutrons==1` filter does NOT cut multi-bar
  clusters — multi-bar still produces 1 RecoNeutron (proven by `hit_mult_n`
  split: 57% of NEBULA-hit events are multi-bar). Real source: single-bar
  group's ±5 MeV/c outer peaks + multi-bar central peak superposition.

- **Bottleneck layer**: Geant4 detection (~43%), not reconstruction (~99.5%).
  NEBULA cluster reco is essentially lossless.

- **R sensitivity to acceptance**: ε_reco weighting shifts ypol R by
  +55-66% under loose/mid cuts. Acceptance correction is mandatory before
  interpreting R as polarization.

## Environment notes

- Local: micromamba env `anaroot-env`, pyarrow added.
- labenpg-hk: same env at `/data/tian/conda`; install pyarrow + uproot
  if missing. `setup.sh` sets ROOT_INCLUDE_PATH so PyROOT can find
  TBeamSimData.hh (rootmap header lookup).
- ssh: use `labenpg-hk` (proxy via tex.enpg.cn); direct `labenpg` drops
  packets per `reference_labenpg_remote_build.md` memory.
- labenpg working tree has a local mod on `rootlogon.C` (lib path
  hardcoded for `/data/tian`); stash it before `git push` to that remote.
