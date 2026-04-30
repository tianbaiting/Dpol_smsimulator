#!/usr/bin/env python3
"""ypol phys: rotate to reaction plane (per event align sum_T to +x), then
recompute R on rot_pxp - rot_pxn. Compare lab-frame R vs reaction-plane R
under each input_ana cut (loose/mid/tight)."""
from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


def safe_float(s):
    try:
        return float(s)
    except (ValueError, TypeError):
        return float("nan")


def cell_decode(tag):
    target, rest = tag.split("_", 1)
    return target, rest[:4], rest[4:]


def ypol_cuts(pxp, pyp, pxn, pyn):
    sum_px = pxp + pxn
    sum_py = pyp + pyn
    sum_xy2 = sum_px * sum_px + sum_py * sum_py
    if math.isnan(sum_xy2):
        return False, False, False
    loose = abs(pyp - pyn) < 150 and sum_xy2 > 2500
    if not loose:
        return False, False, False
    sum_T = math.sqrt(sum_xy2)
    mid = sum_T < 200
    if not mid:
        return True, False, False
    phi = math.atan2(sum_py, sum_px)
    tight = (math.pi - abs(phi)) < 0.2
    return loose, mid, tight


def rotate_to_reaction_plane(pxp, pyp, pxn, pyn):
    """Apply R_z(-phi) so the sum_T vector lies along +x.
    Returns (rot_pxp, rot_pyp, rot_pxn, rot_pyn)."""
    sum_px = pxp + pxn
    sum_py = pyp + pyn
    if sum_px == 0 and sum_py == 0:
        return pxp, pyp, pxn, pyn
    phi = math.atan2(sum_py, sum_px)
    angle = -phi
    cos_a = math.cos(angle)
    sin_a = math.sin(angle)
    return (
        cos_a * pxp - sin_a * pyp,
        sin_a * pxp + cos_a * pyp,
        cos_a * pxn - sin_a * pyn,
        sin_a * pxn + cos_a * pyn,
    )


