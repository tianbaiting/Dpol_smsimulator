#!/usr/bin/env python3
"""Build compact summary figures for the SAMURAI Workshop 2026 talk.

Inputs are the detector-folding CSV tables generated for
docs/reports/gamma_constraint_20260611.  No numbers are hard-coded here except
for labels and styling.
"""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[3]
REPORT = ROOT / "docs/reports/samurai_workshop2026"
FIGDIR = REPORT / "figures"
R_TABLE = ROOT / "docs/reports/gamma_constraint_20260611/figures/sn124_tight_px60_r_table.csv"
SEP_TABLE = ROOT / "docs/reports/gamma_constraint_20260611/figures/sn124_tight_px60_gamma_separation.csv"
ISOTOPE_R_TABLE = ROOT / "docs/reports/gamma_constraint_20260611/figures/sn112_sn124_tight_px60_r_table.csv"


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def as_float(rows: list[dict[str, str]], key: str) -> list[float]:
    return [float(row[key]) for row in rows]


def make_main_y_pol(rows: list[dict[str, str]]) -> None:
    y_truth = [
        r for r in rows
        if r["pol"] == "ypol" and r["stage"] == "truth" and r.get("target", "Sn124E190") == "Sn124E190"
    ]
    y_reco = [
        r for r in rows
        if r["pol"] == "ypol" and r["stage"] == "reco_plane" and r.get("target", "Sn124E190") == "Sn124E190"
    ]

    fig, ax = plt.subplots(figsize=(7.0, 4.1), constrained_layout=True)
    ax.errorbar(
        as_float(y_truth, "gamma_value"),
        as_float(y_truth, "R"),
        yerr=as_float(y_truth, "sigma_R_sim"),
        marker="o",
        linewidth=2.0,
        capsize=4,
        color="#6a6a6a",
        label="truth-side fiducial reference",
    )
    ax.errorbar(
        as_float(y_reco, "gamma_value"),
        as_float(y_reco, "R"),
        yerr=as_float(y_reco, "sigma_R_sim"),
        marker="s",
        linewidth=2.2,
        capsize=4,
        color="#1f77b4",
        label="fully reconstructed folded result",
    )
    ax.set_xlabel(r"symmetry-energy density-dependence parameter $\gamma$")
    ax.set_ylabel(r"$R_x^{\mathrm{reco,fid}}$")
    ax.set_title(
        r"$^{124}$Sn y-pol: truth-defined tight quality class; "
        r"$|p_{x,n}^{\mathrm{reco}}|<60$ MeV/$c$"
    )
    ax.grid(True, alpha=0.25)
    ax.legend(frameon=False)
    ax.set_xlim(0.47, 0.83)
    ax.set_ylim(4.0, 16.0)
    ax.text(
        0.48,
        4.35,
        "Reco points use reconstructed proton, neutron, and event plane.\n"
        "Error bars: current MC statistics only.",
        fontsize=9,
        color="#333333",
    )
    fig.savefig(FIGDIR / "ypol_main_rx_reco_fid.png", dpi=220)
    fig.savefig(FIGDIR / "ypol_main_rx_reco_fid.pdf")
    plt.close(fig)


def make_isotope_y_pol(rows: list[dict[str, str]]) -> None:
    fig, ax = plt.subplots(figsize=(7.0, 4.1), constrained_layout=True)
    styles = {
        "Sn112E190": ("#2ca02c", "o", r"$^{112}$Sn folded reco"),
        "Sn124E190": ("#1f77b4", "s", r"$^{124}$Sn folded reco"),
    }
    for target, (color, marker, label) in styles.items():
        sub = [
            r for r in rows
            if r["target"] == target and r["pol"] == "ypol" and r["stage"] == "reco_plane"
        ]
        sub = sorted(sub, key=lambda r: float(r["gamma_value"]))
        ax.errorbar(
            as_float(sub, "gamma_value"),
            as_float(sub, "R"),
            yerr=as_float(sub, "sigma_R_sim"),
            marker=marker,
            linewidth=2.2,
            capsize=4,
            color=color,
            label=label,
        )
    ax.set_xlabel(r"symmetry-energy density-dependence parameter $\gamma$")
    ax.set_ylabel(r"$R_x^{\mathrm{reco,fid}}$")
    ax.set_title(
        r"y-pol isotope comparison: truth-defined tight quality class; "
        r"$|p_{x,n}^{\mathrm{reco}}|<60$ MeV/$c$"
    )
    ax.grid(True, alpha=0.25)
    ax.legend(frameon=False)
    ax.set_xlim(0.47, 0.83)
    ax.set_ylim(4.0, 13.5)
    ax.text(
        0.48,
        4.35,
        "Folded reco points use reconstructed proton, neutron, and event plane.\n"
        "Error bars: current MC statistics only.",
        fontsize=9,
        color="#333333",
    )
    fig.savefig(FIGDIR / "ypol_isotope_rx_reco_fid.png", dpi=220)
    fig.savefig(FIGDIR / "ypol_isotope_rx_reco_fid.pdf")
    plt.close(fig)


