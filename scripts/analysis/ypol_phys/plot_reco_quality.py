#!/usr/bin/env python3
"""ypol phys: extra plots
1. Per-particle (proton, neutron) × (px, py, pz) reco residual histograms
2. Truth atan2 phi vs reco atan2 phi 2D histogram (NEBULA-hit events only)
"""
from __future__ import annotations

import argparse
import csv
import math
import statistics as _s
from pathlib import Path


def safe_float(s):
    try:
        return float(s)
    except (ValueError, TypeError):
        return float("nan")


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
    args = ap.parse_args()

    csv_dir = Path(args.csv_dir)
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    cells = sorted(p for p in csv_dir.glob("*.csv"))

    # Aggregate residuals across all cells
    p_resid = {"px": [], "py": [], "pz": []}  # all events with reco proton
    n_resid = {"px": [], "py": [], "pz": []}  # NEBULA-hit events
    truth_phi_vs_reco = []  # (truth_phi, reco_phi) NEBULA-hit only

    for cp in cells:
        with open(cp) as f:
            for r in csv.DictReader(f):
                if r["truth_has_proton"] != "1" or r["truth_has_neutron"] != "1":
                    continue
                tpxp, tpyp, tpzp = (safe_float(r["truth_pxp"]),
                                    safe_float(r["truth_pyp"]),
                                    safe_float(r["truth_pzp"]))
                tpxn, tpyn, tpzn = (safe_float(r["truth_pxn"]),
                                    safe_float(r["truth_pyn"]),
                                    safe_float(r["truth_pzn"]))
                rpxp, rpyp, rpzp = (safe_float(r["reco_pxp"]),
                                    safe_float(r["reco_pyp"]),
                                    safe_float(r["reco_pzp"]))
                rpxn, rpyn, rpzn = (safe_float(r["reco_pxn"]),
                                    safe_float(r["reco_pyn"]),
                                    safe_float(r["reco_pzn"]))
                n_reco_p = int(safe_float(r.get("n_reco_proton", "0")))
                n_reco_n = int(safe_float(r.get("n_reco_neutrons", "0")))

                if n_reco_p > 0 and not any(math.isnan(x) for x in (rpxp, rpyp, rpzp, tpxp, tpyp, tpzp)):
                    p_resid["px"].append(rpxp - tpxp)
                    p_resid["py"].append(rpyp - tpyp)
                    p_resid["pz"].append(rpzp - tpzp)
                if (n_reco_n > 0 and rpxn != 0.0 and
                        not any(math.isnan(x) for x in (rpxn, rpyn, rpzn, tpxn, tpyn, tpzn))):
                    n_resid["px"].append(rpxn - tpxn)
                    n_resid["py"].append(rpyn - tpyn)
                    n_resid["pz"].append(rpzn - tpzn)
                    # Truth phi (truth sum) vs reco phi (reco sum)
                    sum_tx = tpxp + tpxn
                    sum_ty = tpyp + tpyn
                    sum_rx = rpxp + rpxn
                    sum_ry = rpyp + rpyn
                    if not any(math.isnan(x) for x in (sum_tx, sum_ty, sum_rx, sum_ry)):
                        tphi = math.atan2(sum_ty, sum_tx)
                        rphi = math.atan2(sum_ry, sum_rx)
                        truth_phi_vs_reco.append((tphi, rphi))

    # ---------- Figure 1: Per-particle reco residuals ----------
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.colors import LogNorm
    except ImportError:
        print("matplotlib unavailable — skipping plots")
        return

    def stats(vs):
        if not vs:
            return float("nan"), float("nan")
        med = _s.median(vs)
        sv = sorted(vs)
        half = (sv[int(0.84 * len(sv))] - sv[int(0.16 * len(sv))]) / 2 if len(sv) > 1 else 0.0
        return med, half

    fig, axes = plt.subplots(2, 3, figsize=(15, 8))
    rows = [("proton (RK)", p_resid, "C1"), ("neutron (NEBULA)", n_resid, "C0")]
    comps = ("px", "py", "pz")
    ranges = {"px": (-100, 100), "py": (-100, 100), "pz": (-200, 200)}
    for ri, (label, resid, color) in enumerate(rows):
        for ci, comp in enumerate(comps):
            ax = axes[ri, ci]
            vs = resid[comp]
            med, half = stats(vs)
            rng = ranges[comp]
            ax.hist(vs, bins=80, range=rng, histtype="stepfilled", alpha=0.5, color=color,
                    edgecolor=color)
            ax.axvline(0, color="k", linestyle=":", linewidth=0.6)
            ax.axvline(med, color="red", linestyle="--", linewidth=1,
                       label=f"median={med:+.2f}")
            ax.set_xlabel(f"reco_{comp} − truth_{comp} (MeV/c)")
            ax.set_ylabel("counts")
            ax.set_title(f"{label} {comp}: N={len(vs)}, med={med:+.2f}, half-width={half:.2f}")
            ax.legend(fontsize=8)
    plt.tight_layout()
    out_path = out_dir / "reco_residuals_per_particle.png"
    plt.savefig(out_path, dpi=120)
    plt.close()
    print(f"[plot] wrote {out_path}")

    # Print per-particle summary
    print()
    print(f"{'particle':20s} {'comp':4s} {'N':>7s} {'median':>9s} {'half-width':>12s}")
    for label, resid, _ in rows:
        for comp in comps:
            med, half = stats(resid[comp])
            print(f"{label:20s} {comp:4s} {len(resid[comp]):>7d} {med:>+9.3f} {half:>12.3f}")

    # ---------- Figure 2: truth phi vs reco phi 2D ----------
    if truth_phi_vs_reco:
        fig, axes = plt.subplots(1, 2, figsize=(13, 5))

        # Left: 2D hist
        ax = axes[0]
        tphis = [math.degrees(t[0]) for t in truth_phi_vs_reco]
        rphis = [math.degrees(t[1]) for t in truth_phi_vs_reco]
        h = ax.hist2d(tphis, rphis, bins=60, range=[[-180, 180], [-180, 180]],
                      cmin=1, norm=LogNorm(), cmap="viridis")
        fig.colorbar(h[3], ax=ax, fraction=0.046, pad=0.04)
        ax.plot([-180, 180], [-180, 180], color="red", linestyle="--", linewidth=1, label="y = x")
        ax.set_xlabel(r"truth $\phi^{\rm event} = \arctan_2(p_{y,p}^t + p_{y,n}^t,\ p_{x,p}^t + p_{x,n}^t)$ (deg)")
        ax.set_ylabel(r"reco $\phi^{\rm event}$ (same formula on reco) (deg)")
        ax.set_title(f"truth $\\phi$ vs reco $\\phi$ (N={len(truth_phi_vs_reco)} NEBULA-hit events)")
        ax.legend(fontsize=8)
        ax.set_aspect("equal")

        # Right: Δphi histogram
        ax = axes[1]
        dphis = []
        for tphi, rphi in truth_phi_vs_reco:
            d = rphi - tphi
            d = ((d + math.pi) % (2 * math.pi)) - math.pi  # wrap to [-π, π]
            dphis.append(math.degrees(d))
        med, half = stats(dphis)
        ax.hist(dphis, bins=80, range=(-180, 180), histtype="stepfilled", alpha=0.5,
                color="C2", edgecolor="C2")
        ax.axvline(0, color="k", linestyle=":", linewidth=0.6)
        ax.axvline(med, color="red", linestyle="--", linewidth=1, label=f"median={med:+.2f}°")
        ax.set_xlabel(r"$\Delta\phi$ = reco $\phi$ - truth $\phi$ (deg, wrapped to [-180, 180])")
        ax.set_ylabel("counts")
        ax.set_title(f"$\\Delta\\phi$ distribution: med={med:+.2f}°, half-width={half:.2f}°")
        ax.legend(fontsize=8)

        plt.tight_layout()
        out_path = out_dir / "truth_phi_vs_reco_phi.png"
        plt.savefig(out_path, dpi=120)
        plt.close()
        print(f"[plot] wrote {out_path}")

        # Stats
        med, half = stats(dphis)
        in_band = sum(1 for d in dphis if abs(d) < 10) / len(dphis)
        print()
        print(f"truth_phi vs reco_phi: N={len(dphis)}")
        print(f"  Δphi median={med:+.2f}°, half-width={half:.2f}°")
        print(f"  fraction with |Δphi| < 10°: {in_band:.3f}")


if __name__ == "__main__":
    main()
