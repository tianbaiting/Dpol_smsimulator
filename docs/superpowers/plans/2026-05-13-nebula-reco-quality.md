# NEBULA Reco Quality Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce one zh report that (Part A) traces the Δp_x two-peak in NEBULA neutron reconstruction back to source code with validation figures from existing 4.59M ypol breakup data, and (Part B) quantifies NEBULA's three-layer efficiency (geometric / detection / reconstruction) over a (px,py) grid using a fresh truth-only neutron-gun scan on labenpg.

**Architecture:** Part A is pure local analysis — extend an existing extraction macro (`02_extract_observables.C`) to emit `hit_mult_n`, re-run extraction + analysis pipeline, then build new figures and tex sections. Part B is a remote Geant4 campaign on labenpg-hk: local Python generates a tree-gun input root and per-job macros, ssh/rsync drives parallel sim_deuteron + reconstruction jobs on labenpg, results summarize into a small parquet that rsyncs back for local analysis and figures. No reconstruction code is modified.

**Tech Stack:** C++ ROOT macros, Python 3 (pandas / numpy / pyarrow / matplotlib), bash + rsync + ssh, Geant4 11.3.2 + ROOT 6.36 via conda env `anaroot-env`, LaTeX (XeLaTeX + ctex).

---

## Reference Spec
`docs/superpowers/specs/2026-05-13-nebula-reco-quality-design.md` (commit `0f8b4ec`)

## File Map

**New files (all under `scripts/analysis/nebula_reco_quality/`)**
- `partA_two_peak.py` — Δp_x split by hit multiplicity, generates Part A figures
- `geom_acceptance.py` — ε_geom ray-cast core (testable module)
- `test_geom_acceptance.py` — pytest tests for ε_geom
- `gen_pxpy_grid_input.py` — generates tree-gun input root + manifest CSV
- `gen_scan_macros.py` — generates per-job .mac files
- `compute_geom_acceptance.py` — runs ε_geom over grid, writes parquet
- `run_scan_on_labenpg.sh` — driver: rsync → parallel sim_deuteron → reconstruct → summarize → rsync back
- `summarize_scan_outputs.py` — reads g4output + reco root per chunk, emits per-event parquet (runs on labenpg)
- `analyze_efficiency.py` — bins events, computes three layered + conditional efficiencies
- `make_efficiency_figures.py` — 2D heatmaps, slices
- `apply_to_breakup.py` — weights ε(px,py) by breakup truth distribution from existing `joined.parquet`
- `README.md` — workflow doc for this subdirectory

**Modified files**
- `scripts/analysis/nn_breakup_phys/02_extract_observables.C` — add `hit_mult_n` column

**Output files (generated, gitignored)**
- `build/test_output/nebula_reco_quality_20260513/pxpy_grid/*.root` (g4output + reco)
- `build/test_output/nebula_reco_quality_20260513/scan_summary.parquet`
- `build/test_output/nebula_reco_quality_20260513/efficiency_grid.parquet`
- `build/test_output/nebula_reco_quality_20260513/figs/*.pdf`

**Report**
- `docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.tex`
- Compiled PDF + figures subdir

---

## Phase 1 — Part A: Two-Peak Source Trace (Local Only)

### Task A1: Add `hit_mult_n` column to extraction macro

**Files:**
- Modify: `scripts/analysis/nn_breakup_phys/02_extract_observables.C:50-56` (header)
- Modify: `scripts/analysis/nn_breakup_phys/02_extract_observables.C:78-100` (data emission)

- [ ] **Step 1: Add column to CSV header**

Find the header line at `scripts/analysis/nn_breakup_phys/02_extract_observables.C:55`:
```cpp
            << "n_reco_neutrons,reco_pxn,reco_pyn,reco_pzn,reco_n_energy" << endl;
```
Change to:
```cpp
            << "n_reco_neutrons,reco_pxn,reco_pyn,reco_pzn,reco_n_energy,hit_mult_n" << endl;
```

- [ ] **Step 2: Capture `hitMultiplicity` from RecoNeutron**

In the `if (re)` block around line 80, after capturing `n_reco_neutrons`, store the hit multiplicity of the first neutron:
```cpp
        int n_reco_neutrons = 0;
        double rpxn_=0, rpyn_=0, rpzn_=0, ren_=0, rbeta_=0;
        int hit_mult_n_ = 0;
        if (re) {
            n_reco_neutrons = (int)re->neutrons.size();
            if (n_reco_neutrons > 0) {
                const auto& neu = re->neutrons[0];
                hit_mult_n_ = neu.hitMultiplicity;
                rbeta_ = neu.beta; ren_ = neu.energy;
                if (rbeta_ > 0 && rbeta_ < 1.0) {
                    const double m_n = 939.565;
                    const double gamma_n = 1.0 / sqrt(1.0 - rbeta_*rbeta_);
                    const double p_mag = m_n * gamma_n * rbeta_;
                    rpxn_ = p_mag * neu.direction.X();
                    rpyn_ = p_mag * neu.direction.Y();
                    rpzn_ = p_mag * neu.direction.Z();
                }
            }
        }
```

- [ ] **Step 3: Emit the column in the CSV row**

At line 100, change:
```cpp
            << n_reco_neutrons << "," << rpxn_ << "," << rpyn_ << "," << rpzn_ << "," << ren_ << endl;
```
to:
```cpp
            << n_reco_neutrons << "," << rpxn_ << "," << rpyn_ << "," << rpzn_ << "," << ren_ << "," << hit_mult_n_ << endl;
```

- [ ] **Step 4: Smoke compile via root** (no separate test, the C macro is JIT-loaded by root each run)

Run:
```bash
cd /home/tian/workspace/dpol/smsimulator5.5
root -l -b -q 'scripts/analysis/nn_breakup_phys/02_extract_observables.C("data/reconstruction/breakup_nn_20260503/y_pol/d+Sn112E190g050ynp_reco_nn.root","/tmp/probe_extract.csv","probe","b0")' 2>&1 | tail -10
head -2 /tmp/probe_extract.csv
```
Expected: header contains `hit_mult_n` as last column; data rows have an integer at position 26.

- [ ] **Step 5: Commit**

```bash
git add scripts/analysis/nn_breakup_phys/02_extract_observables.C
git commit -m "extract: add hit_mult_n column (first RecoNeutron multiplicity) for two-peak split"
```

---

### Task A2: Re-run extraction + analysis pipeline locally

**Files:**
- Read: `scripts/analysis/nn_breakup_phys/run_extract_all.sh`
- Read: `scripts/analysis/nn_breakup_phys/03_analyze_r_breakup.py`