def make_asymmetry(rows: list[dict[str, str]]) -> None:
    y_reco = [r for r in rows if r["pol"] == "ypol" and r["stage"] == "reco_plane"]
    gamma = as_float(y_reco, "gamma_value")
    r_values = as_float(y_reco, "R")
    a_values = [(r - 1.0) / (r + 1.0) for r in r_values]

    fig, ax = plt.subplots(figsize=(6.4, 3.7), constrained_layout=True)
    ax.plot(gamma, a_values, marker="s", linewidth=2.2, color="#2ca02c")
    for x, y in zip(gamma, a_values):
        ax.text(x, y + 0.012, f"{y:.3f}", ha="center", va="bottom", fontsize=9)
    ax.set_xlabel(r"$\gamma$")
    ax.set_ylabel(r"$A_x=(R_x-1)/(R_x+1)$")
    ax.set_title(r"Bounded asymmetry cross-check for y-pol folded reco")
    ax.grid(True, alpha=0.25)
    ax.set_xlim(0.47, 0.83)
    ax.set_ylim(0.62, 0.89)
    fig.savefig(FIGDIR / "ypol_bounded_asymmetry.png", dpi=220)
    fig.savefig(FIGDIR / "ypol_bounded_asymmetry.pdf")
    plt.close(fig)


def make_required_events(sep_rows: list[dict[str, str]]) -> None:
    rows = [r for r in sep_rows if r["pol"] == "ypol"]
    labels = [
        r["left_gamma"].replace("g0", "0.") + r"$\to$" + r["right_gamma"].replace("g0", "0.")
        for r in rows
    ]
    n_req = as_float(rows, "N_required_3sigma")
    expected = float(rows[0]["expected_N_15mm_16h"])

    fig, ax = plt.subplots(figsize=(6.8, 3.8), constrained_layout=True)
    bars = ax.bar(labels, n_req, color=["#9ecae1", "#6baed6", "#3182bd"])
    ax.axhline(expected, color="#d62728", linewidth=2.0, linestyle="--", label="16 h planning anchor")
    ax.set_yscale("log")
    ax.set_ylabel("usable events")
    ax.set_title("Planning-level statistical requirement, y-pol")
    ax.grid(True, axis="y", which="both", alpha=0.25)
    ax.legend(frameon=False, loc="upper left")
    for bar, val in zip(bars, n_req):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            val * 1.15,
            f"{val:.1e}" if val >= 1000 else f"{val:.0f}",
            ha="center",
            va="bottom",
            fontsize=9,
        )
    ax.text(2.45, expected * 0.75, r"$2.75\times10^5$", ha="right", va="top", color="#d62728", fontsize=9)
    fig.savefig(FIGDIR / "ypol_required_events.png", dpi=220)
    fig.savefig(FIGDIR / "ypol_required_events.pdf")
    plt.close(fig)


def main() -> None:
    FIGDIR.mkdir(parents=True, exist_ok=True)
    rows = read_rows(R_TABLE)
    sep_rows = read_rows(SEP_TABLE)
    isotope_rows = read_rows(ISOTOPE_R_TABLE)
    make_main_y_pol(rows)
    make_isotope_y_pol(isotope_rows)
    make_asymmetry(rows)
    make_required_events(sep_rows)


if __name__ == "__main__":
    main()
