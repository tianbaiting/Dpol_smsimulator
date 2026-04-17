# ImQMD Raw Data Analysis Guide

## Overview

This document describes the pipeline for processing ImQMD (Improved Quantum Molecular Dynamics) simulation raw data to extract deuteron breakup polarization observables. The primary observable is the ratio R = N(Px_p > Px_n) / N(Px_p < Px_n), which quantifies the isovector reorientation effect and shows strong sensitivity to the nuclear symmetry energy parameter gamma.

All analysis code is in `scripts/notebooks/input_analysis/`.
Dependencies: Python 3, numpy, matplotlib, plotly.

---

## Data Formats

All data resides under `data/qmdrawdata/qmdrawdata/`. Three directory layouts exist:

| Layout | Path Pattern | Columns | Targets |
|--------|-------------|---------|---------|
| ypol (old) | `y_pol/phi_random/d+{target}E{energy}g{gamma}{pol}/dbreak.dat` | 9 | Pb208, Xe130 |
| zpol | `z_pol/b_discrete/d+{target}E{energy}g{gamma}{pol}/dbreakb{b:02d}.dat` | 7 per b | Pb208, Sn112, Sn124, Xe140 |
| ypol (20260413) | `20260413ypol/d+{target}E{energy}/d+{target}E{energy}g{gamma}{pol}-RP360/dbreak.dat` | 9 | Sn112, Sn124 |

### ypol dbreak.dat format (9 columns, whitespace-separated, 2-line header)

```
ynp d+112Sn   190.00MeV/u b= 0.000fm   1000000events
    No.  pxp(MeV/c)  pyp(MeV/c)  pzp(MeV/c)  pxn(MeV/c)  pyn(MeV/c)  pzn(MeV/c)           b       rpphi
       1      9.7939     -9.2406    629.4912     -3.8008     20.4906    623.4291     12.1437    313.9939
```

### zpol dbreak.dat format (7 columns, one file per impact parameter b)

```
znp d+208Pb   190.00MeV/u b= 7.000fm  10000events
    No.  pxp(MeV/c)  pyp(MeV/c)  pzp(MeV/c)  pxn(MeV/c)  pyn(MeV/c)  pzn(MeV/c)
```

### Notation

From `data/qmdrawdata/qmdrawdata/readme.txt`:

| Code | Meaning |
|------|---------|
| ynp | proton -> neutron along y axis |
| ypn | neutron -> proton along y axis |
| znp | proton -> neutron along z (beam) axis |
| zpn | neutron -> proton along z (beam) axis |

Gamma encoding: g050 = gamma 0.50, g060 = 0.60, g085 = 0.85, g100 = 1.00, etc.

The `-RP360` suffix in the 20260413 layout indicates randomized reaction plane azimuthal angle over 360 degrees.

---

## Code Architecture

All code in `scripts/notebooks/input_analysis/`:

### `core/schema.py` -- Event Data Structure

`Event` frozen dataclass with 13 fields:
- `dataset, target, gamma, pol` -- metadata
- `event_no` -- event index
- `pxp, pyp, pzp, pxn, pyn, pzn` -- proton and neutron momenta (MeV/c)
- `b` (optional) -- impact parameter (fm)
- `rpphi_deg` (optional) -- reaction plane azimuthal angle (degrees)

### `core/paths.py` -- Path Resolution

- `default_data_root()` -- resolves via `SMSIMDIR` env var or repo-relative fallback
- `ypol_phi_random_dir()` -- old ypol layout path resolver
- `zpol_b_discrete_dir()` -- zpol layout path resolver
- `ypol_20260413_dir()` -- new 20260413 ypol layout path resolver

### `core/io.py` -- Data Iterators

- `iter_ypol_phi_random(root, target, energy, gamma, pols)` -- yields `Event` from old ypol data
- `iter_ypol_20260413(root, target, energy, gamma, pols)` -- yields `Event` from new ypol data (same parsing, different directory resolver)
- `iter_zpol_b_discrete(root, target, energy, gamma, pols, b_min, b_max, bmax_for_event_filter)` -- yields `Event` from zpol data (loops over impact parameter files, applies event filtering: `eventNo < 10000 * b / bmax`)

