"""Compute ε_geom, ε_det, ε_reco and conditional efficiencies per (px,py) bin.

scan_summary truth_px/py/pz are stored in **lab frame** (TBeamSimData.fMomentum is
lab-frame). eps_geom_grid px/py/pz are in **target frame** (the gen script's grid
is target-frame). To merge correctly, rotate scan_summary truth_px,pz back to
target frame via R_y(-target_angle_deg) before binning.
"""
from __future__ import annotations
import argparse
import math
from pathlib import Path
import numpy as np
import pandas as pd


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scan-summary", required=True)
    ap.add_argument("--geom-grid", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--energy-threshold", type=float, default=1.0,
                    help="MeV (nebula_sumE) for ε_det")
    ap.add_argument("--target-angle-deg", type=float, default=3.0,
                    help="Target rotation about Y [deg]; truth_px is lab-frame, "
                         "rotate by -angle to convert to target frame for binning "
                         "(default: 3.0 matching 3deg_1.15T.mac)")
    args = ap.parse_args()

    scan = pd.read_parquet(args.scan_summary)
    geom = pd.read_parquet(args.geom_grid)

    # Lab -> target via R_y(-theta) (inverse of the rotation applied at gen-time).
    theta = math.radians(args.target_angle_deg)
    ct, st = math.cos(theta), math.sin(theta)
    scan["truth_px_tgt"] =  ct * scan.truth_px - st * scan.truth_pz
    scan["truth_pz_tgt"] =  st * scan.truth_px + ct * scan.truth_pz
    scan["truth_py_tgt"] = scan.truth_py

    # Bin in target frame (jitter sigma=0.1 MeV/c, rotation preserves precision).
    scan["px_bin"] = scan.truth_px_tgt.round(0).astype(int)
    scan["py_bin"] = scan.truth_py_tgt.round(0).astype(int)

    # ε_det / ε_reco: marginalize pz (pool all 5 pz sub-points within each (px,py)).
    g = scan.groupby(["px_bin", "py_bin"])
    rows = []
    for (px, py), sub in g:
        n = len(sub)
        n_det = int((sub.nebula_sumE > args.energy_threshold).sum())
        n_reco = int((sub.n_reco_neutrons > 0).sum())
        rows.append((px, py, n, n_det, n_reco, n_det/n, n_reco/n))
    df_det = pd.DataFrame(rows,
                          columns=["px","py","n","n_det","n_reco","eps_det","eps_reco"])

    # ε_geom: precomputed per (px,py,pz); marginalize pz uniformly (mean over the 5 pz).
    geom_avg = (geom.groupby(["px", "py"]).hit.mean()
                .reset_index().rename(columns={"hit": "eps_geom"}))

    df = df_det.merge(geom_avg, on=["px", "py"], how="left")
    df["eps_det_given_geom"] = np.where(df.eps_geom > 0, df.eps_det / df.eps_geom, np.nan)
    df["eps_reco_given_det"] = np.where(df.eps_det > 0, df.eps_reco / df.eps_det, np.nan)

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    df.to_parquet(args.out, index=False)
    print(f"[analyze_efficiency] {len(df)} (px,py) bins -> {args.out}")
    print(df.describe()[["eps_geom","eps_det","eps_reco","eps_det_given_geom","eps_reco_given_det"]])


if __name__ == "__main__":
    main()
