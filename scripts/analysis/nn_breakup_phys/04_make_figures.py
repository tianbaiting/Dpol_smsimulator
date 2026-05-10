#!/usr/bin/env python3
"""Render the figures used in nn_breakup_phys_20260503_zh.tex.

Inputs (one polarization at a time):
  build/test_output/nn_breakup_phys_20260503/summary/<pol>/joined.csv.gz
  build/test_output/nn_breakup_phys_20260503/summary/<pol>/r_table.csv

Outputs (per polarization):
  docs/reports/.../figures/nn_breakup_phys/residuals_per_particle_<pol>.png
  docs/reports/.../figures/nn_breakup_phys/px_diff_hist_<pol>_<cut>.png  (one per cut)
  docs/reports/.../figures/nn_breakup_phys/r_vs_gamma_<pol>_<cut>.png    (one per cut)
"""
from __future__ import annotations
import argparse, math
from pathlib import Path
import numpy as np, pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

def half_width(x):
    if len(x) < 5: return float("nan")
    p84, p16 = np.percentile(x, [84.135, 15.865])
    return 0.5 * (p84 - p16)

def add_rot_neutron(df):
    cos = np.cos(-df.phi_event); sin = np.sin(-df.phi_event)
    df["truth_rot_pyp"] = sin * df.truth_pxp + cos * df.truth_pyp
    df["truth_rot_pyn"] = sin * df.truth_pxn + cos * df.truth_pyn
    return df

def plot_residuals_per_particle(df, out_path, pol_label):
    # Proton: NN reco − truth; Neutron: reco (β method) − truth, NEBULA-hit only.
    fig, axes = plt.subplots(2, 3, figsize=(13, 7))
    components = [("px", "truth_pxp", "nn_pxp"),
                  ("py", "truth_pyp", "nn_pyp"),
                  ("pz", "truth_pzp", "nn_pzp")]
    for c, (label, t, r) in enumerate(components):
        ax = axes[0, c]
        diff = (df[r] - df[t]).values
        ax.hist(diff, bins=120, range=(-30, 30), histtype="step", color="C3", lw=1.5)
        med = float(np.median(diff)); hw = half_width(diff)
        ax.axvline(med, color="k", ls="--", lw=0.8)
        ax.set_title(f"proton (NN) Δ{label}\nN={len(diff)}, med={med:+.2f}, hw={hw:.2f}", fontsize=10)
        ax.set_xlabel("reco − truth (MeV/c)"); ax.grid(alpha=0.3)
    nebula = df[df.n_reco_neutrons > 0]
    n_components = [("px", "truth_pxn", "reco_pxn"),
                    ("py", "truth_pyn", "reco_pyn"),
                    ("pz", "truth_pzn", "reco_pzn")]
    for c, (label, t, r) in enumerate(n_components):
        ax = axes[1, c]
        diff = (nebula[r] - nebula[t]).values
        ax.hist(diff, bins=120, range=(-50, 50), histtype="step", color="C0", lw=1.5)
        med = float(np.median(diff)); hw = half_width(diff)
        ax.axvline(med, color="k", ls="--", lw=0.8)
        ax.set_title(f"neutron (NEBULA) Δ{label}\nN={len(diff)}, med={med:+.2f}, hw={hw:.2f}", fontsize=10)
        ax.set_xlabel("reco − truth (MeV/c)"); ax.grid(alpha=0.3)
    fig.suptitle(f"{pol_label}: per-particle residuals (NN proton + NEBULA neutron)", fontsize=11)
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    fig.savefig(out_path, dpi=130); plt.close(fig)

