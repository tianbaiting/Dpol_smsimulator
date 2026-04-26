# Reconstruction Docs Reorg Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize `docs/reports/reconstruction/` into algorithm/topic folders following the two-stage pipeline structure, clean LaTeX build artifacts, and add an entry-point README.

**Architecture:** Three independent commits — (1) artifact cleanup, (2) folder restructure with `git mv` and path-reference fixes verified by `latexmk` smoke test, (3) new top-level `README.md`.

**Tech Stack:** git, bash, latexmk/xelatex (for smoke test only).

**Spec:** `docs/superpowers/specs/2026-04-26-reconstruction-docs-reorg-design.md`

**Working directory for all commands:** `/home/tian/workspace/dpol/smsimulator5.5` (run all commands from repo root).

---

## Task 1: Clean LaTeX Build Artifacts

**Files:**
- Delete (untracked): `docs/reports/reconstruction/*.{aux,fdb_latexmk,fls,log,nav,out,snm,toc,vrb,xdv}` and `missfont.log`
- Remove from git: `docs/reports/reconstruction/pdc_reconstruction_uncertainty_detailed.xdv`
- Remove from git: `docs/reports/reconstruction/rk_reconstruction_status_20260416.xdv`
- Remove from git: `docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.xdv`

- [ ] **Step 1: Sanity-check the count of untracked artifacts**

```bash
ls docs/reports/reconstruction/*.{aux,fdb_latexmk,fls,log,nav,out,snm,toc,vrb,xdv} \
   docs/reports/reconstruction/missfont.log 2>/dev/null | wc -l
```

Expected output: `53` (the count seen at planning time; if different, verify by inspecting `git status`)

- [ ] **Step 2: Verify the 3 tracked .xdv files**

```bash
git ls-files docs/reports/reconstruction/ | grep -E '\.xdv$'
```

Expected output (3 lines):
```
docs/reports/reconstruction/pdc_reconstruction_uncertainty_detailed.xdv
docs/reports/reconstruction/rk_reconstruction_status_20260416.xdv
docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.xdv
```

- [ ] **Step 3: Delete untracked artifacts**

```bash
rm -f docs/reports/reconstruction/*.aux \
      docs/reports/reconstruction/*.fdb_latexmk \
      docs/reports/reconstruction/*.fls \
      docs/reports/reconstruction/*.log \
      docs/reports/reconstruction/*.nav \
      docs/reports/reconstruction/*.out \
      docs/reports/reconstruction/*.snm \
      docs/reports/reconstruction/*.toc \
      docs/reports/reconstruction/*.vrb \
      docs/reports/reconstruction/*.xdv \
      docs/reports/reconstruction/missfont.log
```

- [ ] **Step 4: Verify tracked .xdv files are now restored by git but the untracked ones gone**

```bash
git status --short docs/reports/reconstruction/ | head -10
```

Expected: 3 lines starting with ` D ` for the three `.xdv` files; no other deletions.

- [ ] **Step 5: `git rm` the 3 tracked .xdv files**

```bash
git rm docs/reports/reconstruction/pdc_reconstruction_uncertainty_detailed.xdv \
       docs/reports/reconstruction/rk_reconstruction_status_20260416.xdv \
       docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.xdv
```

Expected output:
```
rm 'docs/reports/reconstruction/pdc_reconstruction_uncertainty_detailed.xdv'
rm 'docs/reports/reconstruction/rk_reconstruction_status_20260416.xdv'
rm 'docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.xdv'
```

- [ ] **Step 6: Confirm clean git status (only the 3 deletions staged)**

```bash
git status --short docs/reports/reconstruction/
```

Expected: exactly 3 lines starting with `D  ` (capital D, staged) for the `.xdv` files.

- [ ] **Step 7: Commit**

```bash
git commit -m "docs(reconstruction): drop tracked LaTeX .xdv build artifacts"
```

---

## Task 2: Create New Folder Skeleton

