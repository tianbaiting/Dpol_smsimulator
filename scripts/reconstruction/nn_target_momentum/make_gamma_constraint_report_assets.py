#!/usr/bin/env python3
"""Build report figures for the NEBULA-Plus gamma-constraint note."""

from __future__ import annotations

import argparse
import math
import shutil
from pathlib import Path
from typing import Any

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


DEFAULT_RECO_BASE = Path(
    "/home/tian/workspace/sshDir/spana03/Dpol_smsimulator"
    "/data/reconstruction/nebulaplus_nn_joint_20260609_012854"
)
DEFAULT_OUT_DIR = Path("docs/reports/gamma_constraint_20260611/figures")

SN124_TARGET = "Sn124E190"
FIDUCIAL = 60.0
CUT = "tight"
DETECTED_EVENTS_16H_15MM = 4.858209e7
DETECTED_RATES_HZ = {
    "12 mm": 1317.873521,
    "15 mm": 843.439054,
    "20 mm": 474.434468,
}


def ratio_sigma(r_value: float, n_events: float) -> float:
    if r_value <= 0.0 or n_events <= 0.0:
        return float("nan")
    return (1.0 + r_value) * math.sqrt(r_value / n_events)


def required_events_for_separation(r_left: float, r_right: float, sigma_level: float = 3.0) -> float:
    delta = abs(r_right - r_left)
    if delta <= 0.0:
        return float("inf")
    r_mid = 0.5 * (r_left + r_right)
    return (sigma_level * (1.0 + r_mid) * math.sqrt(r_mid) / delta) ** 2


def gamma_number(gamma_label: str) -> float:
    return float(str(gamma_label).replace("g", "")) / 100.0


def load_inputs(reco_base: Path) -> dict[str, Any]:
    obs = reco_base / "checks" / "observables"
    folding = obs / "folding_diagnostics"
    return {
        "folding_dir": folding,
        "cell_passrates": pd.read_csv(obs / "cell_passrates.csv"),
        "r_ladder_by_cell": pd.read_csv(folding / "r_ladder_by_cell.csv"),
        "r_ladder": pd.read_csv(folding / "r_ladder.csv"),
        "sign_migration": pd.read_csv(folding / "sign_migration.csv"),
        "survival_summary": pd.read_csv(folding / "survival_summary.csv"),
        "efficiency_1d": pd.read_csv(folding / "efficiency_1d.csv"),
        "eff_px60": pd.read_csv(obs / "efficiency_pxn_sign_px60.csv"),
        "eff_px100": pd.read_csv(obs / "efficiency_pxn_sign_px100.csv"),
    }


def compute_useful_fraction(cell_passrates: pd.DataFrame, r_cell: pd.DataFrame, pol: str) -> float:
    raw = cell_passrates[(cell_passrates["pol"] == pol) & (cell_passrates["target"] == SN124_TARGET)]["N_raw"].sum()
    usable = r_cell[
        (r_cell["pol"] == pol)
        & (r_cell["target"] == SN124_TARGET)
        & (r_cell["cut"] == CUT)
        & (r_cell["px_limit"] == FIDUCIAL)
        & (r_cell["stage"] == "reco_plane")
    ]["N"].sum()
    return float(usable / raw) if raw > 0 else float("nan")


