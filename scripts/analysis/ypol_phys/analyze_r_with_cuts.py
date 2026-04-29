#!/usr/bin/env python3
"""ypol phys: apply input_ana 3 cuts (loose/mid/tight) to existing reco data,
compute R per cut for truth / reco-mixed / reco-full populations.

Cuts (mirror scripts/notebooks/input_analysis/core/cuts.py ypol_cut_*):
  loose: |pyp - pyn| < 150 AND |sum_T|^2 > 2500
  mid:   loose AND |sum_T| < 200
  tight: mid AND (pi - |phi|) < 0.2  where phi = atan2(sum_py, sum_px)

Cuts are applied on TRUTH (sum and phi from truth values). For each truth-passing
event we compute (px_p - px_n) using:
  truth: truth_pxp - truth_pxn (always)
  reco_mixed: reco_pxp - {reco_pxn if NEBULA hit, else truth_pxn}
  reco_full:  reco_pxp - reco_pxn (NEBULA-only subset)
Then R = N(Δpx > 0) / N(Δpx < 0).
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


def ypol_cuts_truth(tpxp, tpyp, tpxn, tpyn):
    sum_px = tpxp + tpxn
    sum_py = tpyp + tpyn
    sum_xy2 = sum_px * sum_px + sum_py * sum_py
    if math.isnan(sum_xy2):
        return False, False, False
    loose = abs(tpyp - tpyn) < 150 and sum_xy2 > 2500
    if not loose:
        return False, False, False
    sum_T = math.sqrt(sum_xy2)
    mid = sum_T < 200
    if not mid:
        return True, False, False
    phi = math.atan2(sum_py, sum_px)
    tight = (math.pi - abs(phi)) < 0.2
    return loose, mid, tight


def cell_decode(tag):
    target, rest = tag.split("_", 1)
    return target, rest[:4], rest[4:]


def safe_R(diffs):
    if not diffs:
        return float("nan"), 0, 0
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
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/output/cut_R_table.csv",
    )
    args = ap.parse_args()

    csv_dir = Path(args.csv_dir)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    cells = sorted(p for p in csv_dir.glob("*.csv"))
    print(f"[cuts] {len(cells)} cell CSVs found")

    # Per (cell, cut, kind) → list of Δpx
    by_key: dict = defaultdict(list)
    cell_cut_counts: dict = defaultdict(lambda: {"loose": 0, "mid": 0, "tight": 0, "raw": 0, "any_reco": 0, "full_reco": 0})

    for cp in cells:
        tag = cp.stem
        target, gamma, hel = cell_decode(tag)
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
                rpxn = safe_float(r["reco_pxn"])
                n_reco_p = int(safe_float(r.get("n_reco_proton", "0")))
                n_reco_n = int(safe_float(r.get("n_reco_neutrons", "0")))

                cell_cut_counts[(target, gamma, hel)]["raw"] += 1
                loose, mid, tight = ypol_cuts_truth(tpxp, tpyp, tpxn, tpyn)

                # Build Δpx for the three "kinds"
                truth_dpx = tpxp - tpxn
                reco_mixed_dpx = float("nan")
                reco_full_dpx = float("nan")
                if n_reco_p > 0 and not math.isnan(rpxp):
                    if n_reco_n > 0 and rpxn != 0.0 and not math.isnan(rpxn):
                        reco_mixed_dpx = rpxp - rpxn
                        reco_full_dpx = rpxp - rpxn
                    else:
                        reco_mixed_dpx = rpxp - tpxn

                for cut_name, ok in (("loose", loose), ("mid", mid), ("tight", tight)):
                    if not ok:
                        continue
                    cell_cut_counts[(target, gamma, hel)][cut_name] += 1
                    by_key[(target, gamma, hel, cut_name, "truth")].append(truth_dpx)
                    if not math.isnan(reco_mixed_dpx):
                        by_key[(target, gamma, hel, cut_name, "reco_mixed")].append(reco_mixed_dpx)
                    if not math.isnan(reco_full_dpx):
                        by_key[(target, gamma, hel, cut_name, "reco_full")].append(reco_full_dpx)

    # Build summary rows.
    targets = sorted({k[0] for k in cell_cut_counts.keys()})
    gammas = sorted({k[1] for k in cell_cut_counts.keys()})
    hels = sorted({k[2] for k in cell_cut_counts.keys()})

    rows = []
    for target in targets:
        for gamma in gammas:
            for hel in hels:
                counts = cell_cut_counts[(target, gamma, hel)]
                row = {
                    "target": target,
                    "gamma": gamma,
                    "helicity": hel,
                    "n_raw": counts["raw"],
                    "n_loose": counts["loose"],
                    "n_mid": counts["mid"],
                    "n_tight": counts["tight"],
                }
                for cut_name in ("loose", "mid", "tight"):
                    for kind in ("truth", "reco_mixed", "reco_full"):
                        diffs = by_key.get((target, gamma, hel, cut_name, kind), [])
                        R, npos, nneg = safe_R(diffs)
                        row[f"{cut_name}_{kind}_R"] = R
                        row[f"{cut_name}_{kind}_n"] = npos + nneg
                rows.append(row)

    # Write CSV.
    if rows:
        with open(out_path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print(f"[cuts] wrote {out_path}")

    # Pretty-print summary table.
    print()
    print(f"{'target':10s} {'gamma':6s} {'hel':4s} | "
          f"{'n_raw':>6s} {'n_loose':>7s} {'n_mid':>6s} {'n_tight':>7s} | "
          f"{'R_truth_loose':>14s} {'R_truth_mid':>11s} {'R_truth_tight':>14s} | "
          f"{'R_reco_loose':>13s} {'R_reco_mid':>11s} {'R_reco_tight':>13s} | "
          f"{'R_full_loose':>13s} {'R_full_mid':>11s} {'R_full_tight':>13s}")
    for r in rows:
        def f3(k):
            v = r.get(k)
            return f"{v:.3f}" if v is not None and not math.isnan(v) else "nan"
        print(
            f"{r['target']:10s} {r['gamma']:6s} {r['helicity']:4s} | "
            f"{r['n_raw']:>6d} {r['n_loose']:>7d} {r['n_mid']:>6d} {r['n_tight']:>7d} | "
            f"{f3('loose_truth_R'):>14s} {f3('mid_truth_R'):>11s} {f3('tight_truth_R'):>14s} | "
            f"{f3('loose_reco_mixed_R'):>13s} {f3('mid_reco_mixed_R'):>11s} {f3('tight_reco_mixed_R'):>13s} | "
            f"{f3('loose_reco_full_R'):>13s} {f3('mid_reco_full_R'):>11s} {f3('tight_reco_full_R'):>13s}"
        )


def plot_r_vs_gamma_per_cut(rows, output_dir):
    """Plot R vs gamma for each cut, comparing truth/reco/reco_full."""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        return
    from pathlib import Path
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    targets = sorted({r["target"] for r in rows})
    hels = sorted({r["helicity"] for r in rows})

    for cut_name in ("loose", "mid", "tight"):
        fig, axes = plt.subplots(1, len(targets), figsize=(6 * len(targets), 5), squeeze=False)
        for ti, target in enumerate(targets):
            ax = axes[0, ti]
            for hel in hels:
                xs, truth_R, reco_R, full_R = [], [], [], []
                for r in rows:
                    if r["target"] != target or r["helicity"] != hel:
                        continue
                    g = r["gamma"]
                    xs.append(int(g[1:]) / 100)  # g050 -> 0.50
                    truth_R.append(r.get(f"{cut_name}_truth_R", float("nan")))
                    reco_R.append(r.get(f"{cut_name}_reco_mixed_R", float("nan")))
                    full_R.append(r.get(f"{cut_name}_reco_full_R", float("nan")))
                idx = sorted(range(len(xs)), key=lambda i: xs[i])
                xs = [xs[i] for i in idx]
                truth_R = [truth_R[i] for i in idx]
                reco_R = [reco_R[i] for i in idx]
                full_R = [full_R[i] for i in idx]
                ax.plot(xs, truth_R, "o-", label=f"{hel} truth", linewidth=1.6)
                ax.plot(xs, reco_R, "s--", label=f"{hel} reco", linewidth=1.4)
                ax.plot(xs, full_R, "^:", label=f"{hel} reco_full", linewidth=1.2)
            ax.set_xlabel("gamma")
            ax.set_ylabel(f"R = N(Δpx>0)/N(Δpx<0)  [{cut_name} cut]")
            ax.set_title(target)
            ax.axhline(1.0, color="gray", linestyle=":", linewidth=0.6)
            ax.legend(fontsize=8)
            ax.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_dir / f"r_vs_gamma_{cut_name}.png", dpi=120)
        plt.close()
        print(f"[cuts] wrote r_vs_gamma_{cut_name}.png")


if __name__ == "__main__":
    main()
    # Also produce per-cut R-vs-gamma plots.
    import csv as _csv
    out_csv = "/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/output/cut_R_table.csv"
    out_dir = "/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/output"
    with open(out_csv) as f:
        rows = []
        for r in _csv.DictReader(f):
            for k, v in list(r.items()):
                if k.endswith("_R") or k.endswith("_n"):
                    try:
                        r[k] = float(v)
                    except (ValueError, TypeError):
                        r[k] = float("nan")
            rows.append(r)
    plot_r_vs_gamma_per_cut(rows, out_dir)
