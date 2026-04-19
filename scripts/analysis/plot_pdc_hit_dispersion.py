#!/usr/bin/env python3
"""Per-truth-point PDC1 / PDC2 hit-position scatter plots from the N=50 ensemble.

One figure per special truth point: 1x2 subplots showing PDC1 (x,y) and PDC2 (x,y)
positions across the 50 Geant4 replicas that share identical injection. Each subplot
is annotated with robust p16-p84 half-width in x and y (mm).

Also produces an overview grid with all 5 points stacked.

Usage:
  micromamba run -n anaroot-env python3 scripts/analysis/plot_pdc_hit_dispersion.py
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

SPECIAL_POINTS = [(px, py) for py in (-20.0, 0.0, 20.0)
                            for px in (-100.0, -50.0, 0.0, 50.0, 100.0)]

DEFAULT_MERGED = (
    "build/test_output/ensemble_n50_targetframe/g4_fluctuation/ensemble_pdc_reco_merged.csv"
)
DEFAULT_OUT_DIR = "docs/reports/reconstruction/figures/g4_fluctuation/pdc_hits"


def robust_halfwidth(s: pd.Series) -> float:
    return 0.5 * (s.quantile(0.84) - s.quantile(0.16))


def plot_one_point(df: pd.DataFrame, px: float, py: float, out_path: Path) -> dict:
    g = df[(df["truth_px"] == px) & (df["truth_py"] == py)]
    n = len(g)

    fig, axes = plt.subplots(1, 2, figsize=(10.5, 4.4))

    result = {"truth_px": px, "truth_py": py, "N": n}

    for ax, plane in zip(axes, ("pdc1", "pdc2")):
        xs = g[f"{plane}_x"]
        ys = g[f"{plane}_y"]

        mean_x, mean_y = xs.median(), ys.median()
        hw_x, hw_y = robust_halfwidth(xs), robust_halfwidth(ys)

        ax.scatter(xs, ys, s=18, alpha=0.55, color="steelblue", edgecolor="none",
                   label=f"{n} replicas")
        ax.axvline(mean_x, color="red", alpha=0.35, linewidth=0.8)
        ax.axhline(mean_y, color="red", alpha=0.35, linewidth=0.8)

        pad_x = max(2.0 * hw_x, 10.0)
        pad_y = max(2.0 * hw_y, 10.0)
        ax.set_xlim(mean_x - pad_x, mean_x + pad_x)
        ax.set_ylim(mean_y - pad_y, mean_y + pad_y)

        ax.set_xlabel(f"{plane.upper()} x (mm)")
        ax.set_ylabel(f"{plane.upper()} y (mm)")
        ax.set_title(f"{plane.upper()}: median=({mean_x:+.0f}, {mean_y:+.0f}) mm\n"
                     f"σ_x={hw_x:.1f}  σ_y={hw_y:.1f}  (robust half-width)",
                     fontsize=10)
        ax.grid(alpha=0.25, linewidth=0.4)

        result[f"{plane}_median_x"] = mean_x
        result[f"{plane}_median_y"] = mean_y
        result[f"{plane}_sigma_x"] = hw_x
        result[f"{plane}_sigma_y"] = hw_y

    fig.suptitle(
        f"PDC hit dispersion: identical Geant4 injection  "
        f"(truth $p_x$, $p_y$) = ({px:+.0f}, {py:+.0f}) MeV/c, $p_z$=627,  N={n}",
        fontsize=11,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    return result


def plot_overview(df: pd.DataFrame, out_path: Path) -> None:
    """3x5 grid: rows = truth_py (-20, 0, +20), cols = truth_px (-100..+100).
    Each cell: PDC2 (x,y) scatter (PDC2 is the wider plane, most informative)."""
    py_vals = sorted({py for _, py in SPECIAL_POINTS})[::-1]  # top = +20
    px_vals = sorted({px for px, _ in SPECIAL_POINTS})
    nrow, ncol = len(py_vals), len(px_vals)
    fig, axes = plt.subplots(nrow, ncol, figsize=(2.6 * ncol, 2.4 * nrow))
    for ir, py in enumerate(py_vals):
        for ic, px in enumerate(px_vals):
            ax = axes[ir, ic]
            g = df[(df["truth_px"] == px) & (df["truth_py"] == py)]
            n = len(g)
            xs, ys = g["pdc2_x"], g["pdc2_y"]
            if n == 0:
                ax.set_visible(False)
                continue
            mean_x, mean_y = xs.median(), ys.median()
            hw_x, hw_y = robust_halfwidth(xs), robust_halfwidth(ys)
            ax.scatter(xs, ys, s=8, alpha=0.55, color="steelblue", edgecolor="none")
            pad_x = max(2.0 * hw_x, 10.0)
            pad_y = max(2.0 * hw_y, 10.0)
            ax.set_xlim(mean_x - pad_x, mean_x + pad_x)
            ax.set_ylim(mean_y - pad_y, mean_y + pad_y)
            ax.tick_params(labelsize=6)
            ax.set_title(f"({px:+.0f},{py:+.0f}) N={n}\nσx={hw_x:.1f} σy={hw_y:.1f}",
                         fontsize=7)
            ax.grid(alpha=0.2, linewidth=0.3)
            if ir == nrow - 1:
                ax.set_xlabel("PDC2 x (mm)", fontsize=7)
            if ic == 0:
                ax.set_ylabel(f"py={py:+.0f}\nPDC2 y (mm)", fontsize=7)
    fig.suptitle("PDC2 hit dispersion under identical injection — full 5×3 truth grid (rows = py, cols = px, MeV/c)",
                 fontsize=10)
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--merged", default=DEFAULT_MERGED)
    parser.add_argument("--out-dir", default=DEFAULT_OUT_DIR)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    os.chdir(repo_root)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    df = pd.read_csv(args.merged)

    summary_rows = []
    for px, py in SPECIAL_POINTS:
        tag = f"px{int(px):+d}_py{int(py):+d}".replace("+", "p").replace("-", "m")
        out_path = out_dir / f"{tag}.png"
        info = plot_one_point(df, px, py, out_path)
        summary_rows.append(info)
        print(f"[plot] wrote {out_path}  N={info['N']}  "
              f"PDC1 σ=({info['pdc1_sigma_x']:.1f},{info['pdc1_sigma_y']:.1f})  "
              f"PDC2 σ=({info['pdc2_sigma_x']:.1f},{info['pdc2_sigma_y']:.1f})  mm")

    overview_path = out_dir / "overview.png"
    plot_overview(df, overview_path)
    print(f"[plot] wrote {overview_path}")

    # also dump summary table CSV next to data
    summary_csv = Path(args.merged).parent / "pdc_dispersion_summary.csv"
    pd.DataFrame(summary_rows).to_csv(summary_csv, index=False)
    print(f"[plot] wrote {summary_csv}")


if __name__ == "__main__":
    main()