def r_vs_gamma_table(data: dict[str, pd.DataFrame]) -> pd.DataFrame:
    r_cell = data["r_ladder_by_cell"]
    rows = []
    for pol in ("ypol", "zpol"):
        useful_fraction = compute_useful_fraction(data["cell_passrates"], r_cell, pol)
        expected_n_15mm_16h = DETECTED_EVENTS_16H_15MM * useful_fraction
        sub = r_cell[
            (r_cell["pol"] == pol)
            & (r_cell["target"] == SN124_TARGET)
            & (r_cell["cut"] == CUT)
            & (r_cell["px_limit"] == FIDUCIAL)
            & (r_cell["stage"].isin(["truth", "reco_plane"]))
        ].copy()
        sub["gamma_value"] = sub["gamma"].map(gamma_number)
        for _, row in sub.iterrows():
            n_for_error = expected_n_15mm_16h if row["stage"] == "reco_plane" else row["N"]
            rows.append(
                {
                    "pol": pol,
                    "gamma": row["gamma"],
                    "gamma_value": row["gamma_value"],
                    "stage": row["stage"],
                    "N_sim": int(row["N"]),
                    "n_pos": int(row["n_pos"]),
                    "n_neg": int(row["n_neg"]),
                    "R": float(row["R"]),
                    "sigma_R_sim": ratio_sigma(float(row["R"]), float(row["N"])),
                    "expected_N_15mm_16h": expected_n_15mm_16h,
                    "sigma_R_15mm_16h": ratio_sigma(float(row["R"]), expected_n_15mm_16h),
                    "useful_fraction": useful_fraction,
                }
            )
    return pd.DataFrame(rows)


def separation_table(r_table: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for pol in ("ypol", "zpol"):
        sub = r_table[(r_table["pol"] == pol) & (r_table["stage"] == "reco_plane")].sort_values("gamma_value")
        values = list(sub.itertuples(index=False))
        for left, right in zip(values, values[1:]):
            rows.append(
                {
                    "pol": pol,
                    "left_gamma": left.gamma,
                    "right_gamma": right.gamma,
                    "R_left": left.R,
                    "R_right": right.R,
                    "delta_R": abs(right.R - left.R),
                    "N_required_3sigma": required_events_for_separation(left.R, right.R, 3.0),
                    "expected_N_15mm_16h": left.expected_N_15mm_16h,
                }
            )
    return pd.DataFrame(rows)


def aggregate_efficiency_by_sign(data: dict[str, pd.DataFrame]) -> pd.DataFrame:
    rows = []
    for fiducial, key in (("px60", "eff_px60"), ("px100", "eff_px100")):
        df = data[key]
        for pol in ("ypol", "zpol"):
            for cut in ("loose", "mid", "tight"):
                sub = df[(df["pol"] == pol) & (df["cut"] == cut) & (df["fiducial"] == fiducial)]
                n_pos = sub["N_pos"].sum()
                n_neg = sub["N_neg"].sum()
                h_pos = sub["N_hit_pos"].sum()
                h_neg = sub["N_hit_neg"].sum()
                eps_pos = h_pos / n_pos if n_pos else float("nan")
                eps_neg = h_neg / n_neg if n_neg else float("nan")
                avg = 0.5 * (eps_pos + eps_neg)
                rows.append(
                    {
                        "pol": pol,
                        "cut": cut,
                        "fiducial": fiducial,
                        "N_pos": int(n_pos),
                        "N_neg": int(n_neg),
                        "eps_pos": eps_pos,
                        "eps_neg": eps_neg,
                        "rel_eff_diff": (eps_pos - eps_neg) / avg if avg else float("nan"),
                    }
                )
    return pd.DataFrame(rows)


def plot_r_vs_gamma(r_table: pd.DataFrame, out_dir: Path) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharex=True)
    for ax, pol in zip(axes, ("ypol", "zpol")):
        for stage, color, marker, label in (
            ("truth", "black", "o", "truth"),
            ("reco_plane", "C0", "s", "folded reco"),
        ):
            sub = r_table[(r_table["pol"] == pol) & (r_table["stage"] == stage)].sort_values("gamma_value")
            yerr = sub["sigma_R_sim"] if stage == "truth" else sub["sigma_R_15mm_16h"]
            ax.errorbar(
                sub["gamma_value"],
                sub["R"],
                yerr=yerr,
                color=color,
                marker=marker,
                lw=1.5,
                capsize=3,
                label=label,
            )
        ax.set_title(f"Sn124 {pol}, tight, |pxn|<60 MeV/c")
        ax.set_xlabel("gamma")
        ax.set_ylabel("R")
        ax.grid(True, alpha=0.3)
        ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(out_dir / "sn124_tight_px60_r_vs_gamma_expected_stat.png", dpi=160)
    plt.close(fig)


