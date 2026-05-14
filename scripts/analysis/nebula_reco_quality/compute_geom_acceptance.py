"""Precompute ε_geom over the (px,py,pz) grid using the pure-Python ray-cast.

Output: parquet with columns (px, py, pz, hit) — 'hit' is 0/1 from the ray-cast
across all 120 NEBULA Neut bars.  px/py/pz are **target-frame** values.
Aggregation to ε_geom(px,py) is done in analyze_efficiency.py.

The target frame is rotated by --target-angle-deg about the Y axis relative to lab.
Ray origin is the target position in lab-frame mm (--target-position-mm).
Defaults match 3deg_1.15T.mac: origin=(-12.488, 0.009, -1069.458) mm, angle=3°.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import pandas as pd

from geom_acceptance import geom_acceptance, load_nebula_bars


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
    ap.add_argument("--target-position-mm", type=str,
                    default="-12.488,0.009,-1069.458",
                    help="Target position in lab-frame mm as 'x,y,z' "
                         "(default: '-12.488,0.009,-1069.458' from 3deg_1.15T.mac)")
    ap.add_argument("--target-angle-deg", type=float, default=3.0,
                    help="Target rotation about Y axis [degrees] "
                         "(default: 3.0 from 3deg_1.15T.mac). "
                         "px/py/pz grid is in target frame.")
    args = ap.parse_args()

    tgt_pos_vals = [float(v) for v in args.target_position_mm.split(",")]
    if len(tgt_pos_vals) != 3:
        raise ValueError(f"--target-position-mm must be 'x,y,z', got: {args.target_position_mm}")
    target_pos = tuple(tgt_pos_vals)

    print(f"[compute_geom_acceptance] frame=target, angle={args.target_angle_deg}°, "
          f"origin=({target_pos[0]},{target_pos[1]},{target_pos[2]}) mm")

    repo = Path(__file__).resolve().parents[3]
    detectors_csv = repo / "configs/simulation/geometry/NEBULA_Detectors_Dayone.csv"
    nebula_csv    = repo / "configs/simulation/geometry/NEBULA_Dayone.csv"

    # Pre-load bar list once; pass via bars_cache so geom_acceptance reuses it.
    bars_cache: dict = {}

    px_grid = np.arange(args.px_min, args.px_max + 0.5 * args.px_step, args.px_step)
    py_grid = np.arange(args.py_min, args.py_max + 0.5 * args.py_step, args.py_step)
    pz_list = [float(s) for s in args.pz_list.split(",")]

    rows = []
    for px in px_grid:
        for py in py_grid:
            for pz in pz_list:
                # [EN] px/py/pz are target-frame; geom_acceptance rotates to lab
                # internally using target_angle_deg and traces from target_pos.
                # [CN] px/py/pz 是靶系分量；geom_acceptance 内部旋转到实验室系后做射线追踪。
                hit = 1 if geom_acceptance(
                    float(px), float(py), pz,
                    detectors_csv, nebula_csv,
                    target_pos=target_pos,
                    target_angle_deg=args.target_angle_deg,
                    bars_cache=bars_cache,
                ) else 0
                rows.append((float(px), float(py), pz, hit))

    df = pd.DataFrame(rows, columns=["px", "py", "pz", "hit"])
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    df.to_parquet(args.out, index=False)
    print(f"[compute_geom_acceptance] wrote {len(df)} grid points to {args.out} "
          f"(frame=target, angle={args.target_angle_deg}°, "
          f"origin=({target_pos[0]},{target_pos[1]},{target_pos[2]}) mm)")


if __name__ == "__main__":
    main()
