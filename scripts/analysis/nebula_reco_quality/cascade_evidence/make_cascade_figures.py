"""Generate figures for the cascade evidence section of the nebula report.

Inputs: scan_summary.parquet (Δp_x, sumE, n_reco etc.), eps_geom_grid.parquet,
        the cascade evidence per-shard scan in g4output (re-scanned here for
        fDetPos.x histograms and layer ratios per px_bin).

Outputs: 3 PDFs in <out_dir>:
  - fig_cascade_layer_ratio.pdf   (L1/L2 hit ratio vs px_tgt; in-geom vs out)
  - fig_cascade_detpos_x.pdf      (fDetPos.x histograms at selected px_bins
                                   with truth-ray x marked; shows clamping)
  - fig_cascade_eps_vs_truth.pdf  (ε_det, ε_geom, predicted direct, residual
                                   excess/deficit vs px_tgt)
"""
from __future__ import annotations
import argparse, math, glob
from pathlib import Path
from collections import Counter, defaultdict

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import uproot


def truth_ray_x_at_neut(px_tgt, pz_tgt, theta_rad,
                       target_pos=(-12.488, 0.009, -1069.458), z_neut=7249.72):
    ct, st = math.cos(theta_rad), math.sin(theta_rad)
    px_lab = ct * px_tgt + st * pz_tgt
    pz_lab = -st * px_tgt + ct * pz_tgt
    if pz_lab <= 0: return float("nan")
    return target_pos[0] + (px_lab / pz_lab) * (z_neut - target_pos[2])


