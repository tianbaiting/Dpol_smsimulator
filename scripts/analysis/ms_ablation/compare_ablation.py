#!/usr/bin/env python3
"""Compare baseline vs no_air dispersion summaries and produce:
  - compare.csv  (stacked rows with delta_ columns on baseline side)
  - figures/dispersion_overlay_<px>_<py>.png  (scatter overlay per truth point)
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd


SIGMA_COLS = [
    "sigma_x_pdc1_mm", "sigma_y_pdc1_mm",
    "sigma_x_pdc2_mm", "sigma_y_pdc2_mm",
    "sigma_theta_x_mrad", "sigma_theta_y_mrad",
]


def build_comparison(
    baseline: pd.DataFrame,
    noair: pd.DataFrame,
) -> pd.DataFrame:
    b = baseline.copy()
    n = noair.copy()
    b["condition"] = "baseline"
    n["condition"] = "no_air"

    key = ["truth_px", "truth_py"]
    n_lookup = n.set_index(key)
    for col in SIGMA_COLS:
        delta_col = f"delta_{col}"
        b[delta_col] = b.apply(
            lambda r: r[col] - n_lookup.loc[(r["truth_px"], r["truth_py"]), col]
            if (r["truth_px"], r["truth_py"]) in n_lookup.index else float("nan"),
            axis=1,
        )
        n[delta_col] = float("nan")

    return pd.concat([b, n], ignore_index=True, sort=False)


def _render_overlays(baseline_hits: pd.DataFrame, noair_hits: pd.DataFrame,
                     figures_dir: Path, truth_points: list[tuple[float, float]]):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figures_dir.mkdir(parents=True, exist_ok=True)
    for tpx, tpy in truth_points:
        fig, axes = plt.subplots(1, 2, figsize=(12, 5))
        for ax, which in zip(axes, ("pdc1", "pdc2")):
            for df, color, label in [(baseline_hits, "tab:blue", "baseline"),
                                     (noair_hits,    "tab:orange", "no_air")]:
                sub = df[(df["truth_px"] == tpx) & (df["truth_py"] == tpy)]
                ax.scatter(sub[f"{which}_x"], sub[f"{which}_y"],
                           c=color, s=18, alpha=0.6, label=f"{label} (N={len(sub)})")
            ax.set_xlabel(f"{which.upper()} x (mm)")
            ax.set_ylabel(f"{which.upper()} y (mm)")
            ax.set_title(f"{which.upper()} hits @ truth=({tpx:.0f},{tpy:.0f})")
            ax.legend(loc="best", fontsize=9)
            ax.grid(alpha=0.3)
        fig.tight_layout()
        out_path = figures_dir / f"dispersion_overlay_tp{int(tpx)}_{int(tpy)}.png"
        fig.savefig(out_path, dpi=120)
        plt.close(fig)
        print(f"[compare] wrote {out_path}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-dir", type=Path, required=True)
    parser.add_argument("--noair-dir", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)

    baseline_summary = pd.read_csv(args.baseline_dir / "dispersion_summary.csv")
    noair_summary    = pd.read_csv(args.noair_dir    / "dispersion_summary.csv")

    compare = build_comparison(baseline_summary, noair_summary)
    compare_path = args.out_dir / "compare.csv"
    compare.to_csv(compare_path, index=False)
    print(f"[compare] wrote {compare_path}", flush=True)
    print(compare.to_string(index=False))

    baseline_hits_path = args.baseline_dir / "pdc_hits.csv"
    noair_hits_path    = args.noair_dir    / "pdc_hits.csv"
    if baseline_hits_path.exists() and noair_hits_path.exists():
        baseline_hits = pd.read_csv(baseline_hits_path)
        noair_hits    = pd.read_csv(noair_hits_path)
        truth_points = list(zip(baseline_summary["truth_px"], baseline_summary["truth_py"]))
        _render_overlays(baseline_hits, noair_hits, args.out_dir / "figures", truth_points)


if __name__ == "__main__":
    sys.exit(main())
