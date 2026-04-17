#!/usr/bin/env python3
"""Per-truth-point Geant4 robust half-width vs median Fisher sigma.

Groups track_summary.csv by (truth_px, truth_py); for each component
px, py, pz, |p| writes robust half-width = (p84-p16)/2 of the reco
value and the median analyzer-reported Fisher sigma.

Usage:
  micromamba run -n anaroot-env python3 \
    scripts/analysis/compute_per_truth_g4_vs_fisher.py
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path

import numpy as np
import pandas as pd

DEFAULT_TRACK_SUMMARY = (
    "build/test_output/ensemble_n50/"
    "rk_fixed_target_pdc_only_error_analysis/track_summary.csv"
)
DEFAULT_OUT = (
    "build/test_output/ensemble_n50/g4_fluctuation/"
    "per_truth_point_g4_vs_fisher.csv"
)


def robust_halfwidth(s: pd.Series) -> float:
    a = s.to_numpy()
    return 0.5 * (np.percentile(a, 84) - np.percentile(a, 16))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--track-summary", default=DEFAULT_TRACK_SUMMARY)
    parser.add_argument("--out", default=DEFAULT_OUT)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    os.chdir(repo_root)

    df = pd.read_csv(args.track_summary)
    if "reco_p" not in df.columns:
        df["reco_p"] = np.sqrt(
            df["reco_px"] ** 2 + df["reco_py"] ** 2 + df["reco_pz"] ** 2
        )

    rows = []
    for (px, py), g in df.groupby(["truth_px", "truth_py"]):
        row = {"truth_px": px, "truth_py": py, "N": len(g)}
        for comp in ("px", "py", "pz", "p"):
            reco = g[f"reco_{comp}"]
            truth_col = f"truth_{comp}" if comp != "p" else None
            if truth_col and truth_col in g.columns:
                resid = reco - g[truth_col]
            else:
                resid = reco - reco.median()
            fcol = f"fisher_{comp}_sigma"
            fisher = g[fcol] if fcol in g.columns else pd.Series([np.nan])
            row[f"g4_halfw68_{comp}"] = robust_halfwidth(resid)
            row[f"std_{comp}"] = float(resid.std(ddof=1)) if len(resid) > 1 else float("nan")
            row[f"fisher_sigma_{comp}"] = float(fisher.median())
        rows.append(row)

    out = pd.DataFrame(rows).sort_values(["truth_py", "truth_px"]).reset_index(drop=True)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out.to_csv(out_path, index=False)
    print(f"[per-truth] wrote {out_path}  ({len(out)} rows)")

    # Global robust half-width vs Fisher sigma summary (for report prose)
    # Use residuals (reco - truth) per component so global value is pure G4 spread,
    # not a mixture including truth grid variation.
    for comp in ("px", "py", "pz", "p"):
        truth_col = f"truth_{comp}" if comp != "p" else None
        if truth_col and truth_col in df.columns:
            all_resid = df[f"reco_{comp}"] - df[truth_col]
        else:
            all_resid = df[f"reco_{comp}"] - df[f"reco_{comp}"].median()
        fcol = f"fisher_{comp}_sigma"
        all_fisher = df[fcol] if fcol in df.columns else None
        hw = robust_halfwidth(all_resid)
        fi = float(all_fisher.median()) if all_fisher is not None else float("nan")
        ratio = hw / fi if fi > 0 else float("nan")
        print(f"  {comp:3s}: global G4 hw={hw:.2f}   Fisher med={fi:.2f}   ratio={ratio:.2f}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