All iterators skip the 2-line header and parse whitespace-separated columns.

### `core/features.py` -- Derived Quantities

`compute_derived(event)` returns a `Derived` dataclass:

- `sum_px, sum_py, sum_pz` -- total (p+n) momentum
- `phi = atan2(sum_py, sum_px)` -- azimuthal angle of total transverse momentum
- Rotation by `-phi` into reaction plane frame: `rot_pxp, rot_pyp, rot_pzp, rot_pxn, rot_pyn, rot_pzn`
- `sum_xy2 = sum_px^2 + sum_py^2` -- total transverse momentum squared

**Reaction plane definition**: The x-axis is along the total transverse momentum direction (positive = sum momentum direction). The y-axis is perpendicular to the reaction plane. Rotation angle is `phi = atan2(sum_py, sum_px)`, and all momenta are rotated by `-phi`.

### `core/cuts.py` -- Event Selection

**ypol cuts:**

| Cut | Conditions | Physics Motivation |
|-----|-----------|-------------------|
| loose | `abs(pyp - pyn) < 150` AND `sum_xy2 > 2500` | Remove unphysical breakup on polarization axis (y); require minimum transverse momentum |
| mid | loose AND `(rot_pxp + rot_pxn) < 200` | Constrain total in-plane momentum |
| tight | mid AND `(pi - abs(phi)) < 0.2` | Select near-backscatter events |

**zpol cuts:**

| Cut | Conditions | Physics Motivation |
|-----|-----------|-------------------|
| loose | `(pzp + pzn) > 1150` AND `abs(pzp - pzn) < 150` | Forward-going events; remove unphysical breakup on beam axis |
| mid | loose AND `(pxp + pxn) < 200` AND `sum_xy2 > 2500` | Constrain transverse momentum |
| tight | mid AND `(pi - abs(phi)) < 0.5` | Select near-backscatter |

### `core/accumulators.py` -- Data Collection

- `ReservoirSampler` -- Algorithm R for capped random sampling of 3D momentum points
- `Accumulator` -- collects raw momenta, rotated momenta, `pxn - pxp` differences, and 3D reservoir samples for proton/neutron/delta

---

## Pipeline Scripts

### `run_analysis.py` -- Main Analysis Pipeline

Processes data through: **read -> derive -> cut -> accumulate -> plot**

```bash
# Old ypol layout
python3 run_analysis.py --dataset ypol --target Pb208 --gammas 050 060 070 080

# New 20260413 ypol layout
python3 run_analysis.py --dataset ypol --data-layout 20260413ypol \
  --target Sn112 --gammas 050 055 060 065 070 075 080 085

# zpol
python3 run_analysis.py --dataset zpol --target Pb208 --gammas 050 060 070 080 \
  --b-min 5 --b-max 10
```

**Key arguments:**

| Argument | Description | Default |
|----------|-------------|---------|
| `--dataset` | ypol, zpol, or both | both |
| `--data-layout` | default or 20260413ypol | default |
| `--target` | Target nucleus | Pb208 |
| `--energy` | Beam energy | 190 |
| `--gammas` | List of gamma values | 050 060 070 080 |
| `--cuts` | Selection levels | loose mid tight |
| `--max-3d` | Max 3D sample points | 20000 |
| `--b-min/--b-max` | Impact parameter range (zpol) | 5 / 10 |
| `--output-dir` | Output directory | output |

**Outputs per cut/gamma:**
- `momentum_3d_proton_neutron.html` -- interactive 3D scatter (plotly)
- `momentum_3d_delta.html` -- 3D momentum difference
- `pxn_vs_pxp.png` -- 2D correlation in reaction plane
- `pxn_minus_pxp.png` -- difference distribution histogram
- `pxp_vs_pxn_raw.png` -- raw 2D correlation
- `phi_p_vs_phi_n_raw.png` -- azimuthal angle correlation

