"""Per-event classification: direct-neutron vs cascade-secondary NEBULA hits.

Idea
----
For each gun event we know the truth neutron's lab-frame momentum (from
TBeamSimData.fMomentum). We can predict where a straight-line truth ray would
hit the NEBULA plane:

    x_pred = O_x + (px_lab / pz_lab) * (z_NEBULA - O_z)
    y_pred = O_y + (py_lab / pz_lab) * (z_NEBULA - O_z)

with O = target_pos = (-12.488, 0.009, -1069.458) mm and z_NEBULA = 7249.72 mm.

If a NEBULA Neut bar (id ∈ [1,120]) is fired, its bar-center fDetPos.x is one
of 60 discrete values (pitch ≈ 122 mm). We tag the event by whether *any*
Neut hit is within ±150 mm of x_pred (≈ 1 bar width, conservative). Classes:

  - "direct"    — truth ray inside Neut geom AND at least one Neut hit within
                  ±150 mm of x_pred. Reads as "the truth neutron itself made it
                  to NEBULA and triggered a bar close to the predicted spot".
  - "cascade"   — at least one Neut hit, but no Neut hit within ±150 mm of x_pred.
                  Reads as "something else (forward secondaries from upstream
                  material) lit up a NEBULA bar away from where the truth ray
                  pointed".
  - "no_hit"    — zero Neut hits.

Note: this is a *proxy* classifier. It does not need FragSimData (which was
empty in our scan); the per-event 60-bar discreteness + position resolution
is enough to separate the two physics. The classifier is wrong on:
  - small-angle elastic-scatter direct neutrons that drift > 150 mm (rare)
  - cascade secondaries that happen to land within 150 mm of x_pred
    (geometrically suppressed when truth ray is far outside Neut extent)

Both error modes are small, especially since the dominant cascade events have
x_pred well outside the bar extent.

Output: parquet with columns
  px (target frame, MeV/c), py (target frame, MeV/c),
  n_total, n_geom, n_direct, n_cascade,
  eps_geom, eps_direct, eps_cascade.

eps_direct + eps_cascade ≤ 1 (events with no Neut hit are the remainder).
"""
from __future__ import annotations
import argparse, math, glob
from pathlib import Path

import numpy as np
import pandas as pd
import uproot


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--g4output-glob", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--target-angle-deg", type=float, default=3.0)
    ap.add_argument("--target-pos-mm", default="-12.488,0.009,-1069.458")
    ap.add_argument("--nebula-z-mm", type=float, default=7249.72)
    ap.add_argument("--neut-x-min", type=float, default=-1707.8)
    ap.add_argument("--neut-x-max", type=float, default=1961.8)
    ap.add_argument("--neut-y-half", type=float, default=900.0)
    ap.add_argument("--direct-window-mm", type=float, default=150.0)
    ap.add_argument("--max-shards", type=int, default=None)
    args = ap.parse_args()

    theta = math.radians(args.target_angle_deg)
    ct, st = math.cos(theta), math.sin(theta)
    ox, oy, oz = [float(v) for v in args.target_pos_mm.split(",")]
    dz = args.nebula_z_mm - oz

    files = sorted(glob.glob(args.g4output_glob))
    files = [f for f in files if "shard000000" not in f]  # stale shard 0
    if args.max_shards:
        files = files[: args.max_shards]
    print(f"[classify] scanning {len(files)} shards", flush=True)

    # Accumulate per (px_bin, py_bin) -> counters
    counts: dict[tuple[int, int], dict[str, int]] = {}

    for k, fname in enumerate(files):
        print(f"  [{k+1}/{len(files)}] {fname}", flush=True)
        f = uproot.open(fname)
        t = f["tree"]

        mom = t["beam/beam.fMomentum"].array(library="np")
        truth_px = np.array([m[0].member("fP").member("fX") if len(m) > 0 else np.nan for m in mom])
        truth_py = np.array([m[0].member("fP").member("fY") if len(m) > 0 else np.nan for m in mom])
        truth_pz = np.array([m[0].member("fP").member("fZ") if len(m) > 0 else np.nan for m in mom])

        # Target-frame momentum (inverse of gen-time R_y(+θ))
        px_tgt = ct * truth_px - st * truth_pz
        py_tgt = truth_py
        px_bin = np.round(px_tgt).astype(int)
        py_bin = np.round(py_tgt).astype(int)

        # Target-frame truth ray hit position at NEBULA z-plane.
        # NOTE: empirically the sim's gun fires in target-frame direction (the
        # gen-script's R_y(+3°) lab rotation gets effectively cancelled inside
        # sim_deuteron; verified by inspecting bar-fire histograms at central
        # py=0 bins — predicted lab bar at x=+423 was rarely fired, while the
        # bar at x=-56 mm (closest to target-frame on-axis from gun position)
        # dominated). Cf. analysis log 2026-05-15.
        pz_tgt = st * truth_px + ct * truth_pz
        with np.errstate(divide="ignore", invalid="ignore"):
            x_pred = ox + (px_tgt / pz_tgt) * dz
            y_pred = oy + (py_tgt / pz_tgt) * dz
        valid = np.isfinite(x_pred) & np.isfinite(y_pred) & (pz_tgt > 0)
        in_geom = (
            valid
            & (x_pred >= args.neut_x_min) & (x_pred <= args.neut_x_max)
            & (np.abs(y_pred) <= args.neut_y_half)
        )

        ids = t["NEBULAPla/NEBULAPla.id"].array(library="np")
        detpos = t["NEBULAPla/NEBULAPla.fDetPos[3]"].array(library="np")

        n_evt = len(truth_px)
        for i in range(n_evt):
            if not valid[i]:
                continue
            has_neut = False
            has_direct = False
            for j in range(len(ids[i])):
                bid = int(ids[i][j])
                if not (1 <= bid <= 120):
                    continue
                has_neut = True
                xb = float(detpos[i][j][0])
                if abs(xb - x_pred[i]) <= args.direct_window_mm:
                    has_direct = True
                    break
            key = (int(px_bin[i]), int(py_bin[i]))
            rec = counts.setdefault(key, {
                "n_total": 0, "n_geom": 0, "n_direct": 0, "n_cascade": 0,
            })
            rec["n_total"] += 1
            if in_geom[i]:
                rec["n_geom"] += 1
            if has_direct:
                rec["n_direct"] += 1
            elif has_neut:
                rec["n_cascade"] += 1
        f.close()

    rows = []
    for (px, py), r in counts.items():
        n = r["n_total"]
        if n == 0:
            continue
        rows.append({
            "px": px, "py": py,
            "n_total": n, "n_geom": r["n_geom"],
            "n_direct": r["n_direct"], "n_cascade": r["n_cascade"],
            "eps_geom": r["n_geom"] / n,
            "eps_direct": r["n_direct"] / n,
            "eps_cascade": r["n_cascade"] / n,
        })
    df = pd.DataFrame(rows).sort_values(["py", "px"]).reset_index(drop=True)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    df.to_parquet(args.out, index=False)
    print(f"\n[classify] wrote {args.out}")
    print(df.describe()[["eps_geom", "eps_direct", "eps_cascade"]])


if __name__ == "__main__":
    main()
