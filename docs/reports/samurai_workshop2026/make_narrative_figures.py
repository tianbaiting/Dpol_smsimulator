#!/usr/bin/env python3
"""Build the two new narrative summary figures for the SAMURAI Workshop 2026 talk.

These figures support the restructured physics-first narrative.  All numbers are
read directly from the detector-folding CSV tables; nothing is hard-coded except
labels, colours, and axis ranges.

Inputs (single joint production chain, SAMURAI + NEBULA + NEBULA Plus):
  - docs/reports/gamma_constraint_20260611/figures/sn112_sn124_tight_px60_r_table.csv
      columns: target, pol, gamma, gamma_value, stage, N_sim, n_pos, n_neg,
               R, sigma_R_sim, ...
      stage == "truth"       -> generator-level (ideal model) R_x
      stage == "reco_plane"  -> folded reconstructed R_x
  - docs/reports/gamma_constraint_20260611/figures/neutron_sign_efficiency_summary.csv
      columns: target, pol, cut, fiducial, N_pos, N_neg, eps_pos, eps_neg, ...

Outputs (into docs/reports/samurai_workshop2026/figures/):
  - main_ideal_vs_reco.{png,pdf}        : Figure 2, the central before/after result
  - neutron_efficiency_balance.{png,pdf}: slide 7, why a fiducial region is needed

Data consistency note
---------------------
Sn112E190 and Sn124E190 rows in the isotope table share an identical production
chain (same geometry, same proton/neutron reconstruction, same event-plane
definition, same truth-defined tight quality class, same reco px60 fiducial).
The truth-defined quality class is a shared caveat, not an isotope mismatch.
"""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parents[3]
REPORT = ROOT / "docs/reports/samurai_workshop2026"
FIGDIR = REPORT / "figures"
ISO_TABLE = ROOT / "docs/reports/gamma_constraint_20260611/figures/sn112_sn124_tight_px60_r_table.csv"
EFF_TABLE = ROOT / "docs/reports/gamma_constraint_20260611/figures/neutron_sign_efficiency_summary.csv"

# Consistent identity across every figure: Sn112 and Sn124 always look the same.
STYLE = {
    "Sn112E190": {"color": "#2ca02c", "marker": "o", "label": r"$^{112}$Sn  isotope reference"},
    "Sn124E190": {"color": "#1f77b4", "marker": "s", "label": r"$^{124}$Sn  primary sensitivity"},
}


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def _series(rows, target, pol, stage):
    sub = sorted(
        [r for r in rows if r["target"] == target and r["pol"] == pol and r["stage"] == stage],
        key=lambda r: float(r["gamma_value"]),
    )
    return (
        np.array([float(r["gamma_value"]) for r in sub]),
        np.array([float(r["R"]) for r in sub]),
        np.array([float(r["sigma_R_sim"]) for r in sub]),
    )


def make_ideal_vs_reco() -> None:
    """Figure 2: ideal model (left) vs folded reconstructed (right)."""
    rows = read_rows(ISO_TABLE)

    fig, (axL, axR) = plt.subplots(
        1, 2, figsize=(11.4, 4.6), constrained_layout=True, sharey=True
    )

    for target, st in STYLE.items():
        # Left: ideal generator-level prediction.
        gx, rR, rE = _series(rows, target, "ypol", "truth")
        axL.errorbar(gx, rR, yerr=rE, marker=st["marker"], linewidth=2.2,
                     capsize=4, color=st["color"], label=st["label"])
        # Right: folded reconstructed observable.
        gx, rR, rE = _series(rows, target, "ypol", "reco_plane")
        axR.errorbar(gx, rR, yerr=rE, marker=st["marker"], linewidth=2.2,
                     capsize=4, color=st["color"], label=st["label"])

    for ax, title in (
        (axL, "Ideal model prediction\n(generator level, before any detector)"),
        (axR, "After detector + reconstruction\n(reconstructed proton, neutron, event plane)"),
    ):
        ax.set_title(title, fontsize=11, fontweight="bold")
        ax.set_xlabel(r"symmetry-energy density dependence  $\gamma$", fontsize=11)
        ax.set_xlim(0.46, 0.84)
        ax.set_xticks([0.5, 0.6, 0.7, 0.8])
        ax.grid(True, alpha=0.25)
        ax.legend(frameon=False, fontsize=9, loc="upper right")

    axL.set_ylabel(r"$R_x$", fontsize=12)
    axL.set_ylim(0.0, 16.5)

    fig.suptitle(
        "The detector changes the scale, not the physical ordering",
        fontsize=13, fontweight="bold", y=1.02,
    )
    fig.text(
        0.5, -0.02,
        "Error bars: current MC statistics only.   "
        "Quality class is truth-defined; observable, neutron momentum, and event plane are reconstructed.",
        ha="center", va="top", fontsize=8.5, color="#555555",
    )

    fig.savefig(FIGDIR / "main_ideal_vs_reco.png", dpi=220, bbox_inches="tight")
    fig.savefig(FIGDIR / "main_ideal_vs_reco.pdf", bbox_inches="tight")
    plt.close(fig)


