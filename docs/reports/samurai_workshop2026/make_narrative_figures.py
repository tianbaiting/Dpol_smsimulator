#!/usr/bin/env python3
"""Build the two new narrative summary figures for the SAMURAI Workshop 2026 talk.

These figures support the restructured physics-first narrative.  y-pol R_x
numbers are read directly from the reco-defined selection closure table; nothing
is hard-coded except labels, colours, and axis ranges.

Inputs (single joint production chain, SAMURAI + NEBULA + NEBULA Plus):
  - docs/reports/samurai_workshop2026/data/rx_truth_vs_reco_selection.csv
      selection_observable_mode == "truth_cut_truth_obs" -> generator-level
      selection_observable_mode == "reco_cut_reco_obs"   -> final reco-defined
  - docs/reports/gamma_constraint_20260611/figures/neutron_sign_efficiency_summary.csv
      columns: target, pol, cut, fiducial, N_pos, N_neg, eps_pos, eps_neg, ...

Outputs (into docs/reports/samurai_workshop2026/figures/):
  - main_ideal_vs_reco.{png,pdf}        : Figure 2, the central before/after result
  - neutron_efficiency_balance.{png,pdf}: slide 7, why a fiducial region is needed

Data consistency note
---------------------
Sn112E190 and Sn124E190 rows in the isotope table share an identical production
chain (same geometry, same proton/neutron reconstruction, same event-plane
definition, same reco-defined quality cuts, same reco px60 fiducial).  Truth is
used only for the generator-level reference, detector diagnostics, and the
truth-matched simulated p+n parent channel.
"""

from __future__ import annotations

import csv
import math
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parents[3]
REPORT = ROOT / "docs/reports/samurai_workshop2026"
FIGDIR = REPORT / "figures"
ISO_TABLE = REPORT / "data/rx_truth_vs_reco_selection.csv"
EFF_TABLE = ROOT / "docs/reports/gamma_constraint_20260611/figures/neutron_sign_efficiency_summary.csv"
PLANNING_ANCHOR_EVENTS = 2.75e5

# Consistent identity across every figure: Sn112 and Sn124 always look the same.
STYLE = {
    "Sn112E190": {"color": "#2ca02c", "marker": "o", "label": r"$^{112}$Sn  isotope reference"},
    "Sn124E190": {"color": "#1f77b4", "marker": "s", "label": r"$^{124}$Sn  primary sensitivity"},
}


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def _series_mode(rows, target, pol, mode):
    sub = sorted(
        [
            r for r in rows
            if r["target"] == target
            and r["pol"] == pol
            and r["selection_observable_mode"] == mode
        ],
        key=lambda r: float(r["gamma_value"]),
    )
    return (
        np.array([float(r["gamma_value"]) for r in sub]),
        np.array([float(r["R"]) for r in sub]),
        np.array([float(r["sigma_R"]) for r in sub]),
    )


def _series_stage(rows, target, pol, stage):
    sub = sorted(
        [r for r in rows if r["target"] == target and r["pol"] == pol and r["stage"] == stage],
        key=lambda r: float(r["gamma_value"]),
    )
    return (
        np.array([float(r["gamma_value"]) for r in sub]),
        np.array([float(r["R"]) for r in sub]),
        np.array([float(r["sigma_R_sim"]) for r in sub]),
    )


def required_events_for_separation(r_left: float, r_right: float, sigma_level: float = 3.0) -> float:
    delta = abs(r_right - r_left)
    if delta <= 0.0:
        return float("inf")
    r_mid = 0.5 * (r_left + r_right)
    return (sigma_level * (1.0 + r_mid) * math.sqrt(r_mid) / delta) ** 2