def scan_g4output(g4output_glob, target_px_bins, theta_rad, max_shards=8):
    """Per-px_bin: collect fDetPos.x of Neut hits and layer counts."""
    files = sorted(glob.glob(g4output_glob))
    files = [f for f in files if "shard000000" not in f][:max_shards]
    print(f"[fig] scanning {len(files)} shards", flush=True)
    A = {b: {"layer": Counter(), "detpos_x": []} for b in target_px_bins}
    for k, fname in enumerate(files):
        print(f"  [{k+1}/{len(files)}] {fname}", flush=True)
        f = uproot.open(fname)
        t = f["tree"]
        mom = t["beam/beam.fMomentum"].array(library="np")
        truth_px = np.array([m[0].member("fP").member("fX") if len(m) > 0 else np.nan for m in mom])
        truth_py = np.array([m[0].member("fP").member("fY") if len(m) > 0 else np.nan for m in mom])
        truth_pz = np.array([m[0].member("fP").member("fZ") if len(m) > 0 else np.nan for m in mom])
        ct, st = math.cos(theta_rad), math.sin(theta_rad)
        px_tgt = ct * truth_px - st * truth_pz
        py_tgt = truth_py
        px_bin = np.round(px_tgt).astype(int)
        py_bin = np.round(py_tgt).astype(int)
        ids = t["NEBULAPla/NEBULAPla.id"].array(library="np")
        layer = t["NEBULAPla/NEBULAPla.fLayer"].array(library="np")
        sublayer = t["NEBULAPla/NEBULAPla.fSubLayer"].array(library="np")
        detpos = t["NEBULAPla/NEBULAPla.fDetPos[3]"].array(library="np")
        mask = (py_bin == 0) & np.isin(px_bin, list(target_px_bins))
        for i in np.where(mask)[0]:
            b = int(px_bin[i])
            for j in range(len(ids[i])):
                bid = int(ids[i][j])
                if 1 <= bid <= 120:
                    L = int(layer[i][j]); S = int(sublayer[i][j])
                    A[b]["layer"][(L, S)] += 1
                    A[b]["detpos_x"].append(float(detpos[i][j][0]))
        f.close()
    return A


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scan-summary", required=True)
    ap.add_argument("--geom-grid", required=True)
    ap.add_argument("--g4output-glob", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--target-angle-deg", type=float, default=3.0)
    ap.add_argument("--max-shards", type=int, default=8)
    args = ap.parse_args()

    out_dir = Path(args.out_dir); out_dir.mkdir(parents=True, exist_ok=True)
    theta = math.radians(args.target_angle_deg)
    ct, st = math.cos(theta), math.sin(theta)

    # Load efficiency tables.
    scan = pd.read_parquet(args.scan_summary)
    geom = pd.read_parquet(args.geom_grid)
    scan["px_tgt"] = ct * scan.truth_px - st * scan.truth_pz
    scan["py_tgt"] = scan.truth_py
    scan["pz_tgt"] = st * scan.truth_px + ct * scan.truth_pz
    scan["px_bin"] = scan.px_tgt.round(0).astype(int)
    scan["py_bin"] = scan.py_tgt.round(0).astype(int)
    eff = (scan[scan.py_bin == 0].groupby("px_bin").agg(
        n=("truth_px", "size"),
        n_det=("nebula_sumE", lambda s: int((s > 1).sum())),
        n_reco=("n_reco_neutrons", lambda s: int((s > 0).sum())),
    ).reset_index())
    geom_avg = geom.groupby(["px", "py"]).hit.mean().reset_index().rename(columns={"hit": "eps_geom"})
    geom_py0 = geom_avg[geom_avg.py == 0][["px", "eps_geom"]].rename(columns={"px": "px_bin"})
    geom_py0["px_bin"] = geom_py0["px_bin"].round(0).astype(int)
    eff = eff.merge(geom_py0, on="px_bin", how="left")
    eff["eps_det"] = eff.n_det / eff.n
    eff["eps_reco"] = eff.n_reco / eff.n
    # Per-bar reaction probability for a neutron traversing the 4-Neut-bar stack
    # (use central-region ratio as empirical baseline):
    central = eff[(eff.px_bin.between(-100, 100)) & (eff.eps_geom == 1.0)]
    p_direct = float(central.eps_det.mean() / 1.0) if len(central) else 0.5
    eff["pred_eps_det_direct"] = eff.eps_geom * p_direct
    eff["residual"] = eff.eps_det - eff.pred_eps_det_direct
    eff = eff.sort_values("px_bin").reset_index(drop=True)

    # =========================================================================
    # FIG 1 — ε curves with direct-prediction overlay & residual
    # =========================================================================
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))
    ax1.plot(eff.px_bin, eff.eps_geom, "-o", color="#1f77b4", label=r"$\varepsilon_\mathrm{geom}$ (truth-ray)")
    ax1.plot(eff.px_bin, eff.pred_eps_det_direct, "--", color="#1f77b4", alpha=0.6,
             label=rf"$\varepsilon_\mathrm{{geom}}\times p_\mathrm{{direct}}={p_direct:.2f}$ (predicted direct)")
    ax1.plot(eff.px_bin, eff.eps_det, "-s", color="#d62728", label=r"$\varepsilon_\mathrm{det}$ (observed)")
    ax1.plot(eff.px_bin, eff.eps_reco, ":^", color="#2ca02c", label=r"$\varepsilon_\mathrm{reco}$ (observed)")
    ax1.set_xlabel("px_tgt [MeV/c]  (py=0 slice, pz pooled)")
    ax1.set_ylabel("efficiency")
    ax1.legend(fontsize=8)
    ax1.grid(alpha=0.3)
    ax1.set_title("Observed vs direct-only prediction")

    ax2.bar(eff.px_bin, eff.residual, width=15,
            color=["#d62728" if r > 0 else "#1f77b4" for r in eff.residual])
    ax2.axhline(0, color="k", lw=0.5)
    ax2.set_xlabel("px_tgt [MeV/c]")
    ax2.set_ylabel(r"$\varepsilon_\mathrm{det} - \varepsilon_\mathrm{geom}\cdot p_\mathrm{direct}$")
    ax2.set_title("Residual: red=cascade leakage, blue=absorption deficit")
    ax2.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_dir / "fig_cascade_eps_vs_truth.pdf")
    plt.close(fig)

    # =========================================================================
    # FIG 2 — Layer ratio (L1 vs L2 hits) per px_bin
    # =========================================================================
    target_bins = [-200, -180, -160, -140, -100, 0, 100, 120, 140, 160, 180, 200]
    A = scan_g4output(args.g4output_glob, target_bins, theta, args.max_shards)
    rows = []
    for b in target_bins:
        L1 = A[b]["layer"].get((1, 1), 0) + A[b]["layer"].get((1, 2), 0)
        L2 = A[b]["layer"].get((2, 1), 0) + A[b]["layer"].get((2, 2), 0)
        rows.append({"px_bin": b, "L1": L1, "L2": L2,
                     "L1_frac": L1 / (L1 + L2) if (L1 + L2) > 0 else 0.0,
                     "truth_ray_x_at_neut_pz627": truth_ray_x_at_neut(float(b), 627, theta)})
    layer_df = pd.DataFrame(rows)
    layer_df.to_csv(out_dir / "layer_ratio.csv", index=False)

    fig, ax = plt.subplots(1, 1, figsize=(7, 4.5))
    ax.plot(layer_df.px_bin, layer_df.L1_frac, "-o", color="#9467bd",
            label="L1 fraction = L1/(L1+L2)")
    ax.axhline(0.5, color="k", ls="--", alpha=0.5, label="L1=L2 (direct neutron)")
    # Shade in-geom region
    geom_in = sorted(eff[eff.eps_geom > 0].px_bin.tolist())
    if geom_in:
        ax.axvspan(geom_in[0] - 10, geom_in[-1] + 10, color="#aaffaa", alpha=0.25,
                   label="ε_geom > 0 (any pz)")
    ax.set_xlabel("px_tgt [MeV/c]")
    ax.set_ylabel("Layer-1 hit fraction")
    ax.set_title("Layer 1 dominance outside geom = secondaries stop in first slab")
    ax.legend(fontsize=8)
    ax.grid(alpha=0.3)
    ax.set_ylim(0, 1)
    fig.tight_layout()
    fig.savefig(out_dir / "fig_cascade_layer_ratio.pdf")
    plt.close(fig)

    # =========================================================================
    # FIG 3 — fDetPos.x histograms at edge vs center, with truth-ray x marked
    # =========================================================================
    selected = [-200, -140, 0, 140, 200]
    fig, axes = plt.subplots(1, len(selected), figsize=(15, 3.5), sharey=True)
    for ax, b in zip(axes, selected):
        xs = np.array(A[b]["detpos_x"])
        if len(xs):
            ax.hist(xs, bins=np.linspace(-2000, 2200, 43), color="#3d5a80")
        x_truth = truth_ray_x_at_neut(float(b), 627, theta)
        ax.axvline(x_truth, color="red", lw=2, label=f"truth ray x={x_truth:.0f} mm")
        ax.axvline(-1707.8, color="k", ls="--", alpha=0.5)
        ax.axvline(1961.8, color="k", ls="--", alpha=0.5, label="Neut bar extent")
        ax.set_title(f"px_tgt={b} MeV/c (n_hits={len(xs)})")
        ax.set_xlabel("fDetPos.x [mm]")
        ax.set_xlim(-2400, 2600)
        ax.legend(fontsize=7, loc="upper left")
    axes[0].set_ylabel("Neut hits / bin")
    fig.suptitle("Edge hits clamp at Neut extent even when truth ray is far outside",
                 fontsize=11)
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(out_dir / "fig_cascade_detpos_x.pdf")
    plt.close(fig)

    # Save evidence summary as CSV for tex include
    eff.to_csv(out_dir / "eps_summary_py0.csv", index=False)

    print(f"\n[fig] wrote 3 PDFs and 2 CSVs to {out_dir}")
    print(f"   p_direct (central baseline) = {p_direct:.4f}")


if __name__ == "__main__":
    main()
