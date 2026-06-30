#!/usr/bin/env python3
"""Generate the SAMURAI Workshop 2026 headline figures from the NEBULA-Plus
gamma-constraint result tables.

Reads the already-computed R(gamma) table and separation table produced by
``make_gamma_constraint_report_assets.py`` (which consume the NEBULA-Plus
joint reconstruction sample ``nebulaplus_nn_joint_20260609_012854``) and emits
workshop-ready plots with simulation-statistical error bars on every point.

Usage:
    python3 scripts/reconstruction/nn_target_momentum/make_samurai_workshop_figures.py
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_SRC = REPO_ROOT / "docs/reports/gamma_constraint_20260611/figures"
DEFAULT_OUT = REPO_ROOT / "docs/reports/samurai_workshop2026/figures"


def load_tables(src: Path) -> dict[str, pd.DataFrame]:
    return {
        "r": pd.read_csv(src / "sn124_tight_px60_r_table.csv"),
        "sep": pd.read_csv(src / "sn124_tight_px60_gamma_separation.csv"),
    }


def _z_separation(r_lo: float, r_hi: float, sig_lo: float, sig_hi: float) -> float:
    denom = math.sqrt(sig_lo ** 2 + sig_hi ** 2)
    if denom <= 0.0:
        return float("nan")
    return abs(r_lo - r_hi) / denom


def plot_main_rx_vs_gamma(r_df: pd.DataFrame, sep_df: pd.DataFrame, out_dir: Path) -> None:
    """Centerpiece: truth vs full reconstruction (NEBULA+NEBULA Plus), sim errors."""
    fig, axes = plt.subplots(1, 2, figsize=(11.5, 4.6), sharex=True)
    pol_label = {"ypol": r"$^{124}$Sn  y-polarized", "zpol": r"$^{124}$Sn  z-polarized"}

    for ax, pol in zip(axes, ("ypol", "zpol")):
        for stage, color, marker, ls, label in (
            ("truth", "#333333", "o", "-", "truth level"),
            ("reco_plane", "#1f4e79", "s", "--", "full reco (NEBULA + NEBULA Plus)"),
        ):
            sub = (
                r_df[(r_df["pol"] == pol) & (r_df["stage"] == stage)]
                .sort_values("gamma_value")
            )
            yerr = sub["sigma_R_sim"].to_numpy()
            ax.errorbar(
                sub["gamma_value"],
                sub["R"],
                yerr=yerr,
                color=color,
                marker=marker,
                linestyle=ls,
                linewidth=1.8,
                markersize=6,
                capsize=3,
                label=label,
            )

        ax.axhline(1.0, color="gray", linestyle=":", linewidth=0.9)
        ax.set_yscale("log")
        ax.set_title(pol_label[pol], fontsize=12)
        ax.set_xlabel(r"symmetry-energy stiffness  $\gamma$")
        if pol == "ypol":
            ax.set_ylabel(r"$R_x = N(\Delta p_x^{\mathrm{rot}}>0)\,/\,N(\Delta p_x^{\mathrm{rot}}<0)$")
        ax.grid(True, which="both", alpha=0.28)
        ax.legend(fontsize=8.5, loc="best")

        # separation annotation between gamma=0.5 and 0.8 (reco_plane)
        reco = (
            r_df[(r_df["pol"] == pol) & (r_df["stage"] == "reco_plane")]
            .sort_values("gamma_value")
        )
        r05 = reco[reco["gamma_value"] == 0.5]["R"].iloc[0]
        r08 = reco[reco["gamma_value"] == 0.8]["R"].iloc[0]
        s05 = reco[reco["gamma_value"] == 0.5]["sigma_R_sim"].iloc[0]
        s08 = reco[reco["gamma_value"] == 0.8]["sigma_R_sim"].iloc[0]
        z58 = _z_separation(r05, r08, s05, s08)
        ax.text(
            0.97,
            0.04,
            r"$\Delta R_x(0.5\!\to\!0.8)=%.2f$" % (r05 - r08)
            + "\n"
            + r"$Z_{0.5,0.8}^{\mathrm{MC\,stat}}=%.1f\,\sigma$" % z58,
            transform=ax.transAxes,
            fontsize=8.5,
            ha="right",
            va="bottom",
            bbox=dict(boxstyle="round,pad=0.3", facecolor="white", alpha=0.85),
        )

    fig.suptitle(
        r"Reconstructed $R_x(\gamma)$ after full SAMURAI detector response  "
        r"(Sn124, tight, $|p_{x,n}^{\mathrm{reco}}|<60$ MeV/$c$)",
        fontsize=11,
        y=0.99,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(out_dir / "main_rx_vs_gamma.png", dpi=170)
    fig.savefig(out_dir / "main_rx_vs_gamma.pdf")
    plt.close(fig)


def plot_separation_bar(sep_df: pd.DataFrame, out_dir: Path) -> None:
    """Adjacent-gamma separation: |delta R| vs the 3sigma required event count."""
    fig, ax = plt.subplots(figsize=(7.0, 4.2))
    width = 0.38
    polys = ("ypol", "zpol")
    x = np.arange(3)  # three adjacent intervals
    for i, pol in enumerate(polys):
        sub = sep_df[sep_df["pol"] == pol].sort_values("left_gamma")
        labels = [f"{a[-1]}.$\\to${b[-1]}." for a, b in zip(sub["left_gamma"], sub["right_gamma"])]
        ax.bar(x + (i - 0.5) * width, sub["delta_R"], width=width, label=f"{pol} $|\\Delta R_x|$")
    ax.set_xticks(x)
    ax.set_xticklabels(["0.5$\\to$0.6", "0.6$\\to$0.7", "0.7$\\to$0.8"])
    ax.set_xlabel(r"adjacent $\gamma$ interval")
    ax.set_ylabel(r"$|\Delta R_x|$ (reco)")
    ax.set_title("Adjacent-$\\gamma$ separation of the reconstructed observable")
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(out_dir / "separation_bar.png", dpi=170)
    plt.close(fig)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--src", type=Path, default=DEFAULT_SRC)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    tables = load_tables(args.src)
    plot_main_rx_vs_gamma(tables["r"], tables["sep"], args.out)
    plot_separation_bar(tables["sep"], args.out)
    print(f"workshop figures written to {args.out}")


if __name__ == "__main__":
    main()