def make_ideal_vs_reco() -> None:
    """Figure 2: ideal model (left) vs folded reconstructed (right)."""
    rows = read_rows(ISO_TABLE)

    fig, (axL, axR) = plt.subplots(
        1, 2, figsize=(11.4, 5.0), constrained_layout=True, sharey=True
    )

    for target, st in STYLE.items():
        # Left: ideal generator-level prediction.
        gx, rR, rE = _series_mode(rows, target, "ypol", "truth_cut_truth_obs")
        axL.errorbar(gx, rR, yerr=rE, marker=st["marker"], linewidth=2.2,
                     capsize=4, color=st["color"], label=st["label"])
        # Right: final reco-defined detector observable.
        gx, rR, rE = _series_mode(rows, target, "ypol", "reco_cut_reco_obs")
        axR.errorbar(gx, rR, yerr=rE, marker=st["marker"], linewidth=2.2,
                     capsize=4, color=st["color"], label=st["label"])

    for ax, title in (
        (axL, "Ideal model prediction\n(generator level, before any detector)"),
        (axR, "After detector + reconstruction\n(reco-defined selection and observable)"),
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
        fontsize=12, fontweight="bold",
    )
    fig.text(
        0.5, -0.02,
        "Error bars: current MC statistics only.   "
        "Detector points use reconstructed proton, neutron, event plane, and reco-defined cuts.",
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
        gx, rR, rE = _series_mode(rows, target, "ypol", "truth_cut_truth_obs")
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


def make_timeline() -> None:
    """Experiment preparation timeline toward the end-of-April-2027 beamtime.

    Anchors given by the speaker: full simulation and reco-defined closure are
    complete now (2026-07); remaining hardware is the polarimeter and the
    target system; beamtime is end of April 2027.  Intermediate phasing is a
    reasonable plan and can be shifted; only the two anchors above are firm.
    """
    import datetime as dt
    import matplotlib.dates as mdates

    # (label, start, end, color, done?)
    tasks = [
        ("Full simulation\n& reco closure", "2025-09", "2026-07", "#3F8457", True),
        ("Polarimeter\n(design, build, test)",        "2026-07", "2027-02", "#1F4E79", False),
        ("Target system\n(holder + $^{112/124}$Sn)",  "2026-09", "2027-02", "#CA6D26", False),
        ("Detector-systematic\nbudget & checks",      "2026-08", "2027-03", "#555555", False),
        ("Commissioning\n(offline + beam test)",      "2027-02", "2027-04", "#7B9EAE", False),
    ]
    beamtime = dt.datetime(2027, 4, 28)
    today = dt.datetime(2026, 7, 1)

    fig, ax = plt.subplots(figsize=(10.5, 3.9), constrained_layout=True)
    y = np.arange(len(tasks))[::-1]  # top-to-bottom order
    for yi, (_, s, e, color, done) in zip(y, tasks):
        s0 = dt.datetime.fromisoformat(s + "-01")
        e0 = dt.datetime.fromisoformat(e + "-01")
        width = mdates.date2num(e0) - mdates.date2num(s0)
        ax.barh(yi, width, left=mdates.date2num(s0), height=0.55,
                color=color, alpha=0.90 if not done else 0.55,
                edgecolor=color, linewidth=1.5)
        if done:
            ax.text(mdates.date2num(e0) + 12, yi, "done", va="center", ha="left",
                    fontsize=9, color="#3F8457", fontweight="bold")

    # beamtime milestone
    ax.scatter([mdates.date2num(beamtime)], [-1.0], marker="*", s=320,
               color="#d62728", zorder=5, clip_on=False)
    ax.annotate("SAMURAI beamtime\n(end of April 2027)",
                xy=(mdates.date2num(beamtime), -1.0), xytext=(mdates.date2num(beamtime) - 95, -1.0),
                fontsize=9.5, color="#d62728", fontweight="bold", va="center", ha="left",
                arrowprops=dict(arrowstyle="->", color="#d62728", lw=1.4))

    # today line
    ax.axvline(mdates.date2num(today), color="#222222", linestyle="--", linewidth=1.2)
    ax.text(mdates.date2num(today), len(tasks) - 0.3, " now", fontsize=9, color="#222222")

    ax.set_yticks(list(y) + [-1.0])
    ax.set_yticklabels([t[0].replace("\n", " ") for t in tasks] + [""])
    ax.set_ylim(-1.7, len(tasks) - 0.3)
    ax.xaxis.set_major_locator(mdates.MonthLocator(bymonth=[1, 4, 7, 10]))
    _MON = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
    ax.xaxis.set_major_formatter(
        mdates.DateFormatter("") if False else plt.FuncFormatter(
            lambda x, _p: f"{_MON[mdates.num2date(x).month - 1]}\n{mdates.num2date(x).year}"
        )
    )
    ax.set_xlim(mdates.date2num(dt.datetime(2025, 8, 1)), mdates.date2num(dt.datetime(2027, 6, 1)))
    ax.grid(True, axis="x", alpha=0.25)
    ax.set_title("Toward the beamtime: simulation and reco closure done",
                 fontsize=12, fontweight="bold")

    fig.savefig(FIGDIR / "timeline.png", dpi=220, bbox_inches="tight")
    fig.savefig(FIGDIR / "timeline.pdf", bbox_inches="tight")
    plt.close(fig)


def make_statistics_reach() -> None:
    """Slide 10: statistics required vs. beamtime delivered.

    Computes the Sn124 y-pol final-reco gamma-separation scale.  Bars are usable
    events needed for a 3-sigma separation of adjacent gamma intervals; the dashed line is the
    16 h / 15 mm planning anchor.  The anchor sits about 70 times above the
    hardest interval, i.e. statistics are not the bottleneck.
    """
    rows = sorted(
        [
            r for r in read_rows(ISO_TABLE)
            if r["target"] == "Sn124E190"
            and r["pol"] == "ypol"
            and r["selection_observable_mode"] == "reco_cut_reco_obs"
        ],
        key=lambda r: float(r["gamma_value"]),
    )
    labels = []
    n_req = []
    for left, right in zip(rows, rows[1:]):
        labels.append(f"{float(left['gamma_value']):.1f}" + r"$\to$" + f"{float(right['gamma_value']):.1f}")
        n_req.append(required_events_for_separation(float(left["R"]), float(right["R"])))
    anchor = PLANNING_ANCHOR_EVENTS

    fig, ax = plt.subplots(figsize=(7.2, 3.9), constrained_layout=True)
    bars = ax.bar(labels, n_req, color=["#9ecae1", "#6baed6", "#3182bd"], width=0.62)
    ax.axhline(anchor, color="#d62728", linewidth=2.0, linestyle="--",
               label=r"16 h, 15 mm diameter / 1.2 g $^{124}$Sn  ($\approx 2.75\times10^5$ usable)")
    ax.set_yscale("log")
    ax.set_ylabel("usable y-pol events", fontsize=11)
    ax.set_title(r"Statistics needed (3$\sigma$, adjacent $\gamma$) vs. beamtime --- $^{124}$Sn",
                 fontsize=11.5, fontweight="bold")
    ax.grid(True, axis="y", which="both", alpha=0.25)
    ax.legend(frameon=False, fontsize=9, loc="upper left")
    for bar, val in zip(bars, n_req):
        ax.text(bar.get_x() + bar.get_width() / 2, val * 1.18,
                f"{val:.0f}" if val < 1000 else f"{val/1000:.2f}$\\times10^3$",
                ha="center", va="bottom", fontsize=9)
    ax.text(len(n_req) - 0.5, anchor * 0.55, r"$\sim70\times$ margin",
            ha="right", va="top", color="#d62728", fontsize=9.5, fontweight="bold")
    fig.savefig(FIGDIR / "statistics_reach.png", dpi=220, bbox_inches="tight")
    fig.savefig(FIGDIR / "statistics_reach.pdf", bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    FIGDIR.mkdir(parents=True, exist_ok=True)
    make_ideal_prediction()
    make_ideal_vs_reco()
    make_efficiency_balance()
    make_timeline()
    make_statistics_reach()


if __name__ == "__main__":
    main()
