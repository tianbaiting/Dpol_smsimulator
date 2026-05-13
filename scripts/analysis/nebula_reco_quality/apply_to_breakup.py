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
    # Bin truth neutron px/py to the same grid step as the (px,py) scan.
    df["pxn_bin"] = (df.truth_pxn / 20).round(0).astype(int) * 20
    df["pyn_bin"] = (df.truth_pyn / 10).round(0).astype(int) * 10

    joined = df.merge(eff[["px", "py", "eps_geom", "eps_det", "eps_reco"]],
                      left_on=["pxn_bin", "pyn_bin"],
                      right_on=["px", "py"], how="left")
    joined["eps_geom"] = joined.eps_geom.fillna(0)
    joined["eps_det"]  = joined.eps_det.fillna(0)
    joined["eps_reco"] = joined.eps_reco.fillna(0)

    rows = []
    for cut_col in ("loose", "mid", "tight"):
        sub = joined[joined[cut_col]]
        n_pos = (sub.truth_pxn > 0).sum()
        n_neg = (sub.truth_pxn < 0).sum()
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