def plot_beamtime_reach(r_table: pd.DataFrame, sep_table: pd.DataFrame, out_dir: Path) -> None:
    hours = np.linspace(0.05, 16.0, 220)
    fig, ax = plt.subplots(figsize=(7.2, 4.5))
    styles = {"ypol": ("C0", "-"), "zpol": ("C3", "--")}
    for pol in ("ypol", "zpol"):
        fraction = r_table[(r_table["pol"] == pol) & (r_table["stage"] == "reco_plane")]["useful_fraction"].iloc[0]
        color, ls = styles[pol]
        for label, rate in DETECTED_RATES_HZ.items():
            scale = {"12 mm": 1.0, "15 mm": 1.5, "20 mm": 2.0}[label]
            ax.plot(
                hours,
                rate * 3600.0 * hours * fraction,
                color=color,
                linestyle=ls,
                alpha=1.0 / scale,
                lw=1.7,
                label=f"{pol} {label}",
            )
        req = sep_table[sep_table["pol"] == pol]["N_required_3sigma"].max()
        ax.axhline(req, color=color, linestyle=":", lw=1.0, label=f"{pol} worst adjacent gamma, 3sigma")
    ax.set_yscale("log")
    ax.set_xlabel("beam time on Sn124 (h)")
    ax.set_ylabel("expected usable R events")
    ax.set_title("Statistics reach after tight px60 visible-window selection")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=8, ncol=2)
    fig.tight_layout()
    fig.savefig(out_dir / "sn124_beamtime_reach_tight_px60.png", dpi=160)
    plt.close(fig)


def plot_efficiency_balance(eff_table: pd.DataFrame, out_dir: Path) -> None:
    sub = eff_table[(eff_table["cut"] == "tight")].copy()
    sub["fiducial_value"] = sub["fiducial"].str.replace("px", "", regex=False).astype(float)
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    width = 0.35
    xbase = np.arange(len(("px60", "px100")))
    for i, pol in enumerate(("ypol", "zpol")):
        pol_sub = sub[sub["pol"] == pol].sort_values("fiducial_value")
        offset = (i - 0.5) * width
        axes[0].bar(xbase + offset, pol_sub["eps_pos"], width / 2, label=f"{pol} pxn > 0")
        axes[0].bar(xbase + offset + width / 2, pol_sub["eps_neg"], width / 2, label=f"{pol} pxn < 0")
    axes[0].set_xticks(xbase + width / 4)
    axes[0].set_xticklabels(["px60", "px100"])
    axes[0].set_ylim(0, 1)
    axes[0].set_ylabel("neutron hit efficiency")
    axes[0].set_title("tight neutron sign efficiency")
    axes[0].grid(True, axis="y", alpha=0.3)
    axes[0].legend(fontsize=7, ncol=2)
    for pol, color in (("ypol", "C0"), ("zpol", "C3")):
        pol_sub = sub[sub["pol"] == pol].sort_values("fiducial_value")
        axes[1].plot(
            pol_sub["fiducial"],
            np.abs(pol_sub["rel_eff_diff"]),
            marker="o",
            lw=1.7,
            color=color,
            label=pol,
        )
    axes[1].set_ylabel("|relative efficiency difference|")
    axes[1].set_title("sign-balance vs visible window")
    axes[1].grid(True, axis="y", alpha=0.3)
    axes[1].legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(out_dir / "ypol_tight_neutron_efficiency_balance.png", dpi=160)
    plt.close(fig)


