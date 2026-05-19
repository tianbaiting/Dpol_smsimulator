"""Build evidence for the cascade hypothesis using only NEBULAPla data.

For each (px_bin, py_bin=0) of interest:
  - Layer (L1/L2) hit ratio: direct neutrons preferentially deposit deeper (L2)
    because the bars are thin and neutrons interact ~uniformly along the stack.
    Cascade secondaries (mostly low-energy protons/gammas from upstream Fe) stop
    in the first scintillator slab they encounter → L1 dominates.
  - fDetPos.x distribution: pure secondary cascade spray cannot exceed the
    rightmost Neut bar center (1901.8 mm). If 100% of edge hits are at the
    boundary bar, that's a signature of a sharply-bounded cascade cone.
  - Per-bar fQAveCal distribution: direct neutron typical edep ~tens of MeV per
    interaction; low-energy cascade products tend to give smaller per-bar values
    or distinct multi-bar patterns.
  - Truth ray geometric x_hit vs measured fDetPos.x: tells us how far off the
    direct trajectory the hits fall.
  - Hit multiplicity distribution: cascade tends to produce more bars per event.

Also estimates the upstream iron column density along the truth ray at off-axis
angles, to argue dipole-iron-induced cascade is quantitatively plausible.
"""
from __future__ import annotations
import argparse, math, glob, json
from pathlib import Path
from collections import Counter, defaultdict

import numpy as np
import pandas as pd
import uproot


def analyze(g4output_glob, px_bins, theta_deg, max_shards):
    theta = math.radians(theta_deg)
    ct, st = math.cos(theta), math.sin(theta)

    files = sorted(glob.glob(g4output_glob))
    # Skip shard 0 (stale schema)
    files = [f for f in files if "shard000000" not in f]
    if max_shards:
        files = files[:max_shards]
    print(f"[evidence] scanning {len(files)} shards")

    # Per-bin aggregates
    A = {b: {
        "n_events": 0,
        "n_neut_hits": 0,
        "n_veto_hits": 0,
        "n_evt_with_neut_hit": 0,
        "layer_count": Counter(),
        "detpos_x": [],
        "detpos_x_layer1": [],
        "detpos_x_layer2": [],
        "qave_per_bar": [],   # fQAveCal per Neut hit
        "n_neut_per_evt": [], # multiplicity per event with at least 1 Neut hit
        "sumE_per_evt": [],   # nebula_sumE (Neut-only) per event-with-Neut-hit
    } for b in px_bins}

    target_set = set(px_bins)

    for k, fname in enumerate(files):
        print(f"  [{k+1}/{len(files)}] {fname}", flush=True)
        f = uproot.open(fname)
        t = f["tree"]

        mom = t["beam/beam.fMomentum"].array(library="np")
        truth_px = np.array([m[0].member("fP").member("fX") if len(m) > 0 else np.nan for m in mom])
        truth_py = np.array([m[0].member("fP").member("fY") if len(m) > 0 else np.nan for m in mom])
        truth_pz = np.array([m[0].member("fP").member("fZ") if len(m) > 0 else np.nan for m in mom])
        px_tgt = ct * truth_px - st * truth_pz
        py_tgt = truth_py
        px_bin = np.round(px_tgt).astype(int)
        py_bin = np.round(py_tgt).astype(int)

        ids = t["NEBULAPla/NEBULAPla.id"].array(library="np")
        layer = t["NEBULAPla/NEBULAPla.fLayer"].array(library="np")
        sublayer = t["NEBULAPla/NEBULAPla.fSubLayer"].array(library="np")
        detpos = t["NEBULAPla/NEBULAPla.fDetPos[3]"].array(library="np")
        qcal = t["NEBULAPla/NEBULAPla.fQAveCal"].array(library="np")

        mask = (py_bin == 0) & np.isin(px_bin, list(target_set))
        for i in np.where(mask)[0]:
            b = int(px_bin[i])
            rec = A[b]
            rec["n_events"] += 1
            n_neut_this = 0
            sumE_neut_this = 0.0
            for j in range(len(ids[i])):
                bid = int(ids[i][j])
                if 1 <= bid <= 120:
                    rec["n_neut_hits"] += 1
                    n_neut_this += 1
                    L = int(layer[i][j]); S = int(sublayer[i][j])
                    rec["layer_count"][(L, S)] += 1
                    x = float(detpos[i][j][0])
                    rec["detpos_x"].append(x)
                    if L == 1:
                        rec["detpos_x_layer1"].append(x)
                    else:
                        rec["detpos_x_layer2"].append(x)
                    q = float(qcal[i][j])
                    rec["qave_per_bar"].append(q)
                    sumE_neut_this += q
                elif bid >= 121:
                    rec["n_veto_hits"] += 1
            if n_neut_this > 0:
                rec["n_evt_with_neut_hit"] += 1
                rec["n_neut_per_evt"].append(n_neut_this)
                rec["sumE_per_evt"].append(sumE_neut_this)
        f.close()

    return A


def truth_ray_x_at_neut(px_tgt, pz_tgt, theta_rad, target_pos=(-12.488, 0.009, -1069.458),
                       z_neut=7249.72):
    ct, st = math.cos(theta_rad), math.sin(theta_rad)
    px_lab = ct * px_tgt + st * pz_tgt
    pz_lab = -st * px_tgt + ct * pz_tgt
    if pz_lab <= 0: return float("nan")
    dz = z_neut - target_pos[2]
    return target_pos[0] + (px_lab / pz_lab) * dz