**Files:**
- Create: `docs/reports/reconstruction/methods/`
- Create: `docs/reports/reconstruction/references/`
- Create: `docs/reports/reconstruction/track_reco/pdc/`
- Create: `docs/reports/reconstruction/track_reco/fdc/`
- Create: `docs/reports/reconstruction/momentum_reco/rk/`
- Create: `docs/reports/reconstruction/momentum_reco/nn/`
- Create: `docs/reports/reconstruction/momentum_reco/synthetic/`

- [ ] **Step 1: Create folders**

```bash
mkdir -p docs/reports/reconstruction/methods \
         docs/reports/reconstruction/references \
         docs/reports/reconstruction/track_reco/pdc \
         docs/reports/reconstruction/track_reco/fdc \
         docs/reports/reconstruction/momentum_reco/rk \
         docs/reports/reconstruction/momentum_reco/nn \
         docs/reports/reconstruction/momentum_reco/synthetic
```

- [ ] **Step 2: Verify all 7 folders exist**

```bash
find docs/reports/reconstruction/{methods,references,track_reco,momentum_reco} -type d | sort
```

Expected output:
```
docs/reports/reconstruction/methods
docs/reports/reconstruction/momentum_reco
docs/reports/reconstruction/momentum_reco/nn
docs/reports/reconstruction/momentum_reco/rk
docs/reports/reconstruction/momentum_reco/synthetic
docs/reports/reconstruction/references
docs/reports/reconstruction/track_reco
docs/reports/reconstruction/track_reco/fdc
docs/reports/reconstruction/track_reco/pdc
```