def _grid_from_edges(df: pd.DataFrame, value_column: str) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    x_edges = np.r_[np.sort(df["x_low"].unique()), df["x_high"].max()]
    y_edges = np.r_[np.sort(df["y_low"].unique()), df["y_high"].max()]
    x_index = {v: i for i, v in enumerate(x_edges[:-1])}
    y_index = {v: i for i, v in enumerate(y_edges[:-1])}
    z = np.full((len(y_edges) - 1, len(x_edges) - 1), np.nan)
    for row in df.itertuples(index=False):
        z[y_index[row.y_low], x_index[row.x_low]] = getattr(row, value_column)
    return x_edges, y_edges, z


def _draw_heatmap(
    ax: plt.Axes,
    df: pd.DataFrame,
    value_column: str,
    title: str,
    xlabel: str,
    ylabel: str,
    *,
    vmin: float = 0.0,
    vmax: float = 1.0,
    cmap: str = "viridis",
    bad_color: str = "white",
) -> Any:
    x_edges, y_edges, z = _grid_from_edges(df, value_column)
    cmap_obj = plt.get_cmap(cmap).copy()
    cmap_obj.set_bad(bad_color)
    mesh = ax.pcolormesh(x_edges, y_edges, z, shading="auto", vmin=vmin, vmax=vmax, cmap=cmap_obj)
    ax.set_title(title)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.axvline(0.0, color="white", lw=0.8, alpha=0.8)
    ax.axhline(0.0, color="white", lw=0.8, alpha=0.8)
    return mesh


def plot_efficiency_1d(efficiency_1d: pd.DataFrame, out_dir: Path) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(11, 7.2), sharex="col", sharey=True)
    min_den = 20
    variables = [
        ("truth_pxn", r"truth $p_{x,n}$ [MeV/$c$]"),
        ("truth_dpx", r"truth $\Delta p_x$ [MeV/$c$]"),
    ]
    modes = [("hit", "hit efficiency"), ("reco_fiducial", "reco fiducial efficiency")]
    colors = {"ypol": "C0", "zpol": "C3"}
    for col, (variable, xlabel) in enumerate(variables):
        for row, (mode, ylabel) in enumerate(modes):
            ax = axes[row, col]
            for pol in ("ypol", "zpol"):
                sub = efficiency_1d[
                    (efficiency_1d["pol"] == pol)
                    & (efficiency_1d["cut"] == CUT)
                    & (efficiency_1d["px_limit"] == FIDUCIAL)
                    & (efficiency_1d["variable"] == variable)
                    & (efficiency_1d["mode"] == mode)
                    & (efficiency_1d["den"] >= min_den)
                ].dropna(subset=["eff"])
                ax.errorbar(
                    sub["bin_center"],
                    sub["eff"],
                    yerr=sub["err"],
                    color=colors[pol],
                    lw=1.3,
                    capsize=1.5,
                    label=pol,
                )
            ax.set_ylim(0, 1.05)
            ax.set_xlabel(xlabel)
            ax.set_ylabel(ylabel)
            ax.grid(True, alpha=0.25)
            ax.legend(fontsize=8)
    fig.suptitle(f"1D neutron folding efficiency, tight px60, bins with N >= {min_den}", y=0.995)
    fig.tight_layout()
    fig.savefig(out_dir / "neutron_efficiency_1d_tight_px60.png", dpi=160)
    plt.close(fig)


def plot_efficiency_2d_maps(folding_dir: Path, out_dir: Path) -> None:
    specs = [
        (
            "pxn_vs_pyn",
            "neutron_efficiency_2d_pxn_vs_pyn_tight_px60.png",
            r"truth $p_{x,n}$ [MeV/$c$]",
            r"truth $p_{y,n}$ [MeV/$c$]",
            r"2D neutron efficiency in $p_{x,n}$-$p_{y,n}$, tight px60",
        ),
        (
            "pxn_vs_dpx",
            "neutron_efficiency_2d_pxn_vs_dpx_tight_px60.png",
            r"truth $p_{x,n}$ [MeV/$c$]",
            r"truth $\Delta p_x$ [MeV/$c$]",
            r"2D neutron efficiency in $p_{x,n}$-$\Delta p_x$, tight px60",
        ),
    ]
    for stem, output, xlabel, ylabel, title in specs:
        fig, axes = plt.subplots(1, 2, figsize=(10.8, 4.4), sharex=True, sharey=True)
        mesh = None
        for ax, pol in zip(axes, ("ypol", "zpol")):
            df = pd.read_csv(folding_dir / f"efficiency_2d_{stem}_{pol}_tight_px60.csv")
            mesh = _draw_heatmap(ax, df, "eff", pol, xlabel, ylabel)
        fig.suptitle(title, y=0.995)
        fig.colorbar(mesh, ax=axes.ravel().tolist(), shrink=0.86, label="efficiency")
        fig.savefig(out_dir / output, dpi=160, bbox_inches="tight")
        plt.close(fig)


