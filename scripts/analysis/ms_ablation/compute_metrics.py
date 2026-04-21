#!/usr/bin/env python3
"""Compute PDC hit and angle dispersion summary for MS ablation study.

Reads a reco ROOT file plus the truth point list, groups events by
(truth_px, truth_py), and writes a robust-half-width summary CSV.

Usage:
  micromamba run -n anaroot-env python3 \
    scripts/analysis/ms_ablation/compute_metrics.py \
    --reco-root <path> \
    --out-dir <path>
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import numpy as np
import pandas as pd


TRUTH_POINTS = [(0.0, 0.0), (100.0, 0.0), (0.0, 20.0)]
TRUTH_TOL = 1e-3  # MeV/c


def _robust_half_width(values: np.ndarray) -> float:
    if len(values) < 2:
        return float("nan")
    p84, p16 = np.percentile(values, [84, 16])
    return float((p84 - p16) / 2.0)


def compute_dispersion_summary(
    hits: pd.DataFrame,
    truth_points: list[tuple[float, float]],
) -> pd.DataFrame:
    rows = []
    for tpx, tpy in truth_points:
        mask = (np.abs(hits["truth_px"] - tpx) < TRUTH_TOL) & \
               (np.abs(hits["truth_py"] - tpy) < TRUTH_TOL)
        sub = hits[mask]
        n = len(sub)
        row = {"truth_px": tpx, "truth_py": tpy, "N": n}
        if n == 0:
            for col in ("sigma_x_pdc1_mm", "sigma_y_pdc1_mm",
                        "sigma_x_pdc2_mm", "sigma_y_pdc2_mm",
                        "sigma_theta_x_mrad", "sigma_theta_y_mrad"):
                row[col] = float("nan")
            rows.append(row)
            continue

        row["sigma_x_pdc1_mm"] = _robust_half_width(sub["pdc1_x"].to_numpy())
        row["sigma_y_pdc1_mm"] = _robust_half_width(sub["pdc1_y"].to_numpy())
        row["sigma_x_pdc2_mm"] = _robust_half_width(sub["pdc2_x"].to_numpy())
        row["sigma_y_pdc2_mm"] = _robust_half_width(sub["pdc2_y"].to_numpy())

        dz = sub["pdc2_z"].to_numpy() - sub["pdc1_z"].to_numpy()
        theta_x = (sub["pdc2_x"].to_numpy() - sub["pdc1_x"].to_numpy()) / dz
        theta_y = (sub["pdc2_y"].to_numpy() - sub["pdc1_y"].to_numpy()) / dz
        row["sigma_theta_x_mrad"] = _robust_half_width(theta_x) * 1000.0
        row["sigma_theta_y_mrad"] = _robust_half_width(theta_y) * 1000.0

        rows.append(row)
    return pd.DataFrame(rows)


def _load_pdc_hits_from_reco(reco_root: Path) -> pd.DataFrame:
    """Read reco.root and return DataFrame with truth_* + pdc1_*/pdc2_* columns.

    Logic mirrors scripts/analysis/extract_pdc_hits_from_reco.py:extract_pdc_hits
    but additionally pulls truth_proton_p4 per event so we can group by truth.
    """
    import ROOT

    for candidate in ("build/lib/libpdcanalysis.so", "build/lib/libanalysis.so"):
        if Path(candidate).exists():
            ROOT.gSystem.Load(candidate)
            break

    f = ROOT.TFile.Open(str(reco_root), "READ")
    if not f or f.IsZombie():
        raise RuntimeError(f"cannot open {reco_root}")
    tree = f.Get("recoTree")
    if not tree:
        raise RuntimeError("recoTree not found in reco.root")

    required = {"recoEvent", "truth_proton_p4"}
    branches = {b.GetName() for b in tree.GetListOfBranches()}
    missing = required - branches
    if missing:
        raise RuntimeError(
            f"reco.root missing required branches: {missing}. Found: {sorted(branches)}"
        )

    rows = []
    for entry in range(tree.GetEntries()):
        tree.GetEntry(entry)
        ev = tree.recoEvent
        if ev is None:
            continue
        truth_p4 = tree.truth_proton_p4
        truth_px = float(truth_p4.X())
        truth_py = float(truth_p4.Y())
        for trk_idx, trk in enumerate(ev.tracks):
            if trk.pdgCode == 2112:
                continue
            rows.append({
                "event_index": entry,
                "track_index": trk_idx,
                "truth_px": truth_px,
                "truth_py": truth_py,
                "pdc1_x": trk.start.X(), "pdc1_y": trk.start.Y(), "pdc1_z": trk.start.Z(),
                "pdc2_x": trk.end.X(),   "pdc2_y": trk.end.Y(),   "pdc2_z": trk.end.Z(),
            })
    f.Close()
    return pd.DataFrame(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reco-root", type=Path, required=True,
                        help="Path to reco.root produced by test_pdc_target_momentum_e2e")
    parser.add_argument("--out-dir", type=Path, required=True,
                        help="Output directory for CSVs")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[3]
    os.chdir(repo_root)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    print(f"[compute] reading {args.reco_root}", flush=True)
    hits = _load_pdc_hits_from_reco(args.reco_root)
    print(f"[compute]   {len(hits)} charged tracks", flush=True)

    hits_path = args.out_dir / "pdc_hits.csv"
    hits.to_csv(hits_path, index=False)
    print(f"[compute] wrote {hits_path}", flush=True)

    summary = compute_dispersion_summary(hits, TRUTH_POINTS)
    summary_path = args.out_dir / "dispersion_summary.csv"
    summary.to_csv(summary_path, index=False)
    print(f"[compute] wrote {summary_path}", flush=True)
    print(summary.to_string(index=False))


if __name__ == "__main__":
    sys.exit(main())