def safe_R(diffs):
    if not diffs:
        return float("nan")
    n_pos = sum(1 for v in diffs if v > 0)
    n_neg = sum(1 for v in diffs if v < 0)
    return n_pos / n_neg if n_neg > 0 else float("nan")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--csv-dir",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/csv",
    )
    ap.add_argument(
        "--output",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/output/reaction_plane_R_table.csv",
    )
    args = ap.parse_args()

    csv_dir = Path(args.csv_dir)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    cells = sorted(p for p in csv_dir.glob("*.csv"))
    rows = []

    for cp in cells:
        tag = cp.stem
        target, gamma, hel = cell_decode(tag)
        cell_diffs: dict = defaultdict(list)
        with open(cp) as f:
            for r in csv.DictReader(f):
                if r["truth_has_proton"] != "1" or r["truth_has_neutron"] != "1":
                    continue
                tpxp = safe_float(r["truth_pxp"])
                tpyp = safe_float(r["truth_pyp"])
                tpxn = safe_float(r["truth_pxn"])
                tpyn = safe_float(r["truth_pyn"])
                if any(math.isnan(x) for x in (tpxp, tpyp, tpxn, tpyn)):
                    continue

                rpxp = safe_float(r["reco_pxp"])
                rpyp = safe_float(r["reco_pyp"])
                rpxn = safe_float(r["reco_pxn"])
                rpyn = safe_float(r["reco_pyn"])
                n_reco_p = int(safe_float(r.get("n_reco_proton", "0")))
                n_reco_n = int(safe_float(r.get("n_reco_neutrons", "0")))
                neb_hit = (n_reco_n > 0 and rpxn != 0.0 and not math.isnan(rpxn))

                cuts = ypol_cuts(tpxp, tpyp, tpxn, tpyn)
                # truth lab-frame Δpx
                truth_dpx_lab = tpxp - tpxn
                # truth reaction-plane rotated Δpx
                trxp, tryp, trxn, tryn = rotate_to_reaction_plane(tpxp, tpyp, tpxn, tpyn)
                truth_dpx_rot = trxp - trxn
                # reco lab-frame
                if n_reco_p > 0:
                    if neb_hit:
                        reco_dpx_lab = rpxp - rpxn
                        rrxp, rryp, rrxn, rryn = rotate_to_reaction_plane(rpxp, rpyp, rpxn, rpyn)
                        reco_dpx_rot = rrxp - rrxn
                        # Also: reaction plane defined by truth (more stable)
                        rrxp_t, _, rrxn_t, _ = rotate_to_reaction_plane_using_phi(rpxp, rpyp, rpxn, rpyn, math.atan2(tpyp+tpyn, tpxp+tpxn))
                        reco_dpx_rot_truthphi = rrxp_t - rrxn_t
                    else:
                        reco_dpx_lab = rpxp - tpxn
                        rrxp, rryp, rrxn, rryn = rotate_to_reaction_plane(rpxp, rpyp, tpxn, tpyn)
                        reco_dpx_rot = rrxp - rrxn
                        rrxp_t, _, rrxn_t, _ = rotate_to_reaction_plane_using_phi(rpxp, rpyp, tpxn, tpyn, math.atan2(tpyp+tpyn, tpxp+tpxn))
                        reco_dpx_rot_truthphi = rrxp_t - rrxn_t
                else:
                    reco_dpx_lab = float("nan")
                    reco_dpx_rot = float("nan")
                    reco_dpx_rot_truthphi = float("nan")

                for cn, ok in zip(("loose","mid","tight"), cuts):
                    if not ok:
                        continue
                    cell_diffs[(cn, "truth_lab")].append(truth_dpx_lab)
                    cell_diffs[(cn, "truth_rot")].append(truth_dpx_rot)
                    if not math.isnan(reco_dpx_lab):
                        cell_diffs[(cn, "reco_lab")].append(reco_dpx_lab)
                        cell_diffs[(cn, "reco_rot")].append(reco_dpx_rot)
                        cell_diffs[(cn, "reco_rot_truthphi")].append(reco_dpx_rot_truthphi)

        row = {"target": target, "gamma": gamma, "helicity": hel}
        for cn in ("loose","mid","tight"):
            row[f"n_{cn}"] = len(cell_diffs[(cn, "truth_lab")])
            for kind in ("truth_lab","truth_rot","reco_lab","reco_rot","reco_rot_truthphi"):
                row[f"R_{cn}_{kind}"] = safe_R(cell_diffs[(cn, kind)])
        rows.append(row)

    # Write CSV
    if rows:
        with open(out_path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print(f"[rp] wrote {out_path}")

    # Pretty-print tight comparison
    print()
    print(f"{'target':10s} {'gamma':6s} {'hel':4s} | {'N_tight':>7s} | "
          f"{'R_tight_truth_lab':>18s} {'R_tight_truth_rot':>18s} | "
          f"{'R_tight_reco_lab':>17s} {'R_tight_reco_rot':>17s} {'R_tight_reco_rot_tphi':>22s}")
    for r in rows:
        def f3(v):
            return f"{v:.3f}" if v is not None and not (isinstance(v, float) and math.isnan(v)) else "nan"
        print(f"{r['target']:10s} {r['gamma']:6s} {r['helicity']:4s} | {r['n_tight']:>7d} | "
              f"{f3(r['R_tight_truth_lab']):>18s} {f3(r['R_tight_truth_rot']):>18s} | "
              f"{f3(r['R_tight_reco_lab']):>17s} {f3(r['R_tight_reco_rot']):>17s} "
              f"{f3(r['R_tight_reco_rot_truthphi']):>22s}")

    # Aggregate stats
    import statistics as s
    rs_tl = [r['R_tight_truth_lab'] for r in rows if not math.isnan(r['R_tight_truth_lab'])]
    rs_tr = [r['R_tight_truth_rot'] for r in rows if not math.isnan(r['R_tight_truth_rot'])]
    rs_rl = [r['R_tight_reco_lab'] for r in rows if not math.isnan(r['R_tight_reco_lab'])]
    rs_rr = [r['R_tight_reco_rot'] for r in rows if not math.isnan(r['R_tight_reco_rot'])]
    rs_rt = [r['R_tight_reco_rot_truthphi'] for r in rows if not math.isnan(r['R_tight_reco_rot_truthphi'])]
    print(f"\nMean over {len(rows)} cells (tight cut):")
    print(f"  R_truth_lab          = {s.mean(rs_tl):.3f}")
    print(f"  R_truth_rot          = {s.mean(rs_tr):.3f}  ← reaction-plane rotation")
    print(f"  R_reco_lab           = {s.mean(rs_rl):.3f}")
    print(f"  R_reco_rot (recoT)   = {s.mean(rs_rr):.3f}  ← rotate via reco sum_T")
    print(f"  R_reco_rot (truthT)  = {s.mean(rs_rt):.3f}  ← rotate via truth sum_T (clean test)")


def rotate_to_reaction_plane_using_phi(pxp, pyp, pxn, pyn, phi):
    """Rotate by -phi (phi is given externally, e.g. truth)."""
    angle = -phi
    cos_a = math.cos(angle)
    sin_a = math.sin(angle)
    return (
        cos_a * pxp - sin_a * pyp,
        sin_a * pxp + cos_a * pyp,
        cos_a * pxn - sin_a * pyn,
        sin_a * pxn + cos_a * pyn,
    )


if __name__ == "__main__":
    main()
