"""Precompute ε_geom over the (px,py,pz) grid using the pure-Python ray-cast.

Output: parquet with columns (px, py, pz, hit) — 'hit' is 0/1 from ray_hits_bar
across all 120 NEBULA Neut bars. Aggregation to ε_geom(px,py) is done in
analyze_efficiency.py.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import pandas as pd

from geom_acceptance import load_nebula_bars, ray_hits_bar


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--px-min", type=float, default=-200.0)
    ap.add_argument("--px-max", type=float, default=+200.0)
    ap.add_argument("--px-step", type=float, default=20.0)
    ap.add_argument("--py-min", type=float, default=-100.0)
    ap.add_argument("--py-max", type=float, default=+100.0)
    ap.add_argument("--py-step", type=float, default=10.0)
    ap.add_argument("--pz-list", type=str, default="550,600,627,650,700")
    args = ap.parse_args()

    repo = Path(__file__).resolve().parents[3]
    bars = load_nebula_bars(
        repo / "configs/simulation/geometry/NEBULA_Detectors_Dayone.csv",
        repo / "configs/simulation/geometry/NEBULA_Dayone.csv",
        neut_only=True,
    )

    px_grid = np.arange(args.px_min, args.px_max + 0.5 * args.px_step, args.px_step)
    py_grid = np.arange(args.py_min, args.py_max + 0.5 * args.py_step, args.py_step)
    pz_list = [float(s) for s in args.pz_list.split(",")]

    rows = []
    for px in px_grid:
        for py in py_grid:
            for pz in pz_list:
                hit = 0
                for b in bars:
                    if ray_hits_bar(0.0, 0.0, 0.0, float(px), float(py), pz, b):
                        hit = 1
                        break
                rows.append((float(px), float(py), pz, hit))

    df = pd.DataFrame(rows, columns=["px", "py", "pz", "hit"])
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    df.to_parquet(args.out, index=False)
    print(f"[compute_geom_acceptance] wrote {len(df)} grid points to {args.out}")


if __name__ == "__main__":
    main()