def estimate_iron_path(px_tgt, pz_tgt, theta_rad):
    """Cartoon estimate of SAMURAI dipole iron column along the truth ray.

    Treat the dipole iron yoke as a steel cylinder centered at (x=0, z=0) with
    inner radius (vacuum gap) R_in ≈ 0.5 m (rough) and outer radius R_out ≈ 2 m,
    in the transverse plane, finite z-extent ±1.0 m. (These are order-of-magnitude
    placeholders — the exact yoke profile isn't in the GDML I have access to here;
    the point is to show that a ~15° ray crosses substantially more iron than a
    forward ray.) Returns an *order-of-magnitude* path length in iron, mm.
    """
    R_in = 500.0   # mm
    R_out = 2000.0
    z_min, z_max = -1500.0, 1500.0
    theta_x = math.atan2(*_lab_dir(px_tgt, pz_tgt, theta_rad))
    # crude: chord length through annular shell intersected with z-slab.
    # Ray from target z=-1069, going at angle theta_x in x-z plane.
    # We approximate by integrating in z, checking if |x(z)| is in [R_in, R_out].
    ct, st = math.cos(theta_rad), math.sin(theta_rad)
    px_lab = ct * px_tgt + st * pz_tgt
    pz_lab = -st * px_tgt + ct * pz_tgt
    if pz_lab <= 0: return 0.0
    slope = px_lab / pz_lab
    origin_x, origin_z = -12.488, -1069.458
    dl_in_iron = 0.0
    n = 200
    dz = (z_max - z_min) / n
    for i in range(n):
        z = z_min + (i + 0.5) * dz
        if z < origin_z: continue
        x = origin_x + slope * (z - origin_z)
        if R_in <= abs(x) <= R_out:
            dl_in_iron += dz * math.hypot(slope, 1.0)
    return dl_in_iron


def _lab_dir(px_tgt, pz_tgt, theta_rad):
    ct, st = math.cos(theta_rad), math.sin(theta_rad)
    return (ct * px_tgt + st * pz_tgt, -st * px_tgt + ct * pz_tgt)


def report(A, px_bins, theta_deg, out_dir: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    theta_rad = math.radians(theta_deg)
    rows = []
    # Average over 5 pz nominal values (550..700) since each bin pools them.
    # For truth-ray x_hit we report at the central pz=627.
    for b in sorted(px_bins):
        r = A[b]
        n_evt = r["n_events"]
        n_evt_hit = r["n_evt_with_neut_hit"]
        layer = r["layer_count"]
        L1 = layer.get((1, 1), 0) + layer.get((1, 2), 0)
        L2 = layer.get((2, 1), 0) + layer.get((2, 2), 0)
        x_hit_central = truth_ray_x_at_neut(float(b), 627.0, theta_rad)
        # Compute geometric "in/out" of Neut acceptance: bar extent is [-1707.8, +1961.8] mm
        in_geom = -1707.8 <= x_hit_central <= 1961.8
        l1_over_l2 = (L1 / L2) if L2 > 0 else float("inf")
        mean_xpos = float(np.mean(r["detpos_x"])) if r["detpos_x"] else float("nan")
        max_xpos = float(np.max(r["detpos_x"])) if r["detpos_x"] else float("nan")
        q_arr = np.array(r["qave_per_bar"]) if r["qave_per_bar"] else np.array([])
        m_arr = np.array(r["n_neut_per_evt"]) if r["n_neut_per_evt"] else np.array([])
        sumE_arr = np.array(r["sumE_per_evt"]) if r["sumE_per_evt"] else np.array([])
        rows.append({
            "px_bin": b,
            "n_events": n_evt,
            "n_evt_with_neut_hit": n_evt_hit,
            "eps_neut": (n_evt_hit / n_evt) if n_evt else 0.0,
            "n_neut_hits": r["n_neut_hits"],
            "n_veto_hits": r["n_veto_hits"],
            "L1_hits": L1,
            "L2_hits": L2,
            "L1_over_L2": l1_over_l2,
            "truth_ray_x_at_neut_pz627_mm": x_hit_central,
            "truth_ray_in_geom_pz627": in_geom,
            "mean_hit_x_mm": mean_xpos,
            "max_hit_x_mm": max_xpos,
            "qave_median_MeV": float(np.median(q_arr)) if len(q_arr) else float("nan"),
            "qave_mean_MeV": float(np.mean(q_arr)) if len(q_arr) else float("nan"),
            "mult_median": float(np.median(m_arr)) if len(m_arr) else float("nan"),
            "mult_mean": float(np.mean(m_arr)) if len(m_arr) else float("nan"),
            "sumE_neut_median_MeV": float(np.median(sumE_arr)) if len(sumE_arr) else float("nan"),
            "sumE_neut_mean_MeV": float(np.mean(sumE_arr)) if len(sumE_arr) else float("nan"),
        })
    df = pd.DataFrame(rows).sort_values("px_bin").reset_index(drop=True)
    df.to_parquet(out_dir / "cascade_evidence_summary.parquet", index=False)
    df.to_csv(out_dir / "cascade_evidence_summary.csv", index=False)
    print(df.to_string(index=False))
    print(f"\n[evidence] wrote {out_dir/'cascade_evidence_summary.parquet'}")
    return df


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--g4output-glob", required=True)
    ap.add_argument("--px-bins", nargs="+", type=int,
                    default=[-200, -180, -160, -140, -100, 0, 100, 120, 140, 160, 180, 200])
    ap.add_argument("--target-angle-deg", type=float, default=3.0)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--max-shards", type=int, default=None)
    args = ap.parse_args()

    A = analyze(args.g4output_glob, args.px_bins, args.target_angle_deg, args.max_shards)
    report(A, args.px_bins, args.target_angle_deg, Path(args.out_dir))


if __name__ == "__main__":
    main()
