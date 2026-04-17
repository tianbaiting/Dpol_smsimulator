#!/usr/bin/env python3
"""Extract PDC1/PDC2 hit positions per event from the reco ROOT file and join
with the error-analysis track_summary.csv on (event_index, track_index).

Outputs:
  pdc_hits_per_event.csv         raw PDC1/PDC2 XYZ per (event_index, track_index)
  ensemble_pdc_reco_merged.csv   inner join with track_summary.csv

Usage:
  micromamba run -n anaroot-env python3 scripts/analysis/extract_pdc_hits_from_reco.py
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import pandas as pd


DEFAULT_ROOT = (
    "build/test_output/ensemble_n50/rk_fixed_target_pdc_only/reco/"
    "pdc_truth_grid_rk_fixed_target_pdc_only_reco.root"
)
DEFAULT_TRACK_SUMMARY = (
    "build/test_output/ensemble_n50/rk_fixed_target_pdc_only_error_analysis/track_summary.csv"
)
DEFAULT_OUT_DIR = "build/test_output/ensemble_n50/g4_fluctuation"


def extract_pdc_hits(root_path: Path) -> pd.DataFrame:
    import ROOT  # deferred import so --help works without ROOT

    # Load the analysis library that provides RecoEvent / RecoTrack dictionaries.
    # The rootmap claims libanalysis.so but the actual built library is libpdcanalysis.so.
    for candidate in ("build/lib/libpdcanalysis.so", "build/lib/libanalysis.so"):
        if Path(candidate).exists():
            ROOT.gSystem.Load(candidate)
            break

    f = ROOT.TFile.Open(str(root_path), "READ")
    if not f or f.IsZombie():
        raise RuntimeError(f"cannot open {root_path}")
    tree = f.Get("recoTree")
    if not tree:
        raise RuntimeError("recoTree not found")

    rows = []
    n = tree.GetEntries()
    for entry in range(n):
        tree.GetEntry(entry)
        ev = tree.recoEvent
        if ev is None:
            continue
        for trk_idx, trk in enumerate(ev.tracks):
            if trk.pdgCode == 2112:  # skip neutrons (matches analyze_pdc_rk_error)
                continue
            rows.append({
                "event_index": entry,
                "track_index": trk_idx,
                "pdc1_x": trk.start.X(),
                "pdc1_y": trk.start.Y(),
                "pdc1_z": trk.start.Z(),
                "pdc2_x": trk.end.X(),
                "pdc2_y": trk.end.Y(),
                "pdc2_z": trk.end.Z(),
            })
    return pd.DataFrame(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=DEFAULT_ROOT)
    parser.add_argument("--track-summary", default=DEFAULT_TRACK_SUMMARY)
    parser.add_argument("--out-dir", default=DEFAULT_OUT_DIR)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    os.chdir(repo_root)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"[extract] reading {args.root}")
    hits = extract_pdc_hits(Path(args.root))
    print(f"[extract]   {len(hits)} (event_index, track_index) rows with charged tracks")
    hits_path = out_dir / "pdc_hits_per_event.csv"
    hits.to_csv(hits_path, index=False)
    print(f"[extract] wrote {hits_path}")

    print(f"[extract] reading {args.track_summary}")
    ts = pd.read_csv(args.track_summary)
    print(f"[extract]   {len(ts)} track_summary rows")

    merged = ts.merge(hits, on=["event_index", "track_index"], how="left",
                      validate="one_to_one")
    missing = merged["pdc1_x"].isna().sum()
    print(f"[extract] merged rows: {len(merged)}; missing PDC hit: {missing}")
    merged_path = out_dir / "ensemble_pdc_reco_merged.csv"
    merged.to_csv(merged_path, index=False)
    print(f"[extract] wrote {merged_path}")

    # Sanity summary per truth point
    grp = merged.groupby(["truth_px", "truth_py"]).size().rename("N")
    print("[extract] events per truth point:")
    print(grp.to_string())


if __name__ == "__main__":
    sys.exit(main())
