#!/usr/bin/env python3
"""ypol phys: proton vs neutron reco bottleneck analysis.

For each cell, restrict to the NEBULA-hit subset and compute (px_p - px_n) R for
4 combinations on the same events to isolate proton vs neutron smearing:
  A: truth_p - truth_n  (subset baseline)
  B: reco_p  - truth_n  (proton-only smear)
  C: truth_p - reco_n   (neutron-only smear)
  D: reco_p  - reco_n   (both smeared, = current reco_full)

Apply the input_ana tight cut (truth-based) so we're in the polarization-relevant
selection. Compare A→B (proton bottleneck size) and A→C (neutron bottleneck size).
"""
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


def ypol_tight_truth(tpxp, tpyp, tpxn, tpyn):
    sum_px = tpxp + tpxn
    sum_py = tpyp + tpyn
    sum_xy2 = sum_px * sum_px + sum_py * sum_py
    if math.isnan(sum_xy2):
        return False
    if abs(tpyp - tpyn) >= 150:
        return False
    if sum_xy2 <= 2500:
        return False
    sum_T = math.sqrt(sum_xy2)
    if sum_T >= 200:
        return False
    phi = math.atan2(sum_py, sum_px)
    return (math.pi - abs(phi)) < 0.2


def cell_decode(tag):
    target, rest = tag.split("_", 1)
    return target, rest[:4], rest[4:]


def safe_R(diffs):
    n_pos = sum(1 for v in diffs if v > 0)
    n_neg = sum(1 for v in diffs if v < 0)
    R = n_pos / n_neg if n_neg > 0 else float("nan")
    return R, n_pos, n_neg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--csv-dir",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/csv",
    )
    ap.add_argument(
        "--output",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/output/bottleneck_table.csv",
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
        A_diffs, B_diffs, C_diffs, D_diffs = [], [], [], []
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
                if not ypol_tight_truth(tpxp, tpyp, tpxn, tpyn):
                    continue

                rpxp = safe_float(r["reco_pxp"])
                rpxn = safe_float(r["reco_pxn"])
                n_reco_p = int(safe_float(r.get("n_reco_proton", "0")))
                n_reco_n = int(safe_float(r.get("n_reco_neutrons", "0")))

                # Restrict to NEBULA-hit subset (where C and D are well defined)
                if n_reco_p == 0 or n_reco_n == 0 or rpxn == 0.0 or math.isnan(rpxn) or math.isnan(rpxp):
                    continue

                A_diffs.append(tpxp - tpxn)         # subset baseline
                B_diffs.append(rpxp - tpxn)         # proton smear only
                C_diffs.append(tpxp - rpxn)         # neutron smear only
                D_diffs.append(rpxp - rpxn)         # both

        n_subset = len(A_diffs)
        if n_subset == 0:
            continue
        RA, _, _ = safe_R(A_diffs)
        RB, _, _ = safe_R(B_diffs)
        RC, _, _ = safe_R(C_diffs)
        RD, _, _ = safe_R(D_diffs)

        # Median residuals to characterize smearing direction
        def med(diffs):
            return sorted(diffs)[len(diffs)//2] if diffs else float("nan")
        med_A = med(A_diffs)
        med_B = med(B_diffs)
        med_C = med(C_diffs)
        med_D = med(D_diffs)

        rows.append({
            "target": target,
            "gamma": gamma,
            "helicity": hel,
            "n_tight_neb": n_subset,
            "R_A_truth": RA,
            "R_B_p_smear": RB,
            "R_C_n_smear": RC,
            "R_D_full": RD,
            "med_A": med_A,
            "med_B": med_B,
            "med_C": med_C,
            "med_D": med_D,
            "dR_proton": RB - RA,
            "dR_neutron": RC - RA,
            "dR_full": RD - RA,
        })

    if rows:
        with open(out_path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print(f"[bottleneck] wrote {out_path}")

    # Pretty-print
    print()
    print('{:10s} {:6s} {:4s} | {:>4s} | {:>10s} {:>7s} {:>7s} {:>9s} | {:>7s} {:>7s} {:>9s}'.format(
        'target','gamma','hel','N','R_A_truth','R_B_p','R_C_n','R_D_full','dR_p','dR_n','dR_both'))
    print("-" * 110)
    for r in rows:
        def f3(v):
            return f"{v:+.3f}" if v is not None and not math.isnan(v) else "nan"
        def f3p(v):
            return f"{v:.3f}" if v is not None and not math.isnan(v) else "nan"
        print(
            f"{r['target']:10s} {r['gamma']:6s} {r['helicity']:4s} | {r['n_tight_neb']:>4d} | "
            f"{f3p(r['R_A_truth']):>10s} {f3p(r['R_B_p_smear']):>7s} {f3p(r['R_C_n_smear']):>7s} "
            f"{f3p(r['R_D_full']):>9s} | "
            f"{f3(r['dR_proton']):>7s} {f3(r['dR_neutron']):>7s} {f3(r['dR_full']):>9s}"
        )

    # Summary statistics
    print()
    import statistics as s
    dR_p = [r['dR_proton'] for r in rows if not math.isnan(r['dR_proton'])]
    dR_n = [r['dR_neutron'] for r in rows if not math.isnan(r['dR_neutron'])]
    dR_full = [r['dR_full'] for r in rows if not math.isnan(r['dR_full'])]
    print(f"Across {len(rows)} cells (NEBULA-hit + tight subset, mean N ~ {s.mean([r['n_tight_neb'] for r in rows]):.0f}):")
    print(f"  ΔR_proton  (B − A) = {s.mean(dR_p):+.3f} ± {s.stdev(dR_p):.3f}  ← proton smearing on R")
    print(f"  ΔR_neutron (C − A) = {s.mean(dR_n):+.3f} ± {s.stdev(dR_n):.3f}  ← neutron smearing on R")
    print(f"  ΔR_full    (D − A) = {s.mean(dR_full):+.3f} ± {s.stdev(dR_full):.3f}")
    print()
    if abs(s.mean(dR_n)) > abs(s.mean(dR_p)):
        ratio = abs(s.mean(dR_n)) / max(abs(s.mean(dR_p)), 1e-6)
        print(f"  → Neutron reco is the dominant bottleneck ({ratio:.1f}× larger ΔR than proton)")
    else:
        ratio = abs(s.mean(dR_p)) / max(abs(s.mean(dR_n)), 1e-6)
        print(f"  → Proton reco is the dominant bottleneck ({ratio:.1f}× larger ΔR than neutron)")


if __name__ == "__main__":
    main()