def plot_px_diff_hist(df, cut, out_path, pol_label):
    targets = sorted(df.target.unique())
    gammas = sorted(df.gamma.unique())
    fig, axes = plt.subplots(len(targets), len(gammas), figsize=(4*len(gammas), 3*len(targets)), sharex=True, sharey=False)
    if len(targets) == 1: axes = np.array([axes])
    if len(gammas)  == 1: axes = axes[:, None]
    for i, tgt in enumerate(targets):
        for j, gam in enumerate(gammas):
            sub = df[(df.target == tgt) & (df.gamma == gam) & df[cut]]
            ax = axes[i, j]
            if len(sub) == 0:
                ax.set_title(f"{tgt} {gam} (empty)"); continue
            tdpx = (sub.truth_rot_pxp - sub.truth_rot_pxn).values
            mdpx_truth_n = sub.truth_rot_pxn.where(~(sub.n_reco_neutrons > 0), sub.reco_rot_pxn)
            mdpx = (sub.nn_rot_pxp - mdpx_truth_n).values
            full = sub[sub.n_reco_neutrons > 0]
            fdpx = (full.nn_rot_pxp - full.reco_rot_pxn).values
            ax.hist(tdpx, bins=80, range=(-300, 300), histtype="step", color="k", lw=1.4, label="truth")
            ax.hist(mdpx, bins=80, range=(-300, 300), histtype="step", color="C1", lw=1.2, label="NN mixed")
            if len(fdpx) > 0:
                ax.hist(fdpx, bins=80, range=(-300, 300), histtype="step", color="C0", lw=1.2, ls="--", label="NN full")
            ax.set_title(f"{tgt} {gam} (N={len(sub)})", fontsize=9)
            ax.grid(alpha=0.3)
            if i == 0 and j == 0: ax.legend(fontsize=8)
            if i == len(targets) - 1: ax.set_xlabel("Δp_x rot (MeV/c)")
    fig.suptitle(f"{pol_label} Δp_x rot ({cut} cut, helicities pooled)", fontsize=11)
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    fig.savefig(out_path, dpi=130); plt.close(fig)

def plot_r_vs_gamma(r_df, cut, out_path, pol_label):
    # ypol: Sn112 and Sn124 R values are similar magnitude → sharey aids comparison.
    # zpol: Sn112 stays in [0.05, 0.4] while Sn124 reaches 3.6 → forcing sharey
    # squashes Sn112 against the bottom. Use independent y-axes for zpol.
    share = "all" if pol_label.startswith("ypol") else False
    targets = sorted(r_df.target.unique())
    fig, axes = plt.subplots(1, len(targets), figsize=(6*len(targets), 4), sharey=share)
    if len(targets) == 1: axes = [axes]
    for i, tgt in enumerate(targets):
        ax = axes[i]
        sub = r_df[(r_df.target == tgt) & (r_df.cut == cut)]
        for variant, color, marker, ls in [("truth", "k", "o", "-"),
                                            ("mixed", "C1", "s", "--"),
                                            ("full",  "C0", "^", ":")]:
            v = sub[sub.variant == variant].sort_values("gamma")
            if len(v) == 0: continue
            yerr_lo = v.R - v.R_lo
            yerr_hi = v.R_hi - v.R
            ax.errorbar(v.gamma, v.R, yerr=[yerr_lo, yerr_hi], color=color, marker=marker, ls=ls, label=variant, lw=1.5, ms=5, capsize=3)
        ax.set_title(f"{tgt}", fontsize=10)
        ax.set_xlabel("gamma"); ax.grid(alpha=0.3)
        if i == 0 or not share:
            ax.set_ylabel(r"$R = N(\Delta p_x^{rot}>0)/N(\Delta p_x^{rot}<0)$")
        if i == 0:
            ax.legend(fontsize=9)
    fig.suptitle(f"{pol_label} R vs γ ({cut} cut)", fontsize=11)
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    fig.savefig(out_path, dpi=130); plt.close(fig)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--summary-dir", required=True, help="dir containing joined.csv.gz + r_table.csv")
    ap.add_argument("--pol", choices=["ypol","zpol"], required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)
    df = pd.read_csv(Path(args.summary_dir) / "joined.csv.gz", compression="gzip")
    r_df = pd.read_csv(Path(args.summary_dir) / "r_table.csv")
    df = add_rot_neutron(df)

    plot_residuals_per_particle(df, out / f"residuals_per_particle_{args.pol}.png", args.pol)
    for cut in ("loose", "mid", "tight"):
        plot_px_diff_hist(df, cut, out / f"px_diff_hist_{args.pol}_{cut}.png", args.pol)
        plot_r_vs_gamma(r_df, cut, out / f"r_vs_gamma_{args.pol}_{cut}.png", args.pol)
    print(f"[figures] wrote -> {out}")

if __name__ == "__main__":
    main()
