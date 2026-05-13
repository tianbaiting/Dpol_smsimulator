"""Part A: Δp_x two-peak source trace.

Loads the regenerated joined.parquet (Task A2), splits NEBULA-hit subset by
hit_mult_n, and emits the three Part A figures:
  fig_A1_dpx_overview      — Δp_x rotated, all events vs NEBULA-hit subset
  fig_A2_dpx_split_mult    — Δp_x rotated split by hit_mult_n (1 vs ≥2)
  fig_A3_nebula_x_discrete — reco neutron x distribution (shows bar-pitch peaks)
"""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def fig_A1(df: pd.DataFrame, out: Path):
    """Δp_x_rot histogram: all events (truth proton + truth neutron) vs NEBULA-hit subset."""
    fig, ax = plt.subplots(figsize=(7, 4.5))
    # truth Δp_x
    dpx_truth = (df.truth_rot_pxp - df.truth_rot_pxn).values
    nh = df.n_reco_neutrons > 0
    dpx_nh = (df.nn_rot_pxp[nh] - df.reco_rot_pxn[nh]).values
    bins = np.linspace(-30, 30, 121)
    ax.hist(dpx_truth, bins=bins, density=True, histtype="step", color="gray", label=f"truth ({len(dpx_truth)})")
    ax.hist(dpx_nh,    bins=bins, density=True, histtype="step", color="C0",   label=f"NEBULA-hit full reco ({len(dpx_nh)})")
    ax.set_xlabel(r"$\Delta p_x^\mathrm{rot}$ [MeV/c]")
    ax.set_ylabel("density")
    ax.set_title("Fig A1: Δp_x overview")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)


def fig_A2(df: pd.DataFrame, out: Path):
    """Δp_x split by hit_mult_n: 1 (single-bar) vs ≥2 (multi-bar cluster)."""
    nh = (df.n_reco_neutrons > 0)
    sub = df[nh]
    dpx = (sub.nn_rot_pxp - sub.reco_rot_pxn).values
    mult = sub.hit_mult_n.values
    fig, ax = plt.subplots(figsize=(7, 4.5))
    bins = np.linspace(-30, 30, 121)
    for m_mask, label, color in [
        (mult == 1, "single bar (mult=1)", "C1"),
        (mult >= 2, "multi-bar (mult≥2)",   "C2"),
    ]:
        n = int(m_mask.sum())
        print(f"  fig_A2: {label}  N={n}")
        ax.hist(dpx[m_mask], bins=bins, density=True, histtype="step", color=color,
                label=f"{label}  N={n}")
    ax.axvline(0, color="k", lw=0.5, ls=":")
    ax.set_xlabel(r"$\Delta p_x^\mathrm{rot}$ [MeV/c]")
    ax.set_ylabel("density")
    ax.set_title("Fig A2: Δp_x split by NEBULA cluster multiplicity")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)


def fig_A3(df: pd.DataFrame, joined_extras: pd.DataFrame | None, out: Path):
    """NEBULA hit x distribution: shows 12-cm bar discrete peaks.

    joined_extras is unused for now (the joined parquet does not currently store
    the per-event reco neutron x in mm). We approximate using direction × L:
        x_reco_mm ~ L_NEBULA * reco_pxn / reco_pzn (mm),  L_NEBULA = 7250 mm.
    """
    nh = df.n_reco_neutrons > 0
    sub = df[nh]
    L = 7250.0  # mm, NEBULA front-face distance from target along z (approximate)
    mask = sub.reco_pzn > 0
    x_reco = L * sub.reco_pxn[mask] / sub.reco_pzn[mask]
    fig, ax = plt.subplots(figsize=(8, 4.5))
    bins = np.linspace(-2100, 2100, 421)  # 10 mm bins; bar pitch is 122 mm
    ax.hist(x_reco, bins=bins, histtype="step", color="C3")
    ax.set_xlabel(r"reco neutron $x$ at NEBULA face [mm]  (≈ $L \cdot p_x/p_z$)")
    ax.set_ylabel("counts")
    ax.set_title("Fig A3: NEBULA reco-x discrete bands (60-bar × ~122 mm pitch)")
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--joined", required=True, help="joined.parquet from 03_analyze_r_breakup.py (must contain hit_mult_n)")
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    df = pd.read_parquet(args.joined)
    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)
    required = {"truth_rot_pxp", "truth_rot_pxn", "nn_rot_pxp", "reco_rot_pxn",
                "n_reco_neutrons", "hit_mult_n", "reco_pxn", "reco_pzn"}
    missing = required - set(df.columns)
    if missing:
        raise SystemExit(f"joined parquet missing columns: {missing}")

    fig_A1(df, out / "fig_A1_dpx_overview.pdf")
    fig_A2(df, out / "fig_A2_dpx_split_mult.pdf")
    fig_A3(df, None, out / "fig_A3_nebula_x_discrete.pdf")
    print(f"[partA] figures written under {out}")


if __name__ == "__main__":
    main()