(No commit yet — empty folders aren't tracked anyway. Folders will commit alongside their contents in Task 7.)

---

## Task 3: Move Methods, References, Track Reco Files

**Files moved (using `git mv` to preserve rename detection):**
- `docs/reports/reconstruction/anaroot_reconstruction_methods.zh.md` → `methods/`
- `docs/reports/reconstruction/靶点动量重建算法研究.md` → `methods/`
- `docs/reports/reconstruction/2019孙亚洲.pdf` → `references/`
- `docs/reports/reconstruction/pdc_reconstruction_uncertainty_detailed.{tex,pdf}` → `track_reco/pdc/`
- `docs/reports/reconstruction/fdc_reconstruction_detailed.{tex,pdf}` → `track_reco/fdc/`

- [ ] **Step 1: Move methods overviews**

```bash
git mv docs/reports/reconstruction/anaroot_reconstruction_methods.zh.md \
       docs/reports/reconstruction/methods/
git mv "docs/reports/reconstruction/靶点动量重建算法研究.md" \
       docs/reports/reconstruction/methods/
```

- [ ] **Step 2: Move external reference**

```bash
git mv "docs/reports/reconstruction/2019孙亚洲.pdf" \
       docs/reports/reconstruction/references/
```

- [ ] **Step 3: Move PDC reconstruction reports**

```bash
git mv docs/reports/reconstruction/pdc_reconstruction_uncertainty_detailed.tex \
       docs/reports/reconstruction/track_reco/pdc/
git mv docs/reports/reconstruction/pdc_reconstruction_uncertainty_detailed.pdf \
       docs/reports/reconstruction/track_reco/pdc/
```

- [ ] **Step 4: Move FDC reconstruction reports**

```bash
git mv docs/reports/reconstruction/fdc_reconstruction_detailed.tex \
       docs/reports/reconstruction/track_reco/fdc/
git mv docs/reports/reconstruction/fdc_reconstruction_detailed.pdf \
       docs/reports/reconstruction/track_reco/fdc/
```

- [ ] **Step 5: Verify rename detection on a sample file**

```bash
git status --short docs/reports/reconstruction/ | head -20
```

Expected: lines starting with `R ` (rename detected) for each moved file. No `D ` (deleted) followed by `A ` (added) splits.

If any file shows `D ` + `A ` instead of `R `, the rename heuristic missed because the file is identical-but-tiny — that's still correct, just unaesthetic; proceed.

(No commit — accumulating all moves, fixes, and smoke test into one Stage 2 commit at the end of Task 7.)

---

## Task 4: Move RK Files (with figures and archive)

**Files moved:**
- `docs/reports/reconstruction/rk_reconstruction_status_20260416.{tex,pdf}` → `momentum_reco/rk/`
- `docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.{tex,pdf}` → `momentum_reco/rk/`
- `docs/reports/reconstruction/rk_reconstruction_bugfix_and_error_analysis_20260416.md` → `momentum_reco/rk/`
- `docs/reports/reconstruction/figures/` (whole subtree) → `momentum_reco/rk/figures/`
- `docs/reports/reconstruction/archive/` (whole subtree) → `momentum_reco/rk/archive/`

- [ ] **Step 1: Move RK status reports (EN + zh)**

```bash
git mv docs/reports/reconstruction/rk_reconstruction_status_20260416.tex \
       docs/reports/reconstruction/momentum_reco/rk/
git mv docs/reports/reconstruction/rk_reconstruction_status_20260416.pdf \
       docs/reports/reconstruction/momentum_reco/rk/
git mv docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.tex \
       docs/reports/reconstruction/momentum_reco/rk/
git mv docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.pdf \
       docs/reports/reconstruction/momentum_reco/rk/
```

- [ ] **Step 2: Move RK bugfix markdown**

```bash
git mv docs/reports/reconstruction/rk_reconstruction_bugfix_and_error_analysis_20260416.md \
       docs/reports/reconstruction/momentum_reco/rk/
```

- [ ] **Step 3: Move figures/ subtree (only RK references it)**

```bash
git mv docs/reports/reconstruction/figures \
       docs/reports/reconstruction/momentum_reco/rk/figures
```

- [ ] **Step 4: Move archive/ (single PNG, RK-related per filename)**

```bash
git mv docs/reports/reconstruction/archive \
       docs/reports/reconstruction/momentum_reco/rk/archive
```

- [ ] **Step 5: Verify RK figures path is unchanged relative to .tex**

```bash
ls docs/reports/reconstruction/momentum_reco/rk/figures/g4_fluctuation/pdc_hits/overview.png
```

Expected: file listed (exists). This confirms `\includegraphics{figures/g4_fluctuation/pdc_hits/overview.png}` in the moved `.tex` still resolves.

---

## Task 5: Move NN, Synthetic, MS Ablation Files

**Files moved:**
- `docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg.{tex,pdf}` → `momentum_reco/nn/`
- `docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg_notes.zh.md` → `momentum_reco/nn/`
- `docs/reports/reconstruction/synthetic_reco_B115T3deg.{tex,pdf}` → `momentum_reco/synthetic/`
- `docs/reports/reconstruction/ms_ablation/` (whole subtree) → `momentum_reco/ms_ablation/`

- [ ] **Step 1: Move NN retrain docs**

```bash
git mv docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg.tex \
       docs/reports/reconstruction/momentum_reco/nn/
git mv docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg.pdf \
       docs/reports/reconstruction/momentum_reco/nn/
git mv docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg_notes.zh.md \
       docs/reports/reconstruction/momentum_reco/nn/
```

- [ ] **Step 2: Move synthetic-data study**

```bash
git mv docs/reports/reconstruction/synthetic_reco_B115T3deg.tex \
       docs/reports/reconstruction/momentum_reco/synthetic/
git mv docs/reports/reconstruction/synthetic_reco_B115T3deg.pdf \
       docs/reports/reconstruction/momentum_reco/synthetic/
```

- [ ] **Step 3: Move ms_ablation subtree (preserves all internal links)**

```bash
git mv docs/reports/reconstruction/ms_ablation \
       docs/reports/reconstruction/momentum_reco/ms_ablation
```

- [ ] **Step 4: Confirm top level is now empty of files except subdirectories**

```bash
ls -la docs/reports/reconstruction/ | grep -vE '^d|^total'
```

Expected: no output (no files left at the top level — all are inside subfolders). Folders shown by the inverted filter excluded.

---

## Task 6: Fix Path References Broken by Depth Change

Two `.tex` files need `..` adjustments because they reference paths *outside* this directory:

| File | Old depth | New depth | `..` correction |
|------|-----------|-----------|-----------------|
| `synthetic_reco_B115T3deg.tex` | 3 (`docs/reports/reconstruction/`) | 5 (`.../momentum_reco/synthetic/`) | `../../../` → `../../../../../` |
| `pdc_reconstruction_uncertainty_detailed.tex` | 3 | 5 (`.../track_reco/pdc/`) | `../../../` → `../../../../../` (in `\LargeRunRoot` macro on line 16) |

The `ms_ablation/` subtree only moved up by one container level but all its internal links are *relative to its own root*, so they're unaffected.

**Files modified:**
- `docs/reports/reconstruction/momentum_reco/synthetic/synthetic_reco_B115T3deg.tex`
- `docs/reports/reconstruction/track_reco/pdc/pdc_reconstruction_uncertainty_detailed.tex`

- [ ] **Step 1: Fix `synthetic_reco_B115T3deg.tex` includegraphics depths**

Edit file `docs/reports/reconstruction/momentum_reco/synthetic/synthetic_reco_B115T3deg.tex`. Replace **all** occurrences of `../../../results/synthetic_reco/` with `../../../../../results/synthetic_reco/`.

The 6 affected paths (each appears in `\includegraphics{...}`):
- `../../../results/synthetic_reco/B115T_3deg/plots/neutron_dpx.png`
- `../../../results/synthetic_reco/B115T_3deg/plots/neutron_px_true_vs_reco_and_dpx_vs_true.png`
- `../../../results/synthetic_reco/B115T_3deg/plots/pdc_uniform_track_efficiency.png`
- `../../../results/synthetic_reco/B115T_3deg/plots/proton_dpx_comparison.png`
- `../../../results/synthetic_reco/B115T_3deg/plots/proton_dpx_vs_true.png`
- `../../../results/synthetic_reco/B115T_3deg/plots/proton_px_true_vs_reco.png`

Use a single `sed -i` since the substring is uniform:

```bash
sed -i 's|\.\./\.\./\.\./results/synthetic_reco/|../../../../../results/synthetic_reco/|g' \
    docs/reports/reconstruction/momentum_reco/synthetic/synthetic_reco_B115T3deg.tex
```

- [ ] **Step 2: Verify the substitution**

```bash
grep -nE 'results/synthetic_reco/' \
    docs/reports/reconstruction/momentum_reco/synthetic/synthetic_reco_B115T3deg.tex
```

Expected: 6 lines, each prefixed with `../../../../../results/synthetic_reco/...`. No occurrence of `../../../results/synthetic_reco/`.

```bash
grep -c '\.\./\.\./\.\./results/' \
    docs/reports/reconstruction/momentum_reco/synthetic/synthetic_reco_B115T3deg.tex
```

Expected: `0`.

- [ ] **Step 3: Verify the target results files actually exist**

```bash
ls /home/tian/workspace/dpol/smsimulator5.5/results/synthetic_reco/B115T_3deg/plots/neutron_dpx.png
```

Expected: file listed.

- [ ] **Step 4: Fix `pdc_reconstruction_uncertainty_detailed.tex` `\LargeRunRoot` macro**

The macro is on line 16:
```
\newcommand{\LargeRunRoot}{../../../results/pdc_rk_error_after_pdcana_20260411}
```

Change to:
```
\newcommand{\LargeRunRoot}{../../../../../results/pdc_rk_error_after_pdcana_20260411}
```

```bash
sed -i 's|\\newcommand{\\LargeRunRoot}{\.\./\.\./\.\./results/|\\newcommand{\\LargeRunRoot}{../../../../../results/|' \
    docs/reports/reconstruction/track_reco/pdc/pdc_reconstruction_uncertainty_detailed.tex
```

- [ ] **Step 5: Verify the macro substitution**

```bash
grep -n 'LargeRunRoot' \
    docs/reports/reconstruction/track_reco/pdc/pdc_reconstruction_uncertainty_detailed.tex
```

Expected: line 16 reads:
```
16:\newcommand{\LargeRunRoot}{../../../../../results/pdc_rk_error_after_pdcana_20260411}
```

- [ ] **Step 6: Verify the target results path exists**

```bash
ls /home/tian/workspace/dpol/smsimulator5.5/results/pdc_rk_error_after_pdcana_20260411/ | head -3
```

Expected: at least the `plots/` subdirectory listed.

---

## Task 7: Smoke-Test LaTeX Compilation and Commit Stage 2

Compile one `.tex` per topic folder to confirm no path or include is broken. Skip files with no figures (no need to verify).

- [ ] **Step 1: Compile RK status (English) — has the most figure references**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/docs/reports/reconstruction/momentum_reco/rk && \
  latexmk -xelatex -interaction=nonstopmode rk_reconstruction_status_20260416.tex && \
  cd - >/dev/null
```

Expected: ends with `Latexmk: All targets ... are up-to-date` or successful PDF write. No "File ... not found" errors. PDF page count should match the pre-move version (~30+ pages).

- [ ] **Step 2: Compile synthetic_reco (verifies the depth fix)**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/docs/reports/reconstruction/momentum_reco/synthetic && \
  latexmk -xelatex -interaction=nonstopmode synthetic_reco_B115T3deg.tex && \
  cd - >/dev/null
```

Expected: success; the 6 PNGs from `results/synthetic_reco/...` resolve.

- [ ] **Step 3: Compile pdc_reconstruction_uncertainty (verifies `\LargeRunRoot` fix)**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/docs/reports/reconstruction/track_reco/pdc && \
  latexmk -xelatex -interaction=nonstopmode pdc_reconstruction_uncertainty_detailed.tex && \
  cd - >/dev/null
```

Expected: success; the `\LargeRunRoot/plots/reco_p_distribution.png` resolves.

- [ ] **Step 4: Clean up sidecar artifacts produced by smoke compile**

The compiles create the same `.aux/.log/.fls/...` artifacts that Task 1 cleaned. Root `.gitignore` covers them so they won't be committed, but tidy the working tree:

```bash
find docs/reports/reconstruction/momentum_reco docs/reports/reconstruction/track_reco \
  -maxdepth 3 -type f \( -name '*.aux' -o -name '*.log' -o -name '*.fls' \
  -o -name '*.fdb_latexmk' -o -name '*.nav' -o -name '*.out' -o -name '*.snm' \
  -o -name '*.toc' -o -name '*.vrb' -o -name '*.xdv' \) -delete
```

- [ ] **Step 5: Verify final git status (only renames + 2 .tex modifications)**

```bash
git status --short docs/reports/reconstruction/
```

Expected pattern (counts approximate):
- Several `R ` (rename) lines for every moved file
- 1 `M ` for `momentum_reco/synthetic/synthetic_reco_B115T3deg.tex` (path edit)
- 1 `M ` for `track_reco/pdc/pdc_reconstruction_uncertainty_detailed.tex` (path edit)
- Possibly 2 `M ` for the recompiled PDFs (if latexmk re-rendered byte-different output) — that's fine, the new PDFs include the corrected paths.

NO untracked files remaining.

- [ ] **Step 6: Verify a sample `git log --follow` returns pre-move history**

```bash
git log --follow --oneline -3 -- \
  docs/reports/reconstruction/momentum_reco/rk/rk_reconstruction_status_20260416.tex
```

Expected: at least one commit before today (rename detection working).

- [ ] **Step 7: Commit Stage 2**

```bash
git add docs/reports/reconstruction/
git commit -m "docs(reconstruction): reorganize into track_reco/ and momentum_reco/ topic folders"
```

---

## Task 8: Add Top-Level README

**Files:**
- Create: `docs/reports/reconstruction/README.md`

- [ ] **Step 1: Write the README**

Create `docs/reports/reconstruction/README.md` with this exact content:

````markdown
# Reconstruction Documentation

Reports, design specs, and notes for the SAMURAI/DPOL reconstruction pipeline. The pipeline runs in two stages, and the folder layout mirrors that:

```
hits ──► [track_reco] ──► tracks ──► [momentum_reco] ──► p_target
```

## Subdirectories

| Path | Contents |
|------|----------|
| `methods/` | Cross-topic algorithm overviews (anaroot reconstruction methods, target-momentum algorithm research) |
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

## Conventions

- LaTeX build artifacts (`.aux`, `.log`, `.fls`, `.fdb_latexmk`, `.nav`, `.out`, `.snm`, `.toc`, `.vrb`, `.xdv`) are gitignored at the repo root.
- Per-topic folders only contain `figures/`, `specs/`, `plans/` when those have content (see `momentum_reco/ms_ablation/` for the full pattern).
- Markdown filenames ending in `.zh.md` are Chinese-language; otherwise English.
````

- [ ] **Step 2: Verify the README renders correctly (markdown link validity)**

```bash
grep -oE '\]\([^)]+\)' docs/reports/reconstruction/README.md | sed 's/.*(//;s/)//' | while read p; do
  if [ -e "docs/reports/reconstruction/$p" ]; then echo "OK: $p"; else echo "MISSING: $p"; fi
done
```

Expected: all lines start with `OK:`. Any `MISSING:` line is a broken link to fix.

- [ ] **Step 3: Commit**

```bash
git add docs/reports/reconstruction/README.md
git commit -m "docs(reconstruction): add top-level README with pipeline overview"
```

---

## Task 9: Final Verification

- [ ] **Step 1: Confirm target layout matches the spec**

```bash
find docs/reports/reconstruction -maxdepth 3 -type d | sort
```

Expected output:
```
docs/reports/reconstruction
docs/reports/reconstruction/methods
docs/reports/reconstruction/momentum_reco
docs/reports/reconstruction/momentum_reco/ms_ablation
docs/reports/reconstruction/momentum_reco/ms_ablation/data
docs/reports/reconstruction/momentum_reco/ms_ablation/figures
docs/reports/reconstruction/momentum_reco/ms_ablation/plans
docs/reports/reconstruction/momentum_reco/ms_ablation/specs
docs/reports/reconstruction/momentum_reco/nn
docs/reports/reconstruction/momentum_reco/rk
docs/reports/reconstruction/momentum_reco/rk/archive
docs/reports/reconstruction/momentum_reco/rk/figures
docs/reports/reconstruction/momentum_reco/rk/figures/g4_fluctuation
docs/reports/reconstruction/momentum_reco/rk/figures/g4_fluctuation/pdc_hits
docs/reports/reconstruction/momentum_reco/rk/figures/g4_fluctuation/reco_momentum
docs/reports/reconstruction/momentum_reco/rk/figures/geometry
docs/reports/reconstruction/momentum_reco/synthetic
docs/reports/reconstruction/references
docs/reports/reconstruction/track_reco
docs/reports/reconstruction/track_reco/fdc
docs/reports/reconstruction/track_reco/pdc
```

- [ ] **Step 2: Confirm no LaTeX build artifacts anywhere in the directory**

```bash
find docs/reports/reconstruction -type f \( -name '*.aux' -o -name '*.log' \
  -o -name '*.fls' -o -name '*.fdb_latexmk' -o -name '*.nav' -o -name '*.out' \
  -o -name '*.snm' -o -name '*.toc' -o -name '*.vrb' -o -name '*.xdv' \
  -o -name 'missfont.log' \) | wc -l
```

Expected: `0`.

- [ ] **Step 3: Confirm 3 commits added on top of the spec commit**

```bash
git log --oneline -5
```

Expected: top 3 commits are (most recent first):
```
docs(reconstruction): add top-level README with pipeline overview
docs(reconstruction): reorganize into track_reco/ and momentum_reco/ topic folders
docs(reconstruction): drop tracked LaTeX .xdv build artifacts
```
followed by `docs: spec for docs/reports/reconstruction reorg ...`

- [ ] **Step 4: Confirm nothing left at top level except README and folders**

```bash
ls docs/reports/reconstruction/
```

Expected: only these entries (any order):
```
README.md
methods
momentum_reco
references
track_reco
```

- [ ] **Step 5: Final clean tree**

```bash
git status
```

Expected: `nothing to commit, working tree clean`.

---

## Rollback

Each commit is independently revertable. If something goes wrong:

- Stage 3 broken: `git revert HEAD` (drops the README only)
- Stage 2 broken: `git revert HEAD~1` (un-moves files; rename detection means PDFs etc. come back)
- Stage 1 broken: `git revert HEAD~2` (restores the 3 `.xdv` files)