**Summary outputs per target:**
- `summary.json` -- event counts per cut/gamma
- `ratio_summary.json` -- R ratios per cut/gamma
- `ratio_vs_gamma.png` -- key result plot: R vs gamma for all cuts

### `analyze_r_vs_b_and_px.py` -- R Observable Analysis

Detailed analysis of R as a function of kinematic variables.

```bash
# Old layout
python3 analyze_r_vs_b_and_px.py --dataset ypol --target Pb208 --gammas 050 060 070 080

# New layout
python3 analyze_r_vs_b_and_px.py --dataset ypol --data-layout 20260413ypol \
  --target Sn112 --gammas 050 055 060 065 070 075 080 085
```

**Outputs per dataset/target:**
- `R_vs_b.csv` / `R_vs_b.png` -- R ratio vs impact parameter
- `R_vs_sum_px.csv` / `R_vs_sum_px.png` -- R vs total in-plane momentum
- `R_vs_pxp.csv` / `R_vs_pxp.png` -- R vs proton px
- `R_vs_pxn.csv` / `R_vs_pxn.png` -- R vs neutron px
- `sensitivity_summary.json` -- top bins by gamma spread
- `summary.json` -- global R values per gamma

---

## Physics Background

### Unphysical Breakup in ImQMD

ImQMD transport calculations produce artificial deuteron breakups with unrealistically large relative momentum on the polarization axis. The deuteron binding energy is 2.2 MeV, corresponding to a maximum physical relative momentum of ~45 MeV/c (Newtonian: p = sqrt(2 * mu * E_b)). Events with |p_pol^p - p_pol^n| > 150 MeV/c are considered unphysical and removed by the loose cut.

- For ypol: polarization axis is y, cut is `|pyp - pyn| < 150`
- For zpol: polarization axis is z, cut is `|pzp - pzn| < 150`

### The R Observable

The primary observable quantifies the isovector reorientation effect:

```
R = N(Px_p > Px_n) / N(Px_p < Px_n)
```

where Px_p and Px_n are the proton and neutron x-momenta in the reaction plane frame (after rotation by -phi). In a non-uniform nuclear mean field, protons and neutrons experience different impulses in the reaction plane, leading to an asymmetry in their momentum ordering.

R shows strong sensitivity to the nuclear symmetry energy parameter gamma, making it a key observable for constraining the equation of state of asymmetric nuclear matter.

### Reaction Plane Definition

The reaction plane is defined by:
- Beam axis (z)
- Total transverse momentum of breakup fragments

Coordinate system:
- x-axis: along total transverse momentum (in-plane, positive direction)
- y-axis: perpendicular to reaction plane
- Rotation angle: `phi = atan2(sum_py, sum_px)`, rotate by `-phi`

---

## Output Directory Structure

```
output/
+-- ypol/
|   +-- {target}/
|       +-- summary.json
|       +-- ratio_summary.json
|       +-- ratio_vs_gamma.png
|       +-- loose/
|       |   +-- gamma_{gamma}/
|       |       +-- momentum_3d_proton_neutron.html
|       |       +-- momentum_3d_delta.html
|       |       +-- pxn_vs_pxp.png
|       |       +-- pxn_minus_pxp.png
|       |       +-- pxp_vs_pxn_raw.png
|       |       +-- phi_p_vs_phi_n_raw.png
|       +-- mid/
|       |   +-- gamma_{gamma}/...
|       +-- tight/
|           +-- gamma_{gamma}/...
+-- zpol/
|   +-- {target}/...
+-- r_observable/
    +-- ypol/
    |   +-- {target}/
    |       +-- R_vs_b.csv, R_vs_b.png
    |       +-- R_vs_sum_px.csv, R_vs_sum_px.png
    |       +-- R_vs_pxp.csv, R_vs_pxp.png
    |       +-- R_vs_pxn.csv, R_vs_pxn.png
    |       +-- sensitivity_summary.json
    +-- zpol/...
    +-- summary.json
```