- [ ] **Step 1: Wipe stale CSVs**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
rm -f build/test_output/nn_breakup_phys_20260503/csv/*.csv
rm -f build/test_output/nn_breakup_phys_20260503/csv_per_cell/*.csv
```
(rm is justified here: extraction appends to CSVs by design; old rows would duplicate without column-matching.)

- [ ] **Step 2: Re-run extraction on all ypol cells**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/analysis/nn_breakup_phys/run_extract_all.sh 2>&1 | tail -20
```
Expected: 16 ypol cells + 160 zpol cells processed; per-cell CSV files produced.

- [ ] **Step 3: Verify the new column appears in CSVs**

```bash
head -1 build/test_output/nn_breakup_phys_20260503/csv/d+Sn112E190g050ynp.csv | tr ',' '\n' | grep -n hit_mult_n
```
Expected: `26:hit_mult_n` (column index 26, 1-based).

- [ ] **Step 4: Re-run R-table analysis for ypol**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
mkdir -p build/test_output/nn_breakup_phys_20260503/summary/ypol_v2
python scripts/analysis/nn_breakup_phys/03_analyze_r_breakup.py \
    --csv-dir build/test_output/nn_breakup_phys_20260503/csv \
    --pol ypol \
    --out-dir build/test_output/nn_breakup_phys_20260503/summary/ypol_v2 \
    2>&1 | tail -10
```
Expected: writes `summary/ypol_v2/joined.parquet` (or `joined.csv.gz` if pyarrow missing) containing the `hit_mult_n` column.

- [ ] **Step 5: Verify hit_mult_n is preserved in joined output**

```bash
python -c "
import pandas as pd
df = pd.read_parquet('build/test_output/nn_breakup_phys_20260503/summary/ypol_v2/joined.parquet')
print('rows:', len(df), 'cols:', list(df.columns))
nh = df[df.n_reco_neutrons > 0]
print('NEBULA-hit rows:', len(nh))
print('hit_mult_n value counts (NEBULA-hit subset):')
print(nh.hit_mult_n.value_counts().sort_index().head(10))
"
```
Expected: `hit_mult_n` listed in columns; value counts show 1 dominates with a smaller tail at 2,3,4 etc.

- [ ] **Step 6: Commit the regenerated summaries (data is gitignored under build/; only commit code if changed)**

No new code in this task. Verify nothing in `git status` other than untracked build outputs:
```bash
git status --short
```
Expected: clean (or only untracked build/test_output/ files).

---

### Task A3: Build `geom_acceptance.py` core (TDD)

This is used by both Part A (verifying which bar each NEBULA hit falls in) and Part B (precomputed ε_geom). Build it first with tests.

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/__init__.py` (empty, makes it a package for pytest)
- Create: `scripts/analysis/nebula_reco_quality/geom_acceptance.py`
- Create: `scripts/analysis/nebula_reco_quality/test_geom_acceptance.py`

- [ ] **Step 1: Create directory + empty `__init__.py`**

```bash
mkdir -p /home/tian/workspace/dpol/smsimulator5.5/scripts/analysis/nebula_reco_quality
touch /home/tian/workspace/dpol/smsimulator5.5/scripts/analysis/nebula_reco_quality/__init__.py
```

- [ ] **Step 2: Write failing tests**

Create `scripts/analysis/nebula_reco_quality/test_geom_acceptance.py`:
```python
"""Tests for geom_acceptance: NEBULA geometric acceptance ray-cast."""
import math
from pathlib import Path
import pytest

from geom_acceptance import (
    load_nebula_bars,
    ray_hits_bar,
    geom_acceptance,
    NEBULA_FRONT_Z_MM,
)

REPO = Path(__file__).resolve().parents[3]
DETECTORS_CSV = REPO / "configs/simulation/geometry/NEBULA_Detectors_Dayone.csv"
NEBULA_CSV    = REPO / "configs/simulation/geometry/NEBULA_Dayone.csv"


def test_load_nebula_bars_counts():
    bars = load_nebula_bars(DETECTORS_CSV, NEBULA_CSV, neut_only=True)
    # 60 bars per layer × 2 layers = 120 Neut bars
    assert len(bars) == 120
    # bar half-size should be (60, 900, 60) mm
    b = bars[0]
    assert b.half_x == 60.0
    assert b.half_y == 900.0
    assert b.half_z == 60.0


def test_load_includes_global_position_offset():
    bars = load_nebula_bars(DETECTORS_CSV, NEBULA_CSV, neut_only=True)
    # Global NEBULA position from NEBULA_Dayone.csv is z=7249.72.
    # Layer-1 bar local z = 0, so global z should be ~7249.72.
    layer1 = [b for b in bars if abs(b.center_z - 7249.72) < 1.0]
    assert len(layer1) == 60


def test_central_pencil_hits_bar():
    # px=0,py=0,pz=627: straight-ahead neutron from target (0,0,0).
    # Must hit a NEBULA bar (one of the central 60).
    assert geom_acceptance(0.0, 0.0, 627.0, DETECTORS_CSV, NEBULA_CSV) is True


def test_far_off_axis_misses():
    # px=1000 MeV/c, pz=627, py=0 -> x at NEBULA_z ~= 7250*1000/627 ~= 11.6 m.
    # NEBULA half-width along X is ~1962 mm; this clearly misses.
    assert geom_acceptance(1000.0, 0.0, 627.0, DETECTORS_CSV, NEBULA_CSV) is False


def test_just_inside_x_edge_hits():
    # NEBULA covers roughly x in [-1962, +1962] mm. Pick px such that x at z=7250 is +1500 mm.
    # px/pz = 1500/7250 -> px = 627 * 1500 / 7250 ~= 129.8 MeV/c.
    assert geom_acceptance(129.8, 0.0, 627.0, DETECTORS_CSV, NEBULA_CSV) is True


def test_just_outside_y_edge_misses():
    # NEBULA bar half-height is 900 mm. py such that y at z=7250 = 1500 mm.
    # py/pz = 1500/7250 -> py = 129.8 MeV/c. y=1500 > 900 -> miss.
    assert geom_acceptance(0.0, 129.8, 627.0, DETECTORS_CSV, NEBULA_CSV) is False


def test_negative_pz_returns_false():
    # Backward-going neutron should not hit (NEBULA is downstream of target).
    assert geom_acceptance(0.0, 0.0, -627.0, DETECTORS_CSV, NEBULA_CSV) is False


def test_ray_hits_bar_local_aabb():
    # Direct AABB test against a bar at x=0, y=0, z=7250, half=(60,900,60).
    from geom_acceptance import Bar
    b = Bar(center_x=0.0, center_y=0.0, center_z=7250.0,
            half_x=60.0, half_y=900.0, half_z=60.0)
    # Ray from origin along +z must enter through front face (z = 7190).
    assert ray_hits_bar(0.0, 0.0, 0.0, 0.0, 0.0, 1.0, b)
    # Ray with positive py direction so y at z=7190 is 100 mm -> within ±900.
    dy = 100.0 / 7190.0
    n = math.sqrt(1 + dy*dy)
    assert ray_hits_bar(0.0, 0.0, 0.0, 0.0, dy/n, 1.0/n, b)
```

- [ ] **Step 3: Run failing tests to verify they fail correctly**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/scripts/analysis/nebula_reco_quality
python -m pytest test_geom_acceptance.py -v 2>&1 | tail -20
```
Expected: import errors (`ModuleNotFoundError: No module named 'geom_acceptance'`).

- [ ] **Step 4: Implement `geom_acceptance.py`**

Create `scripts/analysis/nebula_reco_quality/geom_acceptance.py`:
```python
"""NEBULA geometric acceptance: ray-cast from target (0,0,0) to NEBULA bars.

Pure Python; reads bar geometry from the two CSVs that the Geant4 sim also reads,
so this module is the single source of truth for the ε_geom layer of Part B.
"""
from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List


NEBULA_FRONT_Z_MM = 7189.72  # approximate; bars are at z=7249.72 with half_z=60


@dataclass(frozen=True)
class Bar:
    center_x: float
    center_y: float
    center_z: float
    half_x: float
    half_y: float
    half_z: float


def _parse_nebula_global(nebula_csv: Path) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    """Return (global_position_xyz, neut_size_xyz) in mm."""
    pos = (0.0, 0.0, 0.0)
    neut_size = (120.0, 1800.0, 120.0)
    for line in nebula_csv.read_text().splitlines():
        s = line.strip()
        if not s or s.startswith("//"):
            continue
        parts = [p.strip().rstrip(",") for p in s.split(",")]
        key = parts[0]
        if key == "Position":
            pos = (float(parts[1]), float(parts[2]), float(parts[3]))
        elif key == "NeutSize":
            neut_size = (float(parts[1]), float(parts[2]), float(parts[3]))
    return pos, neut_size


def load_nebula_bars(detectors_csv: Path, nebula_csv: Path, neut_only: bool = True) -> List[Bar]:
    pos, neut_size = _parse_nebula_global(nebula_csv)
    half = (neut_size[0]*0.5, neut_size[1]*0.5, neut_size[2]*0.5)
    bars: List[Bar] = []
    for i, line in enumerate(detectors_csv.read_text().splitlines()):
        if i == 0 or not line.strip():
            continue
        parts = [p.strip().rstrip("!") for p in line.split(",")]
        det_type = parts[1].strip()
        if neut_only and det_type != "Neut":
            continue
        bx, by, bz = float(parts[4]), float(parts[5]), float(parts[6])
        bars.append(Bar(
            center_x=bx + pos[0],
            center_y=by + pos[1],
            center_z=bz + pos[2],
            half_x=half[0], half_y=half[1], half_z=half[2],
        ))
    return bars


def ray_hits_bar(ox: float, oy: float, oz: float,
                 dx: float, dy: float, dz: float,
                 bar: Bar) -> bool:
    """Slab-method AABB ray intersection. Direction need not be unit-length."""
    t_min = -float("inf")
    t_max = float("inf")
    for o, d, c, h in (
        (ox, dx, bar.center_x, bar.half_x),
        (oy, dy, bar.center_y, bar.half_y),
        (oz, dz, bar.center_z, bar.half_z),
    ):
        if abs(d) < 1e-12:
            if o < c - h or o > c + h:
                return False
            continue
        t1 = (c - h - o) / d
        t2 = (c + h - o) / d
        if t1 > t2:
            t1, t2 = t2, t1
        t_min = max(t_min, t1)
        t_max = min(t_max, t2)
        if t_min > t_max:
            return False
    return t_max >= max(t_min, 0.0)


def geom_acceptance(px: float, py: float, pz: float,
                    detectors_csv: Path, nebula_csv: Path,
                    bars_cache: dict | None = None) -> bool:
    """Return True iff a neutron leaving target (0,0,0) with momentum (px,py,pz)
    enters any NEBULA Neut bar before going past it. pz must be > 0."""
    if pz <= 0:
        return False
    key = (str(detectors_csv), str(nebula_csv))
    if bars_cache is None or key not in bars_cache:
        bars = load_nebula_bars(detectors_csv, nebula_csv, neut_only=True)
        if bars_cache is not None:
            bars_cache[key] = bars
    else:
        bars = bars_cache[key]
    for b in bars:
        if ray_hits_bar(0.0, 0.0, 0.0, px, py, pz, b):
            return True
    return False
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/scripts/analysis/nebula_reco_quality
python -m pytest test_geom_acceptance.py -v 2>&1 | tail -20
```
Expected: 8 tests passed.

- [ ] **Step 6: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add scripts/analysis/nebula_reco_quality/__init__.py \
        scripts/analysis/nebula_reco_quality/geom_acceptance.py \
        scripts/analysis/nebula_reco_quality/test_geom_acceptance.py
git commit -m "nebula_reco_quality: add geom_acceptance ray-cast module + tests"
```

---

### Task A4: Part A analysis + figure generation

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/partA_two_peak.py`

- [ ] **Step 1: Implement Part A analysis**

Create the script:
```python
"""Part A: Δp_x two-peak source trace.

Loads the regenerated joined.parquet (Task A2), splits NEBULA-hit subset by
hit_mult_n, and emits the three Part A figures:
  fig_A1_dpx_overview      — Δp_x rotated, all events vs NEBULA-hit subset
  fig_A2_dpx_split_mult    — Δp_x rotated split by hit_mult_n (1 vs ≥2)
  fig_A3_nebula_x_discrete — reco neutron x distribution (shows bar-pitch peaks)
"""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def fig_A1(df: pd.DataFrame, out: Path):
    """Δp_x_rot histogram: all events (truth proton + truth neutron) vs NEBULA-hit subset."""
    fig, ax = plt.subplots(figsize=(7, 4.5))
    # truth Δp_x
    dpx_truth = (df.truth_rot_pxp - df.truth_rot_pxn).values
    nh = df.n_reco_neutrons > 0
    dpx_nh = (df.nn_rot_pxp[nh] - df.reco_rot_pxn[nh]).values
    bins = np.linspace(-30, 30, 121)
    ax.hist(dpx_truth, bins=bins, density=True, histtype="step", color="gray", label=f"truth ({len(dpx_truth)})")
    ax.hist(dpx_nh,    bins=bins, density=True, histtype="step", color="C0",   label=f"NEBULA-hit full reco ({len(dpx_nh)})")
    ax.set_xlabel(r"$\Delta p_x^\mathrm{rot}$ [MeV/c]")
    ax.set_ylabel("density")
    ax.set_title("Fig A1: Δp_x overview")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)


def fig_A2(df: pd.DataFrame, out: Path):
    """Δp_x split by hit_mult_n: 1 (single-bar) vs ≥2 (multi-bar cluster)."""
    nh = (df.n_reco_neutrons > 0)
    sub = df[nh]
    dpx = (sub.nn_rot_pxp - sub.reco_rot_pxn).values
    mult = sub.hit_mult_n.values
    fig, ax = plt.subplots(figsize=(7, 4.5))
    bins = np.linspace(-30, 30, 121)
    for m_mask, label, color in [
        (mult == 1, "single bar (mult=1)", "C1"),
        (mult >= 2, "multi-bar (mult≥2)",   "C2"),
    ]:
        ax.hist(dpx[m_mask], bins=bins, density=True, histtype="step", color=color,
                label=f"{label}  N={int(m_mask.sum())}")
    ax.axvline(0, color="k", lw=0.5, ls=":")
    ax.set_xlabel(r"$\Delta p_x^\mathrm{rot}$ [MeV/c]")
    ax.set_ylabel("density")
    ax.set_title("Fig A2: Δp_x split by NEBULA cluster multiplicity")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)


def fig_A3(df: pd.DataFrame, joined_extras: pd.DataFrame | None, out: Path):
    """NEBULA hit x distribution: shows 12-cm bar discrete peaks.

    joined_extras is unused for now (the joined parquet does not currently store
    the per-event reco neutron x in mm). We approximate using direction × L:
        x_reco_mm ~ L_NEBULA * reco_pxn / reco_pzn (mm),  L_NEBULA = 7250 mm.
    """
    nh = df.n_reco_neutrons > 0
    sub = df[nh]
    # reco_pxn / reco_pzn comes from the un-rotated reco neutron direction × momentum.
    # Note: this estimate uses lab-frame reco neutron momentum, which the extraction
    # macro stores as reco_pxn/reco_pyn/reco_pzn (un-rotated).
    L = 7250.0  # mm, NEBULA front-face distance from target along z (approximate)
    mask = sub.reco_pzn > 0
    x_reco = L * sub.reco_pxn[mask] / sub.reco_pzn[mask]
    fig, ax = plt.subplots(figsize=(8, 4.5))
    bins = np.linspace(-2100, 2100, 421)  # 10 mm bins; bar pitch is 122 mm
    ax.hist(x_reco, bins=bins, histtype="step", color="C3")
    ax.set_xlabel(r"reco neutron $x$ at NEBULA face [mm]  (≈ $L \cdot p_x/p_z$)")
    ax.set_ylabel("counts")
    ax.set_title("Fig A3: NEBULA reco-x discrete bands (60-bar × ~122 mm pitch)")
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--joined", required=True, help="joined.parquet from 03_analyze_r_breakup.py (must contain hit_mult_n)")
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    df = pd.read_parquet(args.joined)
    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)
    required = {"truth_rot_pxp", "truth_rot_pxn", "nn_rot_pxp", "reco_rot_pxn",
                "n_reco_neutrons", "hit_mult_n", "reco_pxn", "reco_pzn"}
    missing = required - set(df.columns)
    if missing:
        raise SystemExit(f"joined parquet missing columns: {missing}")

    fig_A1(df, out / "fig_A1_dpx_overview.pdf")
    fig_A2(df, out / "fig_A2_dpx_split_mult.pdf")
    fig_A3(df, None, out / "fig_A3_nebula_x_discrete.pdf")
    print(f"[partA] figures written under {out}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run the script**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
mkdir -p build/test_output/nebula_reco_quality_20260513/figs
python scripts/analysis/nebula_reco_quality/partA_two_peak.py \
    --joined build/test_output/nn_breakup_phys_20260503/summary/ypol_v2/joined.parquet \
    --out-dir build/test_output/nebula_reco_quality_20260513/figs
```
Expected: 3 PDF files emitted under `build/test_output/nebula_reco_quality_20260513/figs/`.

- [ ] **Step 3: Visual sanity check**

Open figs in viewer (xdg-open or eog):
```bash
xdg-open build/test_output/nebula_reco_quality_20260513/figs/fig_A2_dpx_split_mult.pdf
```
Expected: in `fig_A2`, the mult=1 curve shows two prominent peaks at ~±5 MeV/c; the mult≥2 curve is a single peak centered near 0. In `fig_A3`, x histogram shows ~60 evenly spaced bumps at ~122 mm pitch.

- [ ] **Step 4: Commit**

```bash
git add scripts/analysis/nebula_reco_quality/partA_two_peak.py
git commit -m "nebula_reco_quality: Part A — Δp_x split by hit_mult_n figures"
```

---

### Task A5: Draft Part A tex sections

**Files:**
- Create: `docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.tex` (Part A only this task; Part B later)
- Create: `docs/reports/reconstruction/nebula/figures/` (symlink or copy figures)

- [ ] **Step 1: Create report directory and copy figures**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
mkdir -p docs/reports/reconstruction/nebula/figures
cp build/test_output/nebula_reco_quality_20260513/figs/fig_A*.pdf \
    docs/reports/reconstruction/nebula/figures/
```

- [ ] **Step 2: Write Part A tex skeleton**

Create `docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.tex` with:
- Preamble matching `nn_breakup_phys_20260503_zh.tex` (ctexart, geometry, graphicx, listings, etc.)
- Title, author, date
- §1 Introduction (2 paragraphs: motivation, scope)
- §2 Part A: Two-peak source trace
  - §2.1 Phenomenon recap (cite `nn_breakup_phys_20260503_zh.tex` §5.4, embed `fig_A1`)
  - §2.2 Data path trace
    - L1 Geant4 SimData stores continuous `fPrePosition` — cite `libs/smg4lib/src/data/src/NEBULASimDataConverter_TArtNEBULAPla.cc:80-86`
    - L2 Converter overwrites X with bar center — cite same file `:156`
    - L3 Reconstructor's 5 mm smearing leaves discreteness — cite `libs/analysis/src/NEBULAReconstructor.cc:295-305`
    - L4 Multi-bar cluster pulls boundary events to truth — cite `NEBULAReconstructor.cc:206-218`
    - L5 Downstream `n_reco_neutrons==1` filter removes multi-bar group — cite `scripts/analysis/nn_breakup_phys/03_analyze_r_breakup.py`
  - §2.3 Key split figure: embed `fig_A2_dpx_split_mult.pdf`, explain
  - §2.4 NEBULA reco-x distribution: embed `fig_A3_nebula_x_discrete.pdf`, explain
  - §2.5 Conclusion + 3 future-fix options (no code change for now)

Use this snippet for the LaTeX header (copy from the nn report):
```latex
\documentclass[a4paper,11pt]{ctexart}
\usepackage[margin=2.5cm]{geometry}
\usepackage{graphicx}
\usepackage{booktabs}
\usepackage{listings}
\usepackage{xcolor}
\usepackage{hyperref}
\hypersetup{colorlinks=true, linkcolor=blue, urlcolor=blue}
\lstset{basicstyle=\ttfamily\small, breaklines=true, columns=fullflexible}

\title{NEBULA 重建质量调查：\\ Part A — $\Delta p_x$ 双峰源码追溯, Part B — $(p_x,p_y)$ 效率扫描}
\author{Baiting Tian}
\date{2026-05-13}

\begin{document}
\maketitle
\tableofcontents

\section{引言}
...
```

- [ ] **Step 3: Compile to verify no LaTeX errors**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/docs/reports/reconstruction/nebula
latexmk -xelatex -interaction=nonstopmode nebula_reco_quality_20260513_zh.tex 2>&1 | tail -20
```
Expected: `Output written on nebula_reco_quality_20260513_zh.pdf`.

- [ ] **Step 4: Commit Part A tex + figures**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.tex \
        docs/reports/reconstruction/nebula/figures/fig_A1_dpx_overview.pdf \
        docs/reports/reconstruction/nebula/figures/fig_A2_dpx_split_mult.pdf \
        docs/reports/reconstruction/nebula/figures/fig_A3_nebula_x_discrete.pdf
git commit -m "report: nebula reco quality Part A — Δp_x two-peak source trace"
```

---

## Phase 2 — Part B Prep (Local)

### Task B1: Generate tree-gun input root file

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py`

- [ ] **Step 1: Inspect existing tree-gun input schema**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
source setup.sh
root -l -b -q -e "TFile *f=TFile::Open(\"data/simulation/ips_wrl/sn124_3deg_1.15T_offset0/inputs/neutron_px_scan_pz627_offset0.root\"); ((TTree*)f->Get(\"tree\"))->Print();" 2>&1 | tail -30
```
Record the expected branch list (likely `px, py, pz, pdgid, x, y, z` or similar). Use exactly the same branch names in the new file.

- [ ] **Step 2: Implement the generator**

Create `scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py`:
```python
"""Generate a tree-gun input root for the (px,py) × pz grid scan.

Schema mirrors the existing inputs/neutron_px_scan_pz627_offset0.root so that
sim_deuteron's /action/gun/Type Tree consumer can read it without changes.

Grid:
  px ∈ [-200, +200] step 20 MeV/c  (21 points)
  py ∈ [-100, +100] step 10 MeV/c  (21 points)
  pz ∈ {550, 600, 627, 650, 700} MeV/c  (5 sub-points)
  per (px,py,pz): N events (default 2000)
  total: 21 × 21 × 5 × 2000 = 4 410 000 events
"""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import ROOT


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--px-min", type=float, default=-200.0)
    ap.add_argument("--px-max", type=float, default=+200.0)
    ap.add_argument("--px-step", type=float, default=20.0)
    ap.add_argument("--py-min", type=float, default=-100.0)
    ap.add_argument("--py-max", type=float, default=+100.0)
    ap.add_argument("--py-step", type=float, default=10.0)
    ap.add_argument("--pz-list", type=str, default="550,600,627,650,700")
    ap.add_argument("--n-per-bin", type=int, default=2000)
    args = ap.parse_args()

    px_grid = np.arange(args.px_min, args.px_max + 0.5*args.px_step, args.px_step)
    py_grid = np.arange(args.py_min, args.py_max + 0.5*args.py_step, args.py_step)
    pz_list = [float(s) for s in args.pz_list.split(",")]

    f = ROOT.TFile(args.out, "RECREATE")
    t = ROOT.TTree("tree", "neutron grid for px-py acceptance scan")

    import array
    a_px = array.array("d", [0.0])
    a_py = array.array("d", [0.0])
    a_pz = array.array("d", [0.0])
    a_x  = array.array("d", [0.0])
    a_y  = array.array("d", [0.0])
    a_z  = array.array("d", [0.0])
    a_pdg = array.array("i", [2112])
    t.Branch("px",  a_px,  "px/D")
    t.Branch("py",  a_py,  "py/D")
    t.Branch("pz",  a_pz,  "pz/D")
    t.Branch("x",   a_x,   "x/D")
    t.Branch("y",   a_y,   "y/D")
    t.Branch("z",   a_z,   "z/D")
    t.Branch("pdg", a_pdg, "pdg/I")

    n_total = 0
    rng = np.random.default_rng(20260513)
    for px in px_grid:
        for py in py_grid:
            for pz in pz_list:
                # tiny jitter to avoid identical events
                jx = rng.normal(0.0, 0.1, size=args.n_per_bin)
                jy = rng.normal(0.0, 0.1, size=args.n_per_bin)
                jz = rng.normal(0.0, 0.1, size=args.n_per_bin)
                for k in range(args.n_per_bin):
                    a_px[0] = float(px) + jx[k]
                    a_py[0] = float(py) + jy[k]
                    a_pz[0] = float(pz) + jz[k]
                    a_x[0]  = 0.0
                    a_y[0]  = 0.0
                    a_z[0]  = 0.0
                    a_pdg[0] = 2112
                    t.Fill()
                    n_total += 1

    t.Write()
    f.Close()
    print(f"[gen_pxpy_grid_input] wrote {n_total} events to {args.out}")


if __name__ == "__main__":
    main()
```

⚠️ If the schema from Step 1 differs (e.g., uses different branch names like `mom_x` instead of `px`), edit the branch names in this script to match. The Tree gun must read branches with the exact names the simulator expects.

- [ ] **Step 3: Run on a tiny grid to smoke-test**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
source setup.sh
mkdir -p build/test_output/nebula_reco_quality_20260513/inputs
python scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py \
    --out build/test_output/nebula_reco_quality_20260513/inputs/probe_3x3.root \
    --px-min -20 --px-max 20 --px-step 20 \
    --py-min -10 --py-max 10 --py-step 10 \
    --pz-list 627 --n-per-bin 10
```
Expected: writes ~90 events (3 × 3 × 1 × 10).

- [ ] **Step 4: Verify the file via ROOT**

```bash
root -l -b -q -e "TFile *f=TFile::Open(\"build/test_output/nebula_reco_quality_20260513/inputs/probe_3x3.root\"); ((TTree*)f->Get(\"tree\"))->Print(); cout<<\"entries=\"<<((TTree*)f->Get(\"tree\"))->GetEntries()<<endl;" 2>&1 | tail -15
```
Expected: tree has 90 entries with expected branch list.

- [ ] **Step 5: Commit**

```bash
git add scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py
git commit -m "nebula_reco_quality: tree-gun grid input generator (4.41M neutron events)"
```

---

### Task B2: Precompute ε_geom over the grid

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/compute_geom_acceptance.py`

- [ ] **Step 1: Implement grid ε_geom**

Create the script:
```python
"""Precompute ε_geom over the (px,py,pz) grid using the pure-Python ray-cast.

Output: parquet with columns (px, py, pz, hit) — 'hit' is 0/1 from ray_hits_bar
across all 120 NEBULA Neut bars. Aggregation to ε_geom(px,py) is done in
analyze_efficiency.py.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import pandas as pd

from geom_acceptance import load_nebula_bars, ray_hits_bar


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--px-min", type=float, default=-200.0)
    ap.add_argument("--px-max", type=float, default=+200.0)
    ap.add_argument("--px-step", type=float, default=20.0)
    ap.add_argument("--py-min", type=float, default=-100.0)
    ap.add_argument("--py-max", type=float, default=+100.0)
    ap.add_argument("--py-step", type=float, default=10.0)
    ap.add_argument("--pz-list", type=str, default="550,600,627,650,700")
    args = ap.parse_args()

    repo = Path(__file__).resolve().parents[3]
    bars = load_nebula_bars(
        repo / "configs/simulation/geometry/NEBULA_Detectors_Dayone.csv",
        repo / "configs/simulation/geometry/NEBULA_Dayone.csv",
        neut_only=True,
    )

    px_grid = np.arange(args.px_min, args.px_max + 0.5*args.px_step, args.px_step)
    py_grid = np.arange(args.py_min, args.py_max + 0.5*args.py_step, args.py_step)
    pz_list = [float(s) for s in args.pz_list.split(",")]

    rows = []
    for px in px_grid:
        for py in py_grid:
            for pz in pz_list:
                hit = 0
                for b in bars:
                    if ray_hits_bar(0.0, 0.0, 0.0, float(px), float(py), pz, b):
                        hit = 1
                        break
                rows.append((float(px), float(py), pz, hit))

    df = pd.DataFrame(rows, columns=["px", "py", "pz", "hit"])
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    df.to_parquet(args.out, index=False)
    print(f"[compute_geom_acceptance] wrote {len(df)} grid points to {args.out}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run on full grid**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/scripts/analysis/nebula_reco_quality
python compute_geom_acceptance.py \
    --out ../../../build/test_output/nebula_reco_quality_20260513/eps_geom_grid.parquet
```
Expected: writes 21 × 21 × 5 = 2205 rows in well under a minute.

- [ ] **Step 3: Sanity check**

```bash
python -c "
import pandas as pd
df = pd.read_parquet('/home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/eps_geom_grid.parquet')
print(df.groupby(['px','py']).hit.mean().unstack().round(2).head(5))
print('center (px=0, py=0) hit fraction across pz =', df[(df.px==0)&(df.py==0)].hit.mean())
"
```
Expected: center bin has hit=1.0; corners (|px|=200, |py|=100) should have lower acceptance.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add scripts/analysis/nebula_reco_quality/compute_geom_acceptance.py
git commit -m "nebula_reco_quality: ε_geom precompute over (px,py,pz) grid"
```

---

### Task B3: Generate per-job macros

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/gen_scan_macros.py`

- [ ] **Step 1: Implement macro generator (one job = one chunk of the tree)**

Create the script:
```python
"""Generate per-job sim_deuteron mac files for the (px,py) scan.

Strategy: a single input tree feeds many jobs, each consuming a contiguous slice
of N_chunk events. sim_deuteron's tree-gun reads from the file; we drive parallel
jobs by writing N_jobs distinct mac files that each set OverWrite, RunName, and
beamOn N_chunk. They share the same input tree (read-only).

WARNING: requires that the tree-gun consumes from the file in order. If different
jobs would overwrite the same offsets, we need separate input root files per job.
This task starts with the simple approach (single tree, sequential consumption);
Task B4 first runs a single job end-to-end and validates the contract before
launching parallel jobs.
"""
from __future__ import annotations
import argparse
from pathlib import Path


MAC_TEMPLATE = """\
/control/execute {smsim}/configs/simulation/geometry/3deg_1.15T.mac
/samurai/geometry/Target/SetTarget false
/samurai/geometry/Update
/action/file/OverWrite y
/action/file/SaveDirectory {save_dir}/
/action/file/RunName {run_name}
/action/gun/Type Tree
/action/gun/tree/InputFileName {input_root}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn {n_events}
exit
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--smsim", default="/home/tian/workspace/dpol/smsimulator5.5")
    ap.add_argument("--input-root", required=True, help="path on labenpg (e.g. /home/tian/workspace/.../inputs/grid.root)")
    ap.add_argument("--save-dir", required=True, help="path on labenpg")
    ap.add_argument("--out-dir", required=True, help="local dir for generated mac files")
    ap.add_argument("--n-jobs", type=int, default=1, help="initial smoke run: 1 job")
    ap.add_argument("--n-events-per-job", type=int, default=4410000, help="total events split across jobs")
    args = ap.parse_args()

    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)
    for j in range(args.n_jobs):
        run_name = f"pxpy_scan_job{j:03d}"
        mac = MAC_TEMPLATE.format(
            smsim=args.smsim,
            save_dir=args.save_dir,
            run_name=run_name,
            input_root=args.input_root,
            n_events=args.n_events_per_job,
        )
        (out / f"{run_name}.mac").write_text(mac)
    print(f"[gen_scan_macros] wrote {args.n_jobs} mac files under {out}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Generate a single smoke-test macro**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python scripts/analysis/nebula_reco_quality/gen_scan_macros.py \
    --input-root /home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/inputs/probe_3x3.root \
    --save-dir /home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/g4output \
    --out-dir build/test_output/nebula_reco_quality_20260513/macros \
    --n-jobs 1 --n-events-per-job 90
cat build/test_output/nebula_reco_quality_20260513/macros/pxpy_scan_job000.mac
```
Expected: one mac file printed with the substituted paths.

- [ ] **Step 3: Commit**

```bash
git add scripts/analysis/nebula_reco_quality/gen_scan_macros.py
git commit -m "nebula_reco_quality: per-job mac generator for px-py scan"
```

---

## Phase 3 — Part B on labenpg-hk

### Task B4: Smoke-test single job on labenpg-hk

- [ ] **Step 1: rsync inputs + macros + scripts to labenpg-hk**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
REMOTE_BASE=/home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513
ssh labenpg-hk "mkdir -p ${REMOTE_BASE}/inputs ${REMOTE_BASE}/macros ${REMOTE_BASE}/g4output"
rsync -avh build/test_output/nebula_reco_quality_20260513/inputs/probe_3x3.root \
    labenpg-hk:${REMOTE_BASE}/inputs/
rsync -avh build/test_output/nebula_reco_quality_20260513/macros/pxpy_scan_job000.mac \
    labenpg-hk:${REMOTE_BASE}/macros/
```

- [ ] **Step 2: Run single job on labenpg-hk**

```bash
ssh labenpg-hk 'bash -c "
cd ~/workspace/dpol/smsimulator5.5
export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
eval \"\$(\$MAMBA_EXE shell hook --shell bash --root-prefix \$MAMBA_ROOT_PREFIX)\"
micromamba activate anaroot-env
source setup.sh >/dev/null
sim_deuteron build/test_output/nebula_reco_quality_20260513/macros/pxpy_scan_job000.mac \
    > build/test_output/nebula_reco_quality_20260513/g4output/job000.log 2>&1
ls -lh build/test_output/nebula_reco_quality_20260513/g4output/
tail -5 build/test_output/nebula_reco_quality_20260513/g4output/job000.log
"'
```
Expected: produces `pxpy_scan_job000_0000.root` (or similar), tail log shows clean exit.

- [ ] **Step 3: Verify the output file has expected event count and NEBULA hits**

```bash
ssh labenpg-hk 'bash -c "
cd ~/workspace/dpol/smsimulator5.5
export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
eval \"\$(\$MAMBA_EXE shell hook --shell bash --root-prefix \$MAMBA_ROOT_PREFIX)\"
micromamba activate anaroot-env
root -l -b -q -e \"
TFile *f=TFile::Open(\\\"build/test_output/nebula_reco_quality_20260513/g4output/pxpy_scan_job000_0000.root\\\");
TTree *t=(TTree*)f->Get(\\\"tree\\\");
cout<<\\\"entries=\\\"<<t->GetEntries()<<endl;
\" 2>&1 | tail -5
"'
```
Expected: 90 entries (matching the probe input).

- [ ] **Step 4: Decide on parallel strategy**

Inspect the smoke output. Two cases:
- **Case A — tree gun consumes the whole file each run, ignoring beamOn count > entries.** Then parallel jobs that share one input root will all simulate the SAME events. Must split the input root into N pieces, one per job.
- **Case B — tree gun reads exactly beamOn events starting from entry 0.** Same problem.

Conclusion: we need N input roots, one per job. Update plan: regenerate inputs as N shards in Task B5.

(No commit in this task — pure smoke validation.)

---

### Task B5: Shard inputs into N parallel jobs

**Files:**
- Modify: `scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py` (add `--n-shards`)
- Modify: `scripts/analysis/nebula_reco_quality/gen_scan_macros.py` (one mac per shard)

- [ ] **Step 1: Add `--n-shards` to gen_pxpy_grid_input.py**

In `gen_pxpy_grid_input.py`, after parsing args, change the writing loop to round-robin into N shards. Replace the file open + tree write block with:
```python
    shards = []
    for s in range(args.n_shards):
        out_path = Path(args.out).parent / f"{Path(args.out).stem}_shard{s:02d}.root"
        f = ROOT.TFile(str(out_path), "RECREATE")
        t = ROOT.TTree("tree", "neutron grid shard")
        # ... declare branches identically (see existing code) ...
        shards.append((f, t, out_path))

    # ... in the inner Fill loop, dispatch to shards[(global_evt_idx) % args.n_shards] ...
```
Add `--n-shards` argument with default 32 (to match ~96 cores ÷ 3 oversubscription).

(See full updated source in Step 2.)

- [ ] **Step 2: Write the full updated `gen_pxpy_grid_input.py`**

Rewrite the file with sharding support:
```python
"""Generate sharded tree-gun input roots for the (px,py) scan.

Each shard is an independent root file; sim_deuteron jobs run in parallel,
each consuming one shard. Events are round-robin distributed so every shard
covers the full (px,py,pz) grid uniformly.
"""
from __future__ import annotations
import argparse, array
from pathlib import Path
import numpy as np
import ROOT


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-prefix", required=True, help="e.g. /path/grid -> emits grid_shard00.root ...")
    ap.add_argument("--n-shards", type=int, default=32)
    ap.add_argument("--px-min", type=float, default=-200.0)
    ap.add_argument("--px-max", type=float, default=+200.0)
    ap.add_argument("--px-step", type=float, default=20.0)
    ap.add_argument("--py-min", type=float, default=-100.0)
    ap.add_argument("--py-max", type=float, default=+100.0)
    ap.add_argument("--py-step", type=float, default=10.0)
    ap.add_argument("--pz-list", type=str, default="550,600,627,650,700")
    ap.add_argument("--n-per-bin", type=int, default=2000)
    args = ap.parse_args()

    px_grid = np.arange(args.px_min, args.px_max + 0.5*args.px_step, args.px_step)
    py_grid = np.arange(args.py_min, args.py_max + 0.5*args.py_step, args.py_step)
    pz_list = [float(s) for s in args.pz_list.split(",")]

    Path(args.out_prefix).parent.mkdir(parents=True, exist_ok=True)
    files = []
    trees = []
    arrs = []
    for s in range(args.n_shards):
        path = f"{args.out_prefix}_shard{s:02d}.root"
        f = ROOT.TFile(path, "RECREATE")
        t = ROOT.TTree("tree", "neutron grid shard")
        a_px = array.array("d", [0.0]); a_py = array.array("d", [0.0]); a_pz = array.array("d", [0.0])
        a_x = array.array("d", [0.0]);  a_y  = array.array("d", [0.0]); a_z  = array.array("d", [0.0])
        a_pdg = array.array("i", [2112])
        t.Branch("px", a_px, "px/D"); t.Branch("py", a_py, "py/D"); t.Branch("pz", a_pz, "pz/D")
        t.Branch("x",  a_x,  "x/D");  t.Branch("y",  a_y,  "y/D");  t.Branch("z",  a_z,  "z/D")
        t.Branch("pdg", a_pdg, "pdg/I")
        files.append(f); trees.append(t)
        arrs.append((a_px, a_py, a_pz, a_x, a_y, a_z, a_pdg))

    rng = np.random.default_rng(20260513)
    n_total = 0
    for px in px_grid:
        for py in py_grid:
            for pz in pz_list:
                jx = rng.normal(0.0, 0.1, size=args.n_per_bin)
                jy = rng.normal(0.0, 0.1, size=args.n_per_bin)
                jz = rng.normal(0.0, 0.1, size=args.n_per_bin)
                for k in range(args.n_per_bin):
                    s = n_total % args.n_shards
                    a_px, a_py, a_pz, a_x, a_y, a_z, a_pdg = arrs[s]
                    a_px[0] = float(px) + jx[k]
                    a_py[0] = float(py) + jy[k]
                    a_pz[0] = float(pz) + jz[k]
                    a_x[0] = 0.0; a_y[0] = 0.0; a_z[0] = 0.0
                    a_pdg[0] = 2112
                    trees[s].Fill()
                    n_total += 1

    for s in range(args.n_shards):
        trees[s].Write()
        files[s].Close()
    print(f"[gen_pxpy_grid_input] wrote {n_total} events across {args.n_shards} shards under {args.out_prefix}_shard*.root")


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Update `gen_scan_macros.py` to emit one mac per shard**

Modify the loop to take a list of input roots:
```python
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--smsim", default="/home/tian/workspace/dpol/smsimulator5.5")
    ap.add_argument("--input-prefix", required=True, help="e.g. /path/grid (matches gen_pxpy_grid_input out-prefix)")
    ap.add_argument("--n-shards", type=int, default=32)
    ap.add_argument("--save-dir", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--n-events-per-shard", type=int, default=137813)  # ceil(4410000/32)
    args = ap.parse_args()

    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)
    for j in range(args.n_shards):
        input_root = f"{args.input_prefix}_shard{j:02d}.root"
        run_name = f"pxpy_scan_shard{j:02d}"
        mac = MAC_TEMPLATE.format(
            smsim=args.smsim,
            save_dir=args.save_dir,
            run_name=run_name,
            input_root=input_root,
            n_events=args.n_events_per_shard,
        )
        (out / f"{run_name}.mac").write_text(mac)
    print(f"[gen_scan_macros] wrote {args.n_shards} mac files under {out}")
```

- [ ] **Step 4: Smoke test sharding locally**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py \
    --out-prefix build/test_output/nebula_reco_quality_20260513/inputs/probe \
    --n-shards 4 \
    --px-min -20 --px-max 20 --px-step 20 \
    --py-min -10 --py-max 10 --py-step 10 \
    --pz-list 627 --n-per-bin 10
ls build/test_output/nebula_reco_quality_20260513/inputs/probe_shard*.root
```
Expected: 4 shard files, each with ~22-23 events.

- [ ] **Step 5: Commit**

```bash
git add scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py \
        scripts/analysis/nebula_reco_quality/gen_scan_macros.py
git commit -m "nebula_reco_quality: shard tree-gun inputs + per-shard macros for parallel scan"
```

---

### Task B6: Build parallel scan driver script

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/run_scan_on_labenpg.sh`

- [ ] **Step 1: Write the driver**

Create:
```bash
#!/bin/bash
# Drive the (px,py) scan on labenpg-hk.
# Usage:  bash run_scan_on_labenpg.sh [N_PARALLEL]
# Assumes inputs/, macros/, g4output/ already rsynced to labenpg.
set -euo pipefail

REMOTE_BASE=${REMOTE_BASE:-/home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513}
N_PARALLEL=${1:-24}   # 96 cores ÷ 4 (sim_deuteron uses ~3-4 threads internally)
HOST=${HOST:-labenpg-hk}

ssh "${HOST}" "bash -lc '
set -euo pipefail
cd ~/workspace/dpol/smsimulator5.5
export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
eval \"\$(\$MAMBA_EXE shell hook --shell bash --root-prefix \$MAMBA_ROOT_PREFIX)\"
micromamba activate anaroot-env
source setup.sh >/dev/null

cd ${REMOTE_BASE}
mkdir -p g4output logs
ls macros/pxpy_scan_shard*.mac | xargs -n1 -P${N_PARALLEL} -I{} bash -c \"
    mac=\\\"\\$0\\\"
    name=\\\"\\$(basename \\\"\\$mac\\\" .mac)\\\"
    echo \\\"[\\${name}] start \\$(date -Iseconds)\\\"
    sim_deuteron \\\"\\$mac\\\" > logs/\\${name}.log 2>&1
    echo \\\"[\\${name}] done \\$(date -Iseconds)\\\"
\" {}
echo \"[run_scan] all shards complete\"
ls -lh g4output/*.root | head
'"
```

- [ ] **Step 2: Smoke run on labenpg with 4 small shards from Task B5**

First regenerate macros for the probe shards:
```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python scripts/analysis/nebula_reco_quality/gen_scan_macros.py \
    --input-prefix /home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/inputs/probe \
    --n-shards 4 \
    --save-dir /home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/g4output \
    --out-dir build/test_output/nebula_reco_quality_20260513/macros \
    --n-events-per-shard 25
rsync -avh build/test_output/nebula_reco_quality_20260513/inputs/probe_shard*.root \
    labenpg-hk:/home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/inputs/
rsync -avh build/test_output/nebula_reco_quality_20260513/macros/pxpy_scan_shard*.mac \
    labenpg-hk:/home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/macros/
bash scripts/analysis/nebula_reco_quality/run_scan_on_labenpg.sh 4 2>&1 | tail -30
```
Expected: 4 shard outputs created on labenpg under `g4output/`, total wallclock under 5 minutes for ~100 events.

- [ ] **Step 3: Commit driver**

```bash
chmod +x scripts/analysis/nebula_reco_quality/run_scan_on_labenpg.sh
git add scripts/analysis/nebula_reco_quality/run_scan_on_labenpg.sh
git commit -m "nebula_reco_quality: parallel scan driver for labenpg-hk"
```

---

### Task B7: Generate full-size inputs + macros, launch full scan

- [ ] **Step 1: Generate full inputs locally** (one-time cost; ~few minutes, ~few hundred MB on disk)

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
source setup.sh
rm -f build/test_output/nebula_reco_quality_20260513/inputs/grid_shard*.root  # clean stale shards
python scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py \
    --out-prefix build/test_output/nebula_reco_quality_20260513/inputs/grid \
    --n-shards 32 \
    --n-per-bin 2000
du -sh build/test_output/nebula_reco_quality_20260513/inputs/
```

- [ ] **Step 2: Generate full macros**

```bash
python scripts/analysis/nebula_reco_quality/gen_scan_macros.py \
    --input-prefix /home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/inputs/grid \
    --n-shards 32 \
    --save-dir /home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/g4output \
    --out-dir build/test_output/nebula_reco_quality_20260513/macros \
    --n-events-per-shard 137813
ls build/test_output/nebula_reco_quality_20260513/macros/pxpy_scan_shard*.mac | wc -l
```
Expected: 32.

- [ ] **Step 3: rsync inputs + macros to labenpg-hk**

```bash
ssh labenpg-hk "mkdir -p /home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/{inputs,macros,g4output,logs}"
rsync -avh --info=progress2 build/test_output/nebula_reco_quality_20260513/inputs/grid_shard*.root \
    labenpg-hk:/home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/inputs/
rsync -avh build/test_output/nebula_reco_quality_20260513/macros/pxpy_scan_shard*.mac \
    labenpg-hk:/home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/macros/
```

- [ ] **Step 4: Launch full scan (long-running, run in background)**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
nohup bash scripts/analysis/nebula_reco_quality/run_scan_on_labenpg.sh 24 \
    > build/test_output/nebula_reco_quality_20260513/run_scan_local.log 2>&1 &
echo "scan PID=$!  - tail the log to monitor"
tail -f build/test_output/nebula_reco_quality_20260513/run_scan_local.log
```
Expected: progress lines for each shard start/done. Estimated wallclock 1-2 hours for 4.41M events at ~100-200 events/s on 24 parallel jobs (sim_deuteron ~50-100 ms/event).

- [ ] **Step 5: Sanity check after completion**

```bash
ssh labenpg-hk "ls -lh /home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/g4output/pxpy_scan_shard*.root | wc -l; du -sh /home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/g4output/"
```
Expected: 32 root files (one per shard), total size on the order of 10-30 GB.

(No commit — outputs are gitignored.)

---

### Task B8: Run NEBULA reconstruction on labenpg (post-sim step)

The reconstruction binary `reconstruct_target_momentum` walks the same g4output and produces a separate `_reco.root`. For our scan we only need `n_reco_neutrons` from the reco side; we can either:
- (a) Run `reconstruct_target_momentum` against each `pxpy_scan_shard*.root`, or
- (b) Run NEBULAReconstructor directly on the g4output via a minimal ROOT macro.

Approach (a) reuses production infrastructure but pulls in NN proton reco (unneeded). Approach (b) is leaner. We pick (b) — a small dedicated macro.

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/run_nebula_reco.C`

- [ ] **Step 1: Write the standalone NEBULA-only reco macro**

Create `scripts/analysis/nebula_reco_quality/run_nebula_reco.C`:
```cpp
// Standalone NEBULA-only reconstruction over the px-py scan g4output.
// Reads NEBULAPla from input, runs NEBULAReconstructor, writes a tiny
// summary tree (truth_px/py/pz, n_nebula_hits, nebula_sumE, n_reco_neutrons,
// first_neutron_hit_mult).
//
// Usage:
//   root -l -b -q 'run_nebula_reco.C("input.root","summary.root")'

#include "NEBULAReconstructor.hh"
#include "GeometryManager.hh"
#include "RecoEvent.hh"
#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include <iostream>

void run_nebula_reco(const char* input_root, const char* out_root) {
    TFile *fin = TFile::Open(input_root);
    if (!fin || fin->IsZombie()) { std::cerr<<"open failed: "<<input_root<<std::endl; return; }
    TTree *t_in = (TTree*)fin->Get("tree");
    if (!t_in) { std::cerr<<"no 'tree' in "<<input_root<<std::endl; return; }

    // Truth gun branches (px,py,pz in MeV/c)
    double truth_px=0, truth_py=0, truth_pz=0;
    t_in->SetBranchAddress("BeamSimDataMomentumX", &truth_px);  // <-- adjust to actual branch name
    t_in->SetBranchAddress("BeamSimDataMomentumY", &truth_py);
    t_in->SetBranchAddress("BeamSimDataMomentumZ", &truth_pz);

    TClonesArray *nebPla = nullptr;
    t_in->SetBranchAddress("NEBULAPla", &nebPla);

    GeometryManager gm;
    NEBULAReconstructor nebreco(gm);
    nebreco.SetTargetPosition(TVector3(0,0,0));

    TFile *fout = TFile::Open(out_root, "RECREATE");
    TTree *t_out = new TTree("summary","NEBULA reco summary");
    int n_hits=0; double sumE=0; int n_reco_n=0; int first_mult=0;
    t_out->Branch("truth_px", &truth_px, "truth_px/D");
    t_out->Branch("truth_py", &truth_py, "truth_py/D");
    t_out->Branch("truth_pz", &truth_pz, "truth_pz/D");
    t_out->Branch("n_nebula_hits", &n_hits, "n_nebula_hits/I");
    t_out->Branch("nebula_sumE",   &sumE,   "nebula_sumE/D");
    t_out->Branch("n_reco_neutrons", &n_reco_n, "n_reco_neutrons/I");
    t_out->Branch("first_hit_mult",  &first_mult, "first_hit_mult/I");

    Long64_t n = t_in->GetEntries();
    for (Long64_t i=0; i<n; ++i) {
        t_in->GetEntry(i);
        n_hits = nebPla ? nebPla->GetEntriesFast() : 0;
        sumE = 0.0;
        if (nebPla) {
            for (int k=0; k<n_hits; ++k) {
                TObject *obj = nebPla->At(k);
                if (!obj) continue;
                TDataMember *qm = obj->IsA()->GetDataMember("fQAveCal");
                if (qm) sumE += *(double*)((char*)obj + qm->GetOffset());
            }
        }
        auto neutrons = nebreco.ReconstructNeutrons(nebPla);
        n_reco_n = (int)neutrons.size();
        first_mult = (n_reco_n > 0) ? neutrons[0].hitMultiplicity : 0;
        t_out->Fill();
    }
    t_out->Write();
    fout->Close();
    fin->Close();
    std::cout<<"[run_nebula_reco] DONE: "<<n<<" events -> "<<out_root<<std::endl;
}
```
⚠️ The truth branch names (`BeamSimDataMomentumX/Y/Z`) are placeholders — Step 2 below verifies them and the script gets corrected if needed.

- [ ] **Step 2: Inspect actual branch names**

```bash
ssh labenpg-hk 'bash -c "
cd ~/workspace/dpol/smsimulator5.5
export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
eval \"\$(\$MAMBA_EXE shell hook --shell bash --root-prefix \$MAMBA_ROOT_PREFIX)\"
micromamba activate anaroot-env
root -l -b -q -e \"
TFile *f=TFile::Open(\\\"build/test_output/nebula_reco_quality_20260513/g4output/pxpy_scan_shard00_0000.root\\\");
TTree *t=(TTree*)f->Get(\\\"tree\\\");
t->Print();
\" 2>&1 | grep -E '^[*]|[Bb]ranch|[Mm]omentum|[Bb]eam'
"' | head -30
```
Record the actual beam momentum branch names. Common patterns: `BeamSimData.fMomentumX`, `BeamMom.X()`, `gun_px`, etc.

- [ ] **Step 3: Fix the macro if branch names differ**

Update `run_nebula_reco.C:SetBranchAddress` lines to match. Re-test by running the macro on one shard.

- [ ] **Step 4: Smoke run reconstruction on one shard**

```bash
ssh labenpg-hk 'bash -c "
cd ~/workspace/dpol/smsimulator5.5
export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
eval \"\$(\$MAMBA_EXE shell hook --shell bash --root-prefix \$MAMBA_ROOT_PREFIX)\"
micromamba activate anaroot-env
source setup.sh
cd build/test_output/nebula_reco_quality_20260513
root -l -b -q ~/workspace/dpol/smsimulator5.5/scripts/analysis/nebula_reco_quality/run_nebula_reco.C\\\"\\\\\"g4output/pxpy_scan_shard00_0000.root\\\\\",\\\\\"summary/pxpy_scan_shard00.root\\\\\"\\\" 2>&1 | tail -10
"'
```
Expected: `DONE: N events -> summary/pxpy_scan_shard00.root`.

- [ ] **Step 5: Drive reconstruction over all 32 shards on labenpg**

Add a second loop to `run_scan_on_labenpg.sh` (or write a sibling `run_reco_on_labenpg.sh`). Reuse the same `xargs -P` pattern. After both loops, all `summary/pxpy_scan_shard*.root` exist.

```bash
ssh labenpg-hk "bash -lc '
set -euo pipefail
cd ~/workspace/dpol/smsimulator5.5
export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
eval \"\$(\$MAMBA_EXE shell hook --shell bash --root-prefix \$MAMBA_ROOT_PREFIX)\"
micromamba activate anaroot-env
source setup.sh >/dev/null
cd build/test_output/nebula_reco_quality_20260513
mkdir -p summary
ls g4output/pxpy_scan_shard*.root | xargs -n1 -P24 -I{} bash -c \"
    g4out=\\\"\\$0\\\"
    name=\\\"\\$(basename \\\"\\$g4out\\\" .root)\\\"
    root -l -b -q ~/workspace/dpol/smsimulator5.5/scripts/analysis/nebula_reco_quality/run_nebula_reco.C\\\\\\\"\\\\\\\"\\$g4out\\\\\\\",\\\\\\\"summary/\\${name}.root\\\\\\\"\\\\\\\" > logs/reco_\\${name}.log 2>&1
\" {}
echo all shards reconstructed
'"
```

- [ ] **Step 6: Commit the reco macro**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add scripts/analysis/nebula_reco_quality/run_nebula_reco.C
git commit -m "nebula_reco_quality: standalone NEBULA-only reco macro + driver loop"
```

---

### Task B9: Summarize and rsync back

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/summarize_scan_outputs.py`

- [ ] **Step 1: Implement summarizer (concatenates shard summary roots → one parquet)**

Create:
```python
"""Concatenate per-shard summary roots into one parquet for local analysis."""
from __future__ import annotations
import argparse, glob
from pathlib import Path
import pandas as pd
import uproot  # part of anaroot-env


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--summary-glob", required=True, help="e.g. /path/to/summary/pxpy_scan_shard*.root")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    files = sorted(glob.glob(args.summary_glob))
    if not files:
        raise SystemExit(f"no files match {args.summary_glob}")
    dfs = []
    for f in files:
        with uproot.open(f) as fh:
            t = fh["summary"]
            dfs.append(t.arrays(library="pd"))
    df = pd.concat(dfs, ignore_index=True)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    df.to_parquet(args.out, index=False)
    print(f"[summarize] {len(df)} rows from {len(files)} shards -> {args.out}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run on labenpg**

```bash
ssh labenpg-hk 'bash -c "
cd ~/workspace/dpol/smsimulator5.5
export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
eval \"\$(\$MAMBA_EXE shell hook --shell bash --root-prefix \$MAMBA_ROOT_PREFIX)\"
micromamba activate anaroot-env
python scripts/analysis/nebula_reco_quality/summarize_scan_outputs.py \
    --summary-glob \"build/test_output/nebula_reco_quality_20260513/summary/pxpy_scan_shard*.root\" \
    --out build/test_output/nebula_reco_quality_20260513/scan_summary.parquet
"'
```
Expected: ~4.4M rows, parquet under 100 MB.

- [ ] **Step 3: rsync parquet back to local**

```bash
rsync -avh labenpg-hk:/home/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/scan_summary.parquet \
    build/test_output/nebula_reco_quality_20260513/
```

- [ ] **Step 4: Commit summarizer**

```bash
git add scripts/analysis/nebula_reco_quality/summarize_scan_outputs.py
git commit -m "nebula_reco_quality: summarize per-shard reco roots to one parquet"
```

---

## Phase 4 — Part B Analysis (Local)

### Task B10: Compute three efficiencies + conditional efficiencies

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/analyze_efficiency.py`

- [ ] **Step 1: Implement**

```python
"""Compute ε_geom, ε_det, ε_reco and conditional efficiencies per (px,py) bin."""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import pandas as pd


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scan-summary", required=True)
    ap.add_argument("--geom-grid", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--energy-threshold", type=float, default=1.0, help="MeV (sumE) for ε_det")
    args = ap.parse_args()

    scan = pd.read_parquet(args.scan_summary)
    geom = pd.read_parquet(args.geom_grid)

    # Bin by integer px / py (jitter is sigma=0.1 MeV/c so floor or nearest works).
    scan["px_bin"] = scan.truth_px.round(0).astype(int)
    scan["py_bin"] = scan.truth_py.round(0).astype(int)

    # ε_det / ε_reco: marginalize pz (pool all 5 pz sub-points).
    g = scan.groupby(["px_bin", "py_bin"])
    rows = []
    for (px, py), sub in g:
        n = len(sub)
        n_det = int((sub.nebula_sumE > args.energy_threshold).sum())
        n_reco = int((sub.n_reco_neutrons > 0).sum())
        rows.append((px, py, n, n_det, n_reco, n_det/n, n_reco/n))
    df_det = pd.DataFrame(rows, columns=["px","py","n","n_det","n_reco","eps_det","eps_reco"])

    # ε_geom from precomputed grid; marginalize pz uniformly.
    geom_avg = geom.groupby(["px","py"]).hit.mean().reset_index().rename(columns={"hit": "eps_geom"})

    df = df_det.merge(geom_avg, on=["px","py"], how="left")
    df["eps_det_given_geom"] = np.where(df.eps_geom > 0, df.eps_det / df.eps_geom, np.nan)
    df["eps_reco_given_det"] = np.where(df.eps_det > 0, df.eps_reco / df.eps_det, np.nan)

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    df.to_parquet(args.out, index=False)
    print(f"[analyze_efficiency] {len(df)} (px,py) bins -> {args.out}")
    print(df.describe()[["eps_geom","eps_det","eps_reco","eps_det_given_geom","eps_reco_given_det"]])


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python scripts/analysis/nebula_reco_quality/analyze_efficiency.py \
    --scan-summary build/test_output/nebula_reco_quality_20260513/scan_summary.parquet \
    --geom-grid    build/test_output/nebula_reco_quality_20260513/eps_geom_grid.parquet \
    --out          build/test_output/nebula_reco_quality_20260513/efficiency_grid.parquet
```
Expected: 441 (px,py) rows; printed describe() shows nontrivial values, eps_geom ranges from 0 to 1.

- [ ] **Step 3: Commit**

```bash
git add scripts/analysis/nebula_reco_quality/analyze_efficiency.py
git commit -m "nebula_reco_quality: three-layer + conditional efficiency analyzer"
```

---

### Task B11: Generate Part B figures

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/make_efficiency_figures.py`

- [ ] **Step 1: Implement**

```python
"""Part B figures: 2D heatmaps + 1D slices for ε_geom, ε_det, ε_reco, and conditionals."""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def heatmap(ax, df, col, title):
    pivot = df.pivot(index="py", columns="px", values=col).sort_index()
    im = ax.imshow(pivot.values, origin="lower", aspect="auto",
                   extent=[pivot.columns.min(), pivot.columns.max(),
                           pivot.index.min(), pivot.index.max()],
                   vmin=0, vmax=1, cmap="viridis")
    ax.set_xlabel("px [MeV/c]"); ax.set_ylabel("py [MeV/c]"); ax.set_title(title)
    return im


def slice_py0(df, cols, labels, out: Path):
    sub = df[df.py == 0].sort_values("px")
    fig, ax = plt.subplots(figsize=(7,4.5))
    for c, l in zip(cols, labels):
        ax.plot(sub.px, sub[c], marker="o", label=l)
    ax.set_xlabel("px [MeV/c]  (py=0)"); ax.set_ylabel("efficiency")
    ax.set_ylim(-0.02, 1.05); ax.grid(alpha=0.3); ax.legend()
    ax.set_title("Efficiency vs px (py=0 slice)")
    fig.tight_layout(); fig.savefig(out, dpi=120); plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--efficiency", required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    df = pd.read_parquet(args.efficiency)
    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)

    # 2x2 heatmap of eps_geom, eps_det, eps_reco, eps_reco_given_det
    fig, axes = plt.subplots(2,2, figsize=(11,8))
    for ax, col, title in zip(axes.flat,
                              ["eps_geom","eps_det","eps_reco","eps_reco_given_det"],
                              ["ε_geom","ε_det","ε_reco","ε_reco | det"]):
        im = heatmap(ax, df, col, title)
    fig.colorbar(im, ax=axes.ravel().tolist(), shrink=0.7)
    fig.savefig(out / "fig_B1_efficiency_2D.pdf", dpi=120); plt.close(fig)

    slice_py0(df, ["eps_geom","eps_det","eps_reco"],
              ["ε_geom","ε_det","ε_reco"],
              out / "fig_B2_efficiency_slice_py0.pdf")
    slice_py0(df, ["eps_det_given_geom","eps_reco_given_det"],
              ["ε_det | geom","ε_reco | det"],
              out / "fig_B3_efficiency_conditional_py0.pdf")
    print(f"[make_efficiency_figures] 3 figures -> {out}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python scripts/analysis/nebula_reco_quality/make_efficiency_figures.py \
    --efficiency build/test_output/nebula_reco_quality_20260513/efficiency_grid.parquet \
    --out-dir    build/test_output/nebula_reco_quality_20260513/figs
ls build/test_output/nebula_reco_quality_20260513/figs/fig_B*.pdf
```
Expected: fig_B1, fig_B2, fig_B3.

- [ ] **Step 3: Visual sanity check**

Open `fig_B2_efficiency_slice_py0.pdf`. Expected:
- ε_geom (highest) drops to 0 outside ~±165 MeV/c (the geometric horizon)
- ε_det is below ε_geom by the neutron detection efficiency (~30-50% inside acceptance)
- ε_reco ≤ ε_det

- [ ] **Step 4: Commit**

```bash
git add scripts/analysis/nebula_reco_quality/make_efficiency_figures.py
git commit -m "nebula_reco_quality: Part B efficiency 2D + slice figures"
```

---

### Task B12: Apply ε(px,py) to breakup truth, estimate R-cut acceptance asymmetry

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/apply_to_breakup.py`

- [ ] **Step 1: Implement**

```python
"""Weight ε(px,py) by the breakup truth distribution from joined.parquet,
emit a table summarizing how the px asymmetry of the R-cut subset is shaped by
NEBULA acceptance."""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import pandas as pd


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--joined", required=True, help="joined.parquet from nn_breakup_phys")
    ap.add_argument("--efficiency", required=True, help="efficiency_grid.parquet")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    df = pd.read_parquet(args.joined)
    eff = pd.read_parquet(args.efficiency)

    df = df[(df.truth_has_proton == 1) & (df.truth_has_neutron == 1)].copy()
    # bin truth neutron px/py to the same 20/10 MeV/c grid
    df["pxn_bin"] = (df.truth_pxn / 20).round(0).astype(int) * 20
    df["pyn_bin"] = (df.truth_pyn / 10).round(0).astype(int) * 10

    joined = df.merge(eff[["px","py","eps_geom","eps_det","eps_reco"]],
                      left_on=["pxn_bin","pyn_bin"], right_on=["px","py"], how="left")
    joined["eps_geom"] = joined.eps_geom.fillna(0)
    joined["eps_det"]  = joined.eps_det.fillna(0)
    joined["eps_reco"] = joined.eps_reco.fillna(0)

    # asymmetry: ratio of weighted counts in pxn>0 vs pxn<0
    rows = []
    for cut_col in ("loose","mid","tight"):
        sub = joined[joined[cut_col]]
        n_pos = (sub.truth_pxn > 0).sum()
        n_neg = (sub.truth_pxn < 0).sum()
        # weighted with each efficiency
        w_pos_g = sub.eps_geom[sub.truth_pxn > 0].sum()
        w_neg_g = sub.eps_geom[sub.truth_pxn < 0].sum()
        w_pos_r = sub.eps_reco[sub.truth_pxn > 0].sum()
        w_neg_r = sub.eps_reco[sub.truth_pxn < 0].sum()
        rows.append({
            "cut": cut_col,
            "N_truth_pos": int(n_pos),
            "N_truth_neg": int(n_neg),
            "R_truth": n_pos / max(n_neg, 1),
            "R_weighted_geom":  w_pos_g / max(w_neg_g, 1e-9),
            "R_weighted_reco":  w_pos_r / max(w_neg_r, 1e-9),
        })
    summary = pd.DataFrame(rows)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    summary.to_csv(args.out, index=False)
    print(summary.to_string(index=False))


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python scripts/analysis/nebula_reco_quality/apply_to_breakup.py \
    --joined     build/test_output/nn_breakup_phys_20260503/summary/ypol_v2/joined.parquet \
    --efficiency build/test_output/nebula_reco_quality_20260513/efficiency_grid.parquet \
    --out        build/test_output/nebula_reco_quality_20260513/R_acceptance_table.csv
```
Expected: a small CSV showing R_truth vs R_weighted_geom vs R_weighted_reco for the three cuts.

- [ ] **Step 3: Commit**

```bash
git add scripts/analysis/nebula_reco_quality/apply_to_breakup.py
git commit -m "nebula_reco_quality: weight ε(px,py) by breakup truth, estimate R asymmetry"
```

---

### Task B13: Draft Part B tex sections

**Files:**
- Modify: `docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.tex`
- Copy figures: B1, B2, B3 into `docs/reports/reconstruction/nebula/figures/`

- [ ] **Step 1: Copy figures and R table**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
cp build/test_output/nebula_reco_quality_20260513/figs/fig_B*.pdf \
   docs/reports/reconstruction/nebula/figures/
cp build/test_output/nebula_reco_quality_20260513/R_acceptance_table.csv \
   docs/reports/reconstruction/nebula/figures/
```

- [ ] **Step 2: Append Part B sections to the tex**

Append to `docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.tex` (before `\end{document}`):
- §3 Part B: (px,py) 效率扫描
  - §3.1 Gun 设定、网格、几何配置 (cite `configs/simulation/geometry/3deg_1.15T.mac`, `scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py`)
  - §3.2 ε_geom 二维 (embed `fig_B1`) + 解释 NEBULA 几何边界 (±1962 mm X, ±900 mm Y)
  - §3.3 ε_det / ε_reco 同图 + 切片 (embed `fig_B2`)
  - §3.4 条件效率 — 瓶颈识别 (embed `fig_B3`)
  - §3.5 加权回 breakup truth, R cut 接受不对称表 (include R_acceptance_table.csv as latex tabular)
  - §3.6 结论：哪一段 px 范围 NEBULA 接受度足以信任 R cut
- §4 综合结论与未来工作

- [ ] **Step 3: Compile**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/docs/reports/reconstruction/nebula
latexmk -xelatex -interaction=nonstopmode nebula_reco_quality_20260513_zh.tex 2>&1 | tail -20
```
Expected: clean PDF compile.

- [ ] **Step 4: Commit final report**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.tex \
        docs/reports/reconstruction/nebula/nebula_reco_quality_20260513_zh.pdf \
        docs/reports/reconstruction/nebula/figures/fig_B*.pdf \
        docs/reports/reconstruction/nebula/figures/R_acceptance_table.csv
git commit -m "report: nebula reco quality Part B — (px,py) efficiency scan + R asymmetry"
```

---

## Phase 5 — Wrap-up

### Task C1: Write README for the new analysis directory

**Files:**
- Create: `scripts/analysis/nebula_reco_quality/README.md`

- [ ] **Step 1: Write README**

Document the workflow (one-screen overview):
- Goal: produce nebula_reco_quality report
- Order of scripts (00...15 numbering) and what each does
- How to re-run the full pipeline from scratch
- Output locations
- Tests: `pytest test_geom_acceptance.py`

- [ ] **Step 2: Commit**

```bash
git add scripts/analysis/nebula_reco_quality/README.md
git commit -m "nebula_reco_quality: README documenting end-to-end workflow"
```

### Task C2: Update MEMORY index

- [ ] **Step 1: Add a project memory pointer**

After the report is in, add a project memory:
- `~/.claude/projects/-home-tian-workspace-dpol-smsimulator5-5/memory/project_nebula_reco_quality_findings.md`
- One pointer line in MEMORY.md

Key facts to capture: two-peak source (X = bar center, discrete), ε_geom horizon (~±165 MeV/c at pz=627), conditional efficiency bottleneck (whichever layer the analysis identified).

- [ ] **Step 2: Confirm clean repo**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git status --short
git log --oneline main..HEAD | head -20
```

---

## Self-Review Notes

- Spec §3 (Part A) → Tasks A1–A5 (extraction column, re-run, figures, tex).
- Spec §4 (Part B) → Tasks A3 (geom_acceptance shared), B1–B13 (inputs, scan, reco, summarize, analyze, plot, weight, tex).
- Spec §4.6 (Veto sanity check) — NOT explicitly implemented as a separate task; the standalone reco macro records only Neut. If Veto sanity is required, add it as an optional extra step in Task B8 by also counting `nebPla` entries whose detector type is Veto.
- Method/property names checked: `RecoNeutron::hitMultiplicity` (RecoEvent.hh:44), `re->neutrons[0]`, `pos` schema in tree gun (`px,py,pz,x,y,z,pdg`) — names are consistent across tasks.
- Placeholder scan: no TBD/TODO in code blocks. The two "to verify" items (Task B1 Step 1 schema, Task B8 Step 2 branch names) are deliberate empirical validations, not placeholders — they have concrete commands.
- The destructive `rm` in Task A2 Step 1 is justified inline (extraction macro appends; old rows would corrupt).
