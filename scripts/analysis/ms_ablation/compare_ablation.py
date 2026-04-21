#!/usr/bin/env python3
"""Compare three MS ablation conditions and produce:
  - compare.csv  (stacked rows with three delta columns on the all_air and
    beamline_vacuum rows: upstream_air, downstream_air, total_air)
  - figures/dispersion_overlay_<px>_<py>.png  (3-color scatter per truth point)
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

CONDITION_ORDER = ["all_air", "beamline_vacuum", "all_vacuum"]

# (delta_suffix, minuend_condition, subtrahend_condition)
# "upstream_air"   = all_air - beamline_vacuum (fake upstream air contribution)
# "downstream_air" = beamline_vacuum - all_vacuum (real downstream air contribution)
# "total_air"      = all_air - all_vacuum (stage A's original number)
DELTAS = [
    ("upstream_air",   "all_air",         "beamline_vacuum"),
    ("downstream_air", "beamline_vacuum", "all_vacuum"),
    ("total_air",      "all_air",         "all_vacuum"),
]


def _delta(minuend_row, subtrahend_lookup, col):
    key = (minuend_row["truth_px"], minuend_row["truth_py"])
    if key not in subtrahend_lookup.index:
        return float("nan")
    return minuend_row[col] - subtrahend_lookup.loc[key, col]


def build_comparison(
    all_air: pd.DataFrame,
    beamline_vacuum: pd.DataFrame,
    all_vacuum: pd.DataFrame,
) -> pd.DataFrame:
    frames = {
        "all_air":         all_air.copy(),
        "beamline_vacuum": beamline_vacuum.copy(),
        "all_vacuum":      all_vacuum.copy(),
    }
    for name, df in frames.items():
        df["condition"] = name

    key = ["truth_px", "truth_py"]
    lookups = {name: df.set_index(key) for name, df in frames.items()}

    for suffix, minuend, subtrahend in DELTAS:
        sub_lookup = lookups[subtrahend]
        for col in SIGMA_COLS:
            delta_col = f"delta_{col}_{suffix}"
            for name, df in frames.items():
                if name == minuend:
                    df[delta_col] = df.apply(
                        lambda r: _delta(r, sub_lookup, col), axis=1)
                else:
                    df[delta_col] = float("nan")

    return pd.concat(
        [frames[name] for name in CONDITION_ORDER],
        ignore_index=True, sort=False)


def _render_overlays(hits_by_condition: dict[str, pd.DataFrame],
                     figures_dir: Path, truth_points: list[tuple[float, float]]):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    colors = {
        "all_air":         "tab:gray",
        "beamline_vacuum": "tab:blue",
        "all_vacuum":      "tab:green",
    }
    figures_dir.mkdir(parents=True, exist_ok=True)
    for tpx, tpy in truth_points:
        fig, axes = plt.subplots(1, 2, figsize=(12, 5))
        for ax, which in zip(axes, ("pdc1", "pdc2")):
            for name in CONDITION_ORDER:
                df = hits_by_condition.get(name)
                if df is None:
                    continue
                sub = df[(df["truth_px"] == tpx) & (df["truth_py"] == tpy)]
                ax.scatter(sub[f"{which}_x"], sub[f"{which}_y"],
                           c=colors[name], s=18, alpha=0.6,
                           label=f"{name} (N={len(sub)})")
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
    parser.add_argument("--all-air-dir",         type=Path, required=True)
    parser.add_argument("--beamline-vacuum-dir", type=Path, required=True)
    parser.add_argument("--all-vacuum-dir",      type=Path, required=True)
    parser.add_argument("--out-dir",             type=Path, required=True)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)

    summaries = {
        "all_air":         pd.read_csv(args.all_air_dir         / "dispersion_summary.csv"),
        "beamline_vacuum": pd.read_csv(args.beamline_vacuum_dir / "dispersion_summary.csv"),
        "all_vacuum":      pd.read_csv(args.all_vacuum_dir      / "dispersion_summary.csv"),
    }

    compare = build_comparison(
        all_air         = summaries["all_air"],
        beamline_vacuum = summaries["beamline_vacuum"],
        all_vacuum      = summaries["all_vacuum"])
    compare_path = args.out_dir / "compare.csv"
    compare.to_csv(compare_path, index=False)
    print(f"[compare] wrote {compare_path}", flush=True)
    print(compare.to_string(index=False))

    hits = {}
    for name, d in [("all_air",         args.all_air_dir),
                    ("beamline_vacuum", args.beamline_vacuum_dir),
                    ("all_vacuum",      args.all_vacuum_dir)]:
        p = d / "pdc_hits.csv"
        if p.exists():
            hits[name] = pd.read_csv(p)
    if hits:
        truth_points = list(zip(summaries["all_air"]["truth_px"],
                                summaries["all_air"]["truth_py"]))
        _render_overlays(hits, args.out_dir / "figures", truth_points)


if __name__ == "__main__":
    sys.exit(main())
