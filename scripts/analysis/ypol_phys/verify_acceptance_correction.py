#!/usr/bin/env python3
"""Verify NEBULA acceptance correction: build ε(p_x_n) from truth,
weight tight+NEBULA events by 1/ε, see if R_corrected → R_truth_tight."""
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
    if abs(tpyp - tpyn) >= 150 or sum_xy2 <= 2500:
        return False
    sum_T = math.sqrt(sum_xy2)
    if sum_T >= 200:
        return False
    phi = math.atan2(sum_py, sum_px)
    return (math.pi - abs(phi)) < 0.2


def cell_decode(tag):
    target, rest = tag.split("_", 1)
    return target, rest[:4], rest[4:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--csv-dir",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/csv",
    )
    ap.add_argument(
        "--output-dir",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/output",
    )
    ap.add_argument("--bin-width", type=float, default=20.0)  # MeV/c
    ap.add_argument("--eps-min", type=float, default=0.05)    # truncation
    args = ap.parse_args()

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Aggregate ε(p_xn) from ALL cells combined (gives smooth ε map)
    bin_w = args.bin_width
    def bin_idx(pxn):
        return int(round(pxn / bin_w))

    # Per (target, gamma) tight events — helicities pooled
    by_cell: dict = defaultdict(list)
    cells = sorted(p for p in Path(args.csv_dir).glob("*.csv"))
    for cp in cells:
        tag = cp.stem
        target, gamma, hel = cell_decode(tag)  # hel ignored for keying
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
                n_reco_n = int(safe_float(r.get("n_reco_neutrons", "0")))
                neb_hit = (n_reco_n > 0 and rpxn != 0.0 and not math.isnan(rpxn))
                by_cell[(target, gamma)].append({
                    "tpxp": tpxp, "tpxn": tpxn,
                    "rpxp": rpxp, "rpxn": rpxn,
                    "neb": neb_hit,
                })

    # Build epsilon map global to all cells (better statistics)
    # ε(bin) = N_neb(bin) / N_total(bin), where bin is on truth_pxn
    eps_count = defaultdict(lambda: [0, 0])  # bin -> [n_total, n_neb]
    for events in by_cell.values():
        for e in events:
            b = bin_idx(e["tpxn"])
            eps_count[b][0] += 1
            if e["neb"]:
                eps_count[b][1] += 1

    eps_map = {b: (cnt[1] / cnt[0]) if cnt[0] > 0 else 0.0 for b, cnt in eps_count.items()}

    # Print eps map
    print(f"[eps] bin_w = {bin_w} MeV/c, truncation eps_min = {args.eps_min}")
    print(f"  bin_center  N_tight  N_neb  eps")
    for b in sorted(eps_map.keys()):
        nt, nn = eps_count[b]
        bin_cen = b * bin_w
        flag = " *" if eps_map[b] < args.eps_min else ""
        print(f"  {bin_cen:>+8.1f}    {nt:>5d}   {nn:>4d}  {eps_map[b]:>5.3f}{flag}")

    # Apply 1/eps weighting per (target, gamma), recompute R for truth/reco/full
    print()
    print("Per-(target,gamma) R values (uncorrected vs ε-corrected on truth_pxn binning):")
    print(f"{'target':10s} {'gamma':6s} | "
          f"{'R_truth_tight':>14s} {'R_full_uncorr':>15s} {'R_full_corr':>13s} | "
          f"{'N_neb':>5s} {'N_corr_kept':>12s}")
    summary_rows = []
    for key, events in sorted(by_cell.items()):
        target, gamma = key
        # Truth R on tight (no NEBULA req)
        truth_diffs = [e["tpxp"] - e["tpxn"] for e in events]
        npos = sum(1 for v in truth_diffs if v > 0)
        nneg = sum(1 for v in truth_diffs if v < 0)
        R_truth = npos / nneg if nneg > 0 else float("nan")

        # Full reco R on tight + NEBULA (no weight)
        neb_events = [e for e in events if e["neb"]]
        full_diffs = [e["rpxp"] - e["rpxn"] for e in neb_events]
        np_f = sum(1 for v in full_diffs if v > 0)
        nn_f = sum(1 for v in full_diffs if v < 0)
        R_full = np_f / nn_f if nn_f > 0 else float("nan")

        # Corrected R: weight each event by 1/ε(bin), keep only bins with ε > eps_min
        wpos, wneg, n_kept = 0.0, 0.0, 0
        for e in neb_events:
            b = bin_idx(e["tpxn"])
            eps = eps_map.get(b, 0.0)
            if eps < args.eps_min:
                continue  # truncate low-ε region
            w = 1.0 / eps
            d = e["rpxp"] - e["rpxn"]
            if d > 0:
                wpos += w
            elif d < 0:
                wneg += w
            n_kept += 1
        R_corr = wpos / wneg if wneg > 0 else float("nan")

        print(f"{target:10s} {gamma:6s} | "
              f"{R_truth:>14.3f} {R_full:>15.3f} {R_corr:>13.3f} | "
              f"{len(neb_events):>5d} {n_kept:>12d}")
        summary_rows.append({
            "target": target, "gamma": gamma,
            "R_truth_tight": R_truth, "R_full_uncorr": R_full,
            "R_full_corr": R_corr,
            "N_neb": len(neb_events), "N_corr_kept": n_kept,
        })

    # Write CSV
    out_csv = out_dir / "acceptance_correction_table.csv"
    if summary_rows:
        with open(out_csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(summary_rows[0].keys()))
            w.writeheader()
            w.writerows(summary_rows)
        print(f"\n[acc] wrote {out_csv}")

    # Aggregate: compare mean R values
    import statistics as s
    rs_truth = [r["R_truth_tight"] for r in summary_rows if not math.isnan(r["R_truth_tight"])]
    rs_full = [r["R_full_uncorr"] for r in summary_rows if not math.isnan(r["R_full_uncorr"])]
    rs_corr = [r["R_full_corr"] for r in summary_rows if not math.isnan(r["R_full_corr"])]
    print(f"\nAggregate over {len(summary_rows)} cells:")
    print(f"  mean R_truth_tight = {s.mean(rs_truth):.3f}")
    print(f"  mean R_full_uncorr = {s.mean(rs_full):.3f}")
    print(f"  mean R_full_corr   = {s.mean(rs_corr):.3f}")
    print(f"  → corrected closer to truth?  truth-corr gap = {s.mean(rs_truth)-s.mean(rs_corr):+.3f}")
    print(f"  → uncorrected was off by      truth-uncorr   = {s.mean(rs_truth)-s.mean(rs_full):+.3f}")


if __name__ == "__main__":
    main()