def plot_response_matrices(folding_dir: Path, out_dir: Path) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(12.0, 8.8))
    specs = [
        ("response_pxn_prob", r"$p_{x,n}^{truth}$ [MeV/$c$]", r"$p_{x,n}^{reco}$ [MeV/$c$]", r"$p_{x,n}$ response"),
        ("response_dpx_prob", r"$\Delta p_x^{truth}$ [MeV/$c$]", r"$\Delta p_x^{reco}$ [MeV/$c$]", r"$\Delta p_x$ response"),
    ]
    mesh = None
    for row, pol in enumerate(("ypol", "zpol")):
        for col, (stem, xlabel, ylabel, title) in enumerate(specs):
            ax = axes[row, col]
            df = pd.read_csv(folding_dir / f"{stem}_{pol}_tight_px60.csv")
            mesh = _draw_heatmap(
                ax,
                df,
                "prob",
                f"{pol} {title}",
                xlabel,
                ylabel,
                vmin=0.0,
                vmax=1.0,
                cmap="magma",
                bad_color="black",
            )
            lo = max(df["x_low"].min(), df["y_low"].min())
            hi = min(df["x_high"].max(), df["y_high"].max())
            ax.plot([lo, hi], [lo, hi], color="cyan", lw=0.9, alpha=0.8)
    fig.subplots_adjust(left=0.08, right=0.86, top=0.90, bottom=0.08, hspace=0.42, wspace=0.25)
    cbar_ax = fig.add_axes([0.89, 0.18, 0.025, 0.64])
    fig.colorbar(mesh, cax=cbar_ax, label="conditional probability")
    fig.suptitle("Reconstruction response matrices, tight px60", y=0.97)
    fig.savefig(out_dir / "response_matrices_tight_px60.png", dpi=160)
    plt.close(fig)


def plot_survival_summary(survival: pd.DataFrame, out_dir: Path) -> None:
    sub = survival[(survival["cut"] == CUT) & (survival["px_limit"] == FIDUCIAL)].copy()
    stages = [
        ("N_truth_fiducial", "truth fiducial"),
        ("N_hit_truth_fiducial", "neutron hit"),
        ("N_reco_fiducial", "reco fiducial"),
    ]
    x = np.arange(len(stages))
    fig, axes = plt.subplots(1, 2, figsize=(10.5, 4.2), sharey=True)
    for ax, pol in zip(axes, ("ypol", "zpol")):
        row = sub[sub["pol"] == pol].iloc[0]
        fractions = [row[col] / row["N_cut"] for col, _ in stages]
        ax.bar(x, fractions, color=["C0", "C2", "C1"])
        ax.set_xticks(x)
        ax.set_xticklabels([label for _, label in stages], rotation=20, ha="right")
        ax.set_title(f"{pol}, tight px60")
        ax.set_ylabel("fraction of selected events")
        ax.grid(True, axis="y", alpha=0.3)
        for xpos, value in zip(x, fractions):
            ax.text(xpos, value, f"{value:.3f}", ha="center", va="bottom", fontsize=8)
    fig.suptitle("Visible-window survival through neutron hit and reconstruction", y=0.995)
    fig.tight_layout()
    fig.savefig(out_dir / "survival_flow_tight_px60.png", dpi=160)
    plt.close(fig)


