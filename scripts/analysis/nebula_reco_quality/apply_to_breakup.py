"""Weight ε(px,py) by the breakup truth distribution from joined.parquet,
emit a table summarizing how the px asymmetry of the R-cut subset is shaped by
NEBULA acceptance.

Coordinate frame note
---------------------
The ε grid (eps_geom_grid.parquet and efficiency_grid.parquet) is stored in
**target frame** (px, py are target-frame components).  The joined.parquet truth
columns truth_pxn/truth_pyn/truth_pzn are in **lab frame**.  Before binning,
this script rotates truth lab → target via R_y(-target_angle_deg):

    px_tgt =  cos(θ) * px_lab - sin(θ) * pz_lab
    py_tgt =  py_lab
    pz_tgt = +sin(θ) * px_lab + cos(θ) * pz_lab

Default target_angle_deg = 3.0° (from 3deg_1.15T.mac).
"""
from __future__ import annotations
import argparse
import math
from pathlib import Path
import numpy as np
import pandas as pd


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--joined", required=True, help="joined.parquet from nn_breakup_phys")
    ap.add_argument("--efficiency", required=True, help="efficiency_grid.parquet")
    ap.add_argument("--out", required=True)
    ap.add_argument("--target-angle-deg", type=float, default=3.0,
                    help="Target rotation about Y axis [degrees] used to build "
                         "the ε grid (default: 3.0). Truth lab-frame momentum is "
                         "rotated to target frame before binning.")
    args = ap.parse_args()

    df = pd.read_parquet(args.joined)
    eff = pd.read_parquet(args.efficiency)

    df = df[(df.truth_has_proton == 1) & (df.truth_has_neutron == 1)].copy()

    # [EN] Rotate truth neutron momentum from lab frame to target frame so it
    # can be matched against the target-frame ε grid.
    # [CN] 将真值中子动量从实验室系旋转到靶系，以匹配靶系下的 ε 网格。
    ang = math.radians(args.target_angle_deg)
    ct, st = math.cos(ang), math.sin(ang)
    df["truth_pxn_tgt"] =  ct * df["truth_pxn"] - st * df["truth_pzn"]
    df["truth_pzn_tgt"] = +st * df["truth_pxn"] + ct * df["truth_pzn"]
    df["truth_pyn_tgt"] =  df["truth_pyn"]

    # Bin target-frame px/py to the same grid step as the (px,py) scan.
    df["pxn_bin"] = (df["truth_pxn_tgt"] / 20).round(0).astype(int) * 20
    df["pyn_bin"] = (df["truth_pyn_tgt"] / 10).round(0).astype(int) * 10

    joined = df.merge(eff[["px", "py", "eps_geom", "eps_det", "eps_reco"]],
                      left_on=["pxn_bin", "pyn_bin"],
                      right_on=["px", "py"], how="left")
    joined["eps_geom"] = joined["eps_geom"].fillna(0)
    joined["eps_det"]  = joined["eps_det"].fillna(0)
    joined["eps_reco"] = joined["eps_reco"].fillna(0)

    # [EN] Sign of neutron px asymmetry is evaluated in target frame to be
    # consistent with the ε grid frame. / [CN] 中子 px 不对称性的符号在靶系中
    # 评估，与 ε 网格保持一致。
    rows = []
    for cut_col in ("loose", "mid", "tight"):
        sub = joined[joined[cut_col]]
        pxn_tgt = sub["truth_pxn_tgt"]
        n_pos = (pxn_tgt > 0).sum()
        n_neg = (pxn_tgt < 0).sum()
        w_pos_g = sub["eps_geom"][pxn_tgt > 0].sum()
        w_neg_g = sub["eps_geom"][pxn_tgt < 0].sum()
        w_pos_r = sub["eps_reco"][pxn_tgt > 0].sum()
        w_neg_r = sub["eps_reco"][pxn_tgt < 0].sum()
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
