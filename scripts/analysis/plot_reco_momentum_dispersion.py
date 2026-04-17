#!/usr/bin/env python3
"""Per-truth-point reconstructed momentum dispersion histograms.

One figure per special truth point: 1x3 subplots of reco_px, reco_py, reco_pz
histograms overlaid with truth (black) and median (red) + Fisher sigma band (gray).
Each subplot annotates robust G4 half-width, bias, and the ratio vs Fisher sigma.

Usage:
  micromamba run -n anaroot-env python3 scripts/analysis/plot_reco_momentum_dispersion.py
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
    "build/test_output/ensemble_n50/g4_fluctuation/ensemble_pdc_reco_merged.csv"
)
DEFAULT_OUT_DIR = "docs/reports/reconstruction"
DEFAULT_TRUTH_PZ = 627.0


def robust_halfwidth(a: np.ndarray) -> float:
    return 0.5 * (np.percentile(a, 84) - np.percentile(a, 16))


def plot_one_point(df: pd.DataFrame, px: float, py: float, pz: float, out_path: Path) -> dict:
    g = df[(df["truth_px"] == px) & (df["truth_py"] == py)]
    n = len(g)

    truths = {"px": px, "py": py, "pz": pz}
    fig, axes = plt.subplots(1, 3, figsize=(12.5, 4.0))
    result = {"truth_px": px, "truth_py": py, "N": n}

    for ax, comp in zip(axes, ("px", "py", "pz")):
        reco = g[f"reco_{comp}"].values
        truth = truths[comp]
        fisher_col = f"fisher_{comp}_sigma"
        fisher = g[fisher_col].median() if fisher_col in g.columns else float("nan")

        # robust metrics (resistant to LM failure outliers)
        median_reco = np.median(reco)
        hw = robust_halfwidth(reco)
        bias = median_reco - truth

        # histogram axis: center on truth; show most of the distribution but clip far outliers
        lo = np.percentile(reco, 2)
        hi = np.percentile(reco, 98)
        span = max(hi - lo, 6.0 * hw, 30.0)
        center = 0.5 * (lo + hi)
        ax_lo = min(truth - 0.5 * span, center - 0.5 * span)
        ax_hi = max(truth + 0.5 * span, center + 0.5 * span)

        bins = np.linspace(ax_lo, ax_hi, 22)
        ax.hist(reco, bins=bins, alpha=0.65, color="steelblue", edgecolor="white", linewidth=0.5)
        ax.axvline(truth, color="black", linestyle="-", linewidth=1.4, label=f"truth={truth:g}")
        ax.axvline(median_reco, color="red", linestyle="--", linewidth=1.2, label=f"median={median_reco:.1f}")
        if not np.isnan(fisher):
            ax.axvspan(median_reco - fisher, median_reco + fisher,
                       color="red", alpha=0.08, label=f"Fisher σ={fisher:.1f}")

        ratio = hw / fisher if fisher > 0 else float("nan")
        ax.set_title(
            f"reco {comp}  N={n}\n"
            f"G4 σ={hw:.1f}  bias={bias:+.1f}  (G4/Fisher={ratio:.1f})",
            fontsize=9,
        )
        ax.set_xlabel(f"reco {comp} (MeV/c)")
        ax.set_ylabel("count" if comp == "px" else "")
        ax.tick_params(labelsize=8)
        ax.legend(loc="upper right", fontsize=7, framealpha=0.85)
        ax.grid(axis="y", alpha=0.3, linewidth=0.3)

        result[f"reco_{comp}_median"] = float(median_reco)
        result[f"reco_{comp}_g4_halfwidth"] = float(hw)
        result[f"reco_{comp}_bias"] = float(bias)
        result[f"reco_{comp}_fisher_sigma"] = float(fisher)
        result[f"reco_{comp}_ratio"] = float(ratio) if not np.isnan(ratio) else float("nan")

    fig.suptitle(
        f"Reco momentum dispersion: identical Geant4 injection  "
        f"(truth $p_x$, $p_y$, $p_z$) = ({px:+.0f}, {py:+.0f}, {pz:.0f}) MeV/c,  N={n}",
        fontsize=11,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--merged", default=DEFAULT_MERGED)
    parser.add_argument("--out-dir", default=DEFAULT_OUT_DIR)
    parser.add_argument("--truth-pz", type=float, default=DEFAULT_TRUTH_PZ)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    os.chdir(repo_root)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    df = pd.read_csv(args.merged)

    rows = []
    for px, py in SPECIAL_POINTS:
        tag = f"px{int(px):+d}_py{int(py):+d}".replace("+", "p").replace("-", "m")
        out_path = out_dir / f"reco_momentum_dispersion_{tag}.png"
        info = plot_one_point(df, px, py, args.truth_pz, out_path)
        rows.append(info)
        print(f"[plot] wrote {out_path}  N={info['N']}  "
              f"px σ={info['reco_px_g4_halfwidth']:.1f}/{info['reco_px_fisher_sigma']:.1f}  "
              f"py σ={info['reco_py_g4_halfwidth']:.1f}/{info['reco_py_fisher_sigma']:.1f}  "
              f"pz σ={info['reco_pz_g4_halfwidth']:.1f}/{info['reco_pz_fisher_sigma']:.1f}")

    summary_csv = Path("build/test_output/ensemble_n50/g4_fluctuation") / "reco_momentum_dispersion_summary.csv"
    pd.DataFrame(rows).to_csv(summary_csv, index=False)
    print(f"[plot] wrote {summary_csv}")

    # 3x5 overview grids, one per component.
    for comp in ("px", "py", "pz"):
        grid_path = out_dir / f"reco_momentum_dispersion_grid_{comp}.png"
        plot_component_grid(df, comp, args.truth_pz, grid_path)
        print(f"[plot] wrote {grid_path}")


def plot_component_grid(df: pd.DataFrame, comp: str, truth_pz: float, out_path: Path) -> None:
    py_vals = sorted({py for _, py in SPECIAL_POINTS})[::-1]
    px_vals = sorted({px for px, _ in SPECIAL_POINTS})
    nrow, ncol = len(py_vals), len(px_vals)
    fig, axes = plt.subplots(nrow, ncol, figsize=(2.6 * ncol, 2.3 * nrow))
    for ir, py in enumerate(py_vals):
        for ic, px in enumerate(px_vals):
            ax = axes[ir, ic]
            g = df[(df["truth_px"] == px) & (df["truth_py"] == py)]
            n = len(g)
            if n == 0:
                ax.set_visible(False)
                continue
            reco = g[f"reco_{comp}"].values
            truth = {"px": px, "py": py, "pz": truth_pz}[comp]
            fcol = f"fisher_{comp}_sigma"
            fisher = g[fcol].median() if fcol in g.columns else float("nan")
            median_reco = float(np.median(reco))
            hw = robust_halfwidth(reco)
            lo = np.percentile(reco, 2); hi = np.percentile(reco, 98)
            span = max(hi - lo, 6.0 * hw, 30.0)
            center = 0.5 * (lo + hi)
            ax_lo = min(truth - 0.5 * span, center - 0.5 * span)
            ax_hi = max(truth + 0.5 * span, center + 0.5 * span)
            bins = np.linspace(ax_lo, ax_hi, 20)
            ax.hist(reco, bins=bins, alpha=0.65, color="steelblue",
                    edgecolor="white", linewidth=0.4)
            ax.axvline(truth, color="black", linewidth=1.1)
            ax.axvline(median_reco, color="red", linestyle="--", linewidth=1.0)
            if not np.isnan(fisher):
                ax.axvspan(median_reco - fisher, median_reco + fisher,
                           color="red", alpha=0.10)
            ratio = hw / fisher if fisher > 0 else float("nan")
            bias = median_reco - truth
            ax.set_title(f"({px:+.0f},{py:+.0f}) N={n}\n"
                         f"G4σ={hw:.1f} bias={bias:+.1f} r={ratio:.1f}",
                         fontsize=7)
            ax.tick_params(labelsize=6)
            ax.grid(axis="y", alpha=0.3, linewidth=0.3)
            if ir == nrow - 1:
                ax.set_xlabel(f"reco {comp} (MeV/c)", fontsize=7)
            if ic == 0:
                ax.set_ylabel(f"py={py:+.0f}\ncount", fontsize=7)
    fig.suptitle(
        f"Reco {comp} dispersion — full 5×3 truth grid (rows = py, cols = px, MeV/c). "
        "Black = truth, red dashed = G4 median, pink = Fisher σ band.",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    main()