def plot_sign_migration_summary(sign_migration: pd.DataFrame, out_dir: Path) -> None:
    sub = sign_migration[sign_migration["cut"] == CUT].copy()
    fig, axes = plt.subplots(1, 2, figsize=(10.5, 4.2), sharey=True)
    for ax, pol in zip(axes, ("ypol", "zpol")):
        pol_sub = sub[sub["pol"] == pol]
        neg = pol_sub[pol_sub["truth_sign"] == "negative"].sort_values("px_limit")
        pos = pol_sub[pol_sub["truth_sign"] == "positive"].sort_values("px_limit")
        ax.plot(neg["px_limit"], neg["P_reco_positive"], marker="o", label="negative truth -> positive reco")
        ax.plot(pos["px_limit"], pos["P_reco_negative"], marker="s", label="positive truth -> negative reco")
        ax.set_yscale("log")
        ax.set_xlabel(r"$|p_{x,n}|$ visible limit [MeV/$c$]")
        ax.set_ylabel("sign-flip probability")
        ax.set_title(f"{pol}, tight")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend(fontsize=8)
    fig.suptitle(r"$\Delta p_x$ sign migration after reconstruction", y=0.995)
    fig.tight_layout()
    fig.savefig(out_dir / "sign_migration_summary_tight.png", dpi=160)
    plt.close(fig)


def plot_r_ladder_summary(r_ladder: pd.DataFrame, out_dir: Path) -> None:
    stages = ["truth", "hit_truth", "reco_truth_plane", "reco_plane"]
    labels = ["truth", "neutron hit", "reco truth-plane", "reco plane"]
    x = np.arange(len(stages))
    fig, axes = plt.subplots(1, 2, figsize=(10.8, 4.2), sharey=False)
    for ax, pol in zip(axes, ("ypol", "zpol")):
        sub = r_ladder[
            (r_ladder["pol"] == pol)
            & (r_ladder["cut"] == CUT)
            & (r_ladder["px_limit"] == FIDUCIAL)
        ].set_index("stage")
        y = sub.loc[stages, "R"].to_numpy()
        yerr = np.vstack(
            [
                y - sub.loc[stages, "R_lo"].to_numpy(),
                sub.loc[stages, "R_hi"].to_numpy() - y,
            ]
        )
        ax.errorbar(x, y, yerr=yerr, marker="o", capsize=3, lw=1.6)
        ax.set_xticks(x)
        ax.set_xticklabels(labels, rotation=25, ha="right")
        ax.set_ylabel("R")
        ax.set_title(f"{pol}, tight px60")
        ax.grid(True, axis="y", alpha=0.3)
    fig.suptitle("R ladder: where detector folding changes the observable", y=0.995)
    fig.tight_layout()
    fig.savefig(out_dir / "r_ladder_summary_tight_px60.png", dpi=160)
    plt.close(fig)


def plot_cell_passrate_summary(cell_passrates: pd.DataFrame, out_dir: Path) -> None:
    sub = cell_passrates[cell_passrates["target"] == SN124_TARGET].copy()
    sub["gamma_value"] = sub["gamma"].map(gamma_number)
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharey=True)
    stages = [
        ("N_loose", "loose"),
        ("N_mid", "mid"),
        ("N_tight", "tight"),
        ("N_NEBULA_in_tight", "NEBULA in tight"),
    ]
    for ax, pol in zip(axes, ("ypol", "zpol")):
        pol_sub = sub[sub["pol"] == pol].sort_values("gamma_value")
        for col, label in stages:
            ax.plot(
                pol_sub["gamma_value"],
                pol_sub[col] / pol_sub["N_raw"],
                marker="o",
                lw=1.4,
                label=label,
            )
        ax.set_yscale("log")
        ax.set_xlabel("gamma")
        ax.set_ylabel("fraction of raw simulated events")
        ax.set_title(f"Sn124 {pol}")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend(fontsize=8)
    fig.suptitle("Event-selection and NEBULA-visible pass rates", y=0.995)
    fig.tight_layout()
    fig.savefig(out_dir / "sn124_selection_passrates.png", dpi=160)
    plt.close(fig)


