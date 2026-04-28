#!/usr/bin/env python3
"""ypol elastic phys pilot — physics observable analysis.

Reads per-cell CSVs produced by `extract_phys_observables.cc` and computes:
  - (px_p - px_n) 1D histograms (truth vs reco, per cell)
  - 2D truth-vs-reco scatter histograms
  - asymmetry table: median, robust half-width, R = N(>0)/N(<0)
  - R vs gamma trend (per target, per helicity)
  - Side-by-side truth/reco plots
"""
from __future__ import annotations

import argparse
import csv
import math
import os
import statistics
from collections import defaultdict
from pathlib import Path

import numpy as np


def load_csv(p: Path) -> list[dict]:
    with open(p) as f:
        return list(csv.DictReader(f))


def safe_float(s):
    try:
        return float(s)
    except (ValueError, TypeError):
        return float("nan")


def cell_decode(tag: str) -> tuple[str, str, str]:
    # tag = e.g. "Sn124E190_g050ynp"
    # parts: target = before "_"; gamma = next 4 chars; helicity = remaining 3 chars
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
    args = ap.parse_args()

    csv_dir = Path(args.csv_dir)
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    cells = sorted(p for p in csv_dir.glob("*.csv"))
    print(f"[analyze] {len(cells)} cell CSVs found")

    # Aggregate.
    by_cell: dict[tuple, list[dict]] = {}
    for cp in cells:
        tag = cp.stem
        rows = load_csv(cp)
        target, gamma, hel = cell_decode(tag)
        by_cell[(target, gamma, hel)] = rows
        print(f"  [{tag}] N = {len(rows)} (target={target}, gamma={gamma}, helicity={hel})")

    # Compute per-cell stats: truth (px_p - px_n) median + reco; truth_R = N_pos/N_neg etc.
    # First-pass: reco_neutron deferred (rootmap library mismatch in extract macro).
    # Use truth_neutron as a perfect-neutron proxy. Reco side = reco_proton + truth_neutron.
    summary_rows = []
    for (target, gamma, hel), rows in by_cell.items():
        truth_diffs = []
        reco_diffs = []
        truth_pyp_minus_pyn = []
        reco_pyp_minus_pyn = []
        for r in rows:
            if r["truth_has_proton"] != "1" or r["truth_has_neutron"] != "1":
                continue
            tpxp, tpxn = safe_float(r["truth_pxp"]), safe_float(r["truth_pxn"])
            tpyp, tpyn = safe_float(r["truth_pyp"]), safe_float(r["truth_pyn"])
            rpxp = safe_float(r["reco_pxp"])
            rpyp = safe_float(r["reco_pyp"])
            n_reco_p = int(safe_float(r["n_reco_proton"])) if "n_reco_proton" in r else 0
            if any(math.isnan(x) for x in (tpxp, tpxn, tpyp, tpyn)):
                continue
            truth_diffs.append(tpxp - tpxn)
            truth_pyp_minus_pyn.append(tpyp - tpyn)
            # Reco-mixed: reco_proton + truth_neutron (truth_neutron used as perfect proxy
            # since reco_neutron extraction is deferred).
            if n_reco_p > 0 and not (math.isnan(rpxp) or math.isnan(rpyp)):
                reco_diffs.append(rpxp - tpxn)         # reco_pxp - truth_pxn
                reco_pyp_minus_pyn.append(rpyp - tpyn) # reco_pyp - truth_pyn

        n = len(truth_diffs)
        n_reco = len(reco_diffs)

        def safe_stats(vals):
            if not vals:
                return {"n": 0, "median": float("nan"), "halfwidth": float("nan"), "n_pos": 0, "n_neg": 0, "R": float("nan")}
            vs = sorted(vals)
            med = statistics.median(vs)
            half = (vs[int(0.84 * len(vs))] - vs[int(0.16 * len(vs))]) / 2 if len(vs) > 1 else 0.0
            n_pos = sum(1 for v in vs if v > 0)
            n_neg = sum(1 for v in vs if v < 0)
            R = n_pos / n_neg if n_neg > 0 else float("nan")
            return {"n": len(vs), "median": med, "halfwidth": half, "n_pos": n_pos, "n_neg": n_neg, "R": R}

        truth_x = safe_stats(truth_diffs)
        reco_x = safe_stats(reco_diffs)
        truth_y = safe_stats(truth_pyp_minus_pyn)
        reco_y = safe_stats(reco_pyp_minus_pyn)
        summary_rows.append({
            "target": target,
            "gamma": gamma,
            "helicity": hel,
            "n_truth": n,
            "n_reco_pair": n_reco,
            "truth_pxp_minus_pxn_median": truth_x["median"],
            "truth_pxp_minus_pxn_halfwidth": truth_x["halfwidth"],
            "truth_R_x": truth_x["R"],
            "reco_pxp_minus_pxn_median": reco_x["median"],
            "reco_pxp_minus_pxn_halfwidth": reco_x["halfwidth"],
            "reco_R_x": reco_x["R"],
            "truth_pyp_minus_pyn_median": truth_y["median"],
            "reco_pyp_minus_pyn_median": reco_y["median"],
            "truth_R_y": truth_y["R"],
            "reco_R_y": reco_y["R"],
        })

    # Write summary CSV.
    if summary_rows:
        with open(out_dir / "asymmetry_table.csv", "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(summary_rows[0].keys()))
            w.writeheader()
            w.writerows(summary_rows)
        print(f"[analyze] wrote {out_dir / 'asymmetry_table.csv'}")

    # Plotting.
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.colors import LogNorm
    except ImportError:
        print("[analyze] matplotlib unavailable; skipping plots")
        return

    # Figure 1: (truth px_p - px_n) and (reco px_p - px_n) overlay histograms, per target.
    targets = sorted(set(t for (t, _, _) in by_cell.keys()))
    gammas = sorted(set(g for (_, g, _) in by_cell.keys()))
    hels = sorted(set(h for (_, _, h) in by_cell.keys()))

    for target in targets:
        fig, axes = plt.subplots(len(gammas), len(hels), figsize=(5 * len(hels), 4 * len(gammas)), squeeze=False)
        for gi, gamma in enumerate(gammas):
            for hi, hel in enumerate(hels):
                ax = axes[gi, hi]
                rows = by_cell.get((target, gamma, hel), [])
                tvals, rvals = [], []
                for r in rows:
                    if r["truth_has_proton"] != "1" or r["truth_has_neutron"] != "1":
                        continue
                    tpxp, tpxn = safe_float(r["truth_pxp"]), safe_float(r["truth_pxn"])
                    rpxp = safe_float(r["reco_pxp"])
                    n_reco_p = int(safe_float(r["n_reco_proton"])) if "n_reco_proton" in r else 0
                    if not any(math.isnan(x) for x in (tpxp, tpxn)):
                        tvals.append(tpxp - tpxn)
                    # Reco-mixed: reco_proton + truth_neutron
                    if n_reco_p > 0 and not math.isnan(rpxp) and not math.isnan(tpxn):
                        rvals.append(rpxp - tpxn)
                rng = (-500, 500)
                bins = 60
                ax.hist(tvals, bins=bins, range=rng, histtype="step", color="black", linewidth=1.4, label=f"truth (N={len(tvals)})")
                if rvals:
                    ax.hist(rvals, bins=bins, range=rng, histtype="stepfilled", alpha=0.4, color="C1", label=f"reco (N={len(rvals)})")
                ax.set_xlabel("px_p - px_n (MeV/c)")
                ax.set_ylabel("counts")
                ax.legend(fontsize=7)
                ax.set_title(f"{target} {gamma} {hel}")
                ax.axvline(0, color="gray", linestyle=":", linewidth=0.6)
        plt.tight_layout()
        plt.savefig(out_dir / f"px_diff_hist_{target}.png", dpi=120)
        plt.close()
        print(f"[analyze] wrote px_diff_hist_{target}.png")

    # Figure 2: truth vs reco 2D histogram of (px_p - px_n) per cell.
    fig, axes = plt.subplots(len(targets), len(gammas) * len(hels), figsize=(4 * len(gammas) * len(hels), 4 * len(targets)), squeeze=False)
    for ti, target in enumerate(targets):
        col = 0
        for gamma in gammas:
            for hel in hels:
                ax = axes[ti, col]
                rows = by_cell.get((target, gamma, hel), [])
                xs, ys = [], []
                for r in rows:
                    if r["truth_has_proton"] != "1" or r["truth_has_neutron"] != "1":
                        continue
                    tpxp, tpxn = safe_float(r["truth_pxp"]), safe_float(r["truth_pxn"])
                    rpxp = safe_float(r["reco_pxp"])
                    n_reco_p = int(safe_float(r["n_reco_proton"])) if "n_reco_proton" in r else 0
                    if any(math.isnan(x) for x in (tpxp, tpxn, rpxp)) or n_reco_p == 0:
                        continue
                    xs.append(tpxp - tpxn)
                    ys.append(rpxp - tpxn)  # reco_pxp - truth_pxn (mixed)
                if xs:
                    h = ax.hist2d(xs, ys, bins=40, range=[[-500, 500], [-500, 500]], cmin=1, norm=LogNorm(), cmap="viridis")
                    fig.colorbar(h[3], ax=ax, fraction=0.046, pad=0.04)
                    ax.plot([-500, 500], [-500, 500], color="red", linestyle="--", linewidth=0.8)
                ax.set_xlabel(f"truth Δpx")
                ax.set_ylabel(f"reco Δpx")
                ax.set_aspect("equal", adjustable="box")
                ax.set_title(f"{target} {gamma} {hel}")
                col += 1
    plt.tight_layout()
    plt.savefig(out_dir / "px_diff_truth_vs_reco_hist2d.png", dpi=120)
    plt.close()
    print(f"[analyze] wrote px_diff_truth_vs_reco_hist2d.png")

    # Figure 3: R vs gamma curve per (target, helicity).
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    for ti, target in enumerate(targets):
        ax = axes[ti]
        for hel in hels:
            xs = []
            truth_R = []
            reco_R = []
            for gamma in gammas:
                row = next((r for r in summary_rows if r["target"] == target and r["gamma"] == gamma and r["helicity"] == hel), None)
                if row is None:
                    continue
                xs.append(int(gamma[1:]) / 100)  # g050 -> 0.50
                truth_R.append(row["truth_R_x"])
                reco_R.append(row["reco_R_x"])
            ax.plot(xs, truth_R, "o-", label=f"{hel} truth", linewidth=1.5)
            ax.plot(xs, reco_R, "s--", label=f"{hel} reco", linewidth=1.5)
        ax.set_xlabel("gamma")
        ax.set_ylabel("R = N(Δpx>0) / N(Δpx<0)")
        ax.set_title(target)
        ax.axhline(1.0, color="gray", linestyle=":", linewidth=0.6)
        ax.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(out_dir / "r_vs_gamma.png", dpi=120)
    plt.close()
    print(f"[analyze] wrote r_vs_gamma.png")

    # Print summary table.
    print()
    print("=== Asymmetry summary ===")
    hdr_keys = ["target", "gamma", "helicity", "n_truth", "n_reco_pair",
                "truth_pxp_minus_pxn_median", "reco_pxp_minus_pxn_median",
                "truth_R_x", "reco_R_x"]
    print(",".join(hdr_keys))
    for r in summary_rows:
        print(",".join(f"{r[k]:.3f}" if isinstance(r[k], float) else str(r[k]) for k in hdr_keys))


if __name__ == "__main__":
    main()