def make_efficiency_balance() -> None:
    """Slide 7: a fiducial region is needed because efficiency is not uniform."""
    rows = read_rows(EFF_TABLE)

    def pick(target, fid):
        r = [x for x in rows
             if x["target"] == target and x["pol"] == "ypol" and x["cut"] == "tight"
             and x["fiducial"] == fid][0]
        return float(r["eps_pos"]), float(r["eps_neg"])

    # Use Sn124 (primary target) to illustrate; px60 vs px100 contrast.
    e60 = pick("Sn124E190", "px60")
    e100 = pick("Sn124E190", "px100")

    fig, ax = plt.subplots(figsize=(7.6, 4.3), constrained_layout=True)
    x = np.array([0.0, 1.0])
    w = 0.36
    b1 = ax.bar(x - w / 2, [e60[0], e100[0]], w, color="#3182bd",
                label=r"$p_{x,n}>0$ branch")
    b2 = ax.bar(x + w / 2, [e60[1], e100[1]], w, color="#fdae6b",
                label=r"$p_{x,n}<0$ branch")

    for bars, vals in ((b1, [e60[0], e100[0]]), (b2, [e60[1], e100[1]])):
        for bar, v in zip(bars, vals):
            ax.text(bar.get_x() + bar.get_width() / 2, v + 0.015, f"{v:.2f}",
                    ha="center", va="bottom", fontsize=10)

    ax.set_xticks(x)
    ax.set_xticklabels([r"fiducial $|p_{x,n}^{\,\mathrm{reco}}|<60$  MeV/$c$",
                        r"full range  $|p_{x,n}^{\,\mathrm{reco}}|<100$  MeV/$c$"],
                       fontsize=10)
    ax.set_ylabel("neutron detection efficiency", fontsize=11)
    ax.set_ylim(0, 0.82)
    ax.set_title(
        "Choose a region where both sides of the asymmetry are seen",
        fontsize=12, fontweight="bold",
    )
    ax.text(0.5, -0.22,
            r"In the wider range the $p_{x,n}<0$ branch becomes almost blind,"
            " so a raw asymmetry would be dominated by acceptance, not physics."
            "\nIllustrated for $^{124}$Sn, y-pol, tight class.",
            transform=ax.transAxes, ha="center", va="top", fontsize=9, color="#555555")
    ax.grid(True, axis="y", alpha=0.25)
    ax.legend(frameon=False, fontsize=10, loc="upper right")

    fig.savefig(FIGDIR / "neutron_efficiency_balance.png", dpi=220, bbox_inches="tight")
    fig.savefig(FIGDIR / "neutron_efficiency_balance.pdf", bbox_inches="tight")
    plt.close(fig)


def make_ideal_prediction() -> None:
    """Slide 4: the ideal generator-level prediction for both isotopes."""
    rows = read_rows(ISO_TABLE)
    fig, ax = plt.subplots(figsize=(7.0, 4.4), constrained_layout=True)
    for target, st in STYLE.items():
        gx, rR, rE = _series(rows, target, "ypol", "truth")
        ax.errorbar(gx, rR, yerr=rE, marker=st["marker"], linewidth=2.2,
                    capsize=4, color=st["color"], label=st["label"])
    ax.set_xlabel(r"symmetry-energy density dependence  $\gamma$", fontsize=11)
    ax.set_ylabel(r"$R_x$  (ideal model)", fontsize=11)
    ax.set_title(r"Ideal model prediction: a clear isotope and $\gamma$ ordering",
                 fontsize=12, fontweight="bold")
    ax.set_xlim(0.46, 0.84)
    ax.set_xticks([0.5, 0.6, 0.7, 0.8])
    ax.set_ylim(0.0, 16.5)
    ax.grid(True, alpha=0.25)
    ax.legend(frameon=False, fontsize=10, loc="upper right")
    fig.text(0.5, -0.03,
             "Generator level, before any detector.  $^{124}$Sn gives the stronger spread; "
             "$^{112}$Sn is the isotope reference.",
             ha="center", va="top", fontsize=9, color="#555555")
    fig.savefig(FIGDIR / "ideal_prediction.png", dpi=220, bbox_inches="tight")
    fig.savefig(FIGDIR / "ideal_prediction.pdf", bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    FIGDIR.mkdir(parents=True, exist_ok=True)
    make_ideal_prediction()
    make_ideal_vs_reco()
    make_efficiency_balance()


if __name__ == "__main__":
    main()