def copy_reference_figures(reco_base: Path, out_dir: Path) -> None:
    fig_dir = reco_base / "checks" / "figures" / "folding_diagnostics"
    names = [
        "dpx_ladder_hist_ypol_tight_px60.png",
        "response_dpx_ypol_tight_px60.png",
        "efficiency_2d_ypol_tight_px60.png",
        "r_ladder_ypol_tight_px60.png",
        "r_ladder_zpol_tight_px60.png",
        "efficiency_1d_ypol_tight_px60.png",
        "efficiency_1d_zpol_tight_px60.png",
        "response_pxn_ypol_tight_px60.png",
        "response_pxn_zpol_tight_px60.png",
        "response_dpx_zpol_tight_px60.png",
        "sign_migration_ypol_tight_px60.png",
        "sign_migration_zpol_tight_px60.png",
        "dphi_reco_truth_ypol_tight_px60.png",
        "dphi_reco_truth_zpol_tight_px60.png",
        "r_ladder_vs_gamma_ypol_tight_px60.png",
        "r_ladder_vs_gamma_zpol_tight_px60.png",
    ]
    for name in names:
        src = fig_dir / name
        if src.exists():
            shutil.copy2(src, out_dir / name)


def write_tables(r_table: pd.DataFrame, sep_table: pd.DataFrame, eff_table: pd.DataFrame, out_dir: Path) -> None:
    r_table.to_csv(out_dir / "sn124_tight_px60_r_table.csv", index=False)
    sep_table.to_csv(out_dir / "sn124_tight_px60_gamma_separation.csv", index=False)
    eff_table.to_csv(out_dir / "neutron_sign_efficiency_summary.csv", index=False)

    headline = {
        "max_N_required_3sigma_ypol": float(sep_table[sep_table["pol"] == "ypol"]["N_required_3sigma"].max()),
        "max_N_required_3sigma_zpol": float(sep_table[sep_table["pol"] == "zpol"]["N_required_3sigma"].max()),
        "expected_N_15mm_16h_ypol": float(
            r_table[(r_table["pol"] == "ypol") & (r_table["stage"] == "reco_plane")]["expected_N_15mm_16h"].iloc[0]
        ),
        "expected_N_15mm_16h_zpol": float(
            r_table[(r_table["pol"] == "zpol") & (r_table["stage"] == "reco_plane")]["expected_N_15mm_16h"].iloc[0]
        ),
    }
    pd.Series(headline).to_json(out_dir / "headline_numbers.json", indent=2)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reco-base", type=Path, default=DEFAULT_RECO_BASE)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    data = load_inputs(args.reco_base)
    r_table = r_vs_gamma_table(data)
    sep_table = separation_table(r_table)
    eff_table = aggregate_efficiency_by_sign(data)
    plot_r_vs_gamma(r_table, args.out_dir)
    plot_beamtime_reach(r_table, sep_table, args.out_dir)
    plot_efficiency_balance(eff_table, args.out_dir)
    plot_efficiency_1d(data["efficiency_1d"], args.out_dir)
    plot_efficiency_2d_maps(data["folding_dir"], args.out_dir)
    plot_response_matrices(data["folding_dir"], args.out_dir)
    plot_survival_summary(data["survival_summary"], args.out_dir)
    plot_sign_migration_summary(data["sign_migration"], args.out_dir)
    plot_r_ladder_summary(data["r_ladder"], args.out_dir)
    plot_cell_passrate_summary(data["cell_passrates"], args.out_dir)
    copy_reference_figures(args.reco_base, args.out_dir)
    write_tables(r_table, sep_table, eff_table, args.out_dir)
    print(f"gamma constraint report assets saved to {args.out_dir}")


if __name__ == "__main__":
    main()
