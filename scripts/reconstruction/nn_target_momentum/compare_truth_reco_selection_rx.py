#!/usr/bin/env python3
"""Reco-defined selection closure for NEBULA-Plus R_x."""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


REPO_ROOT = Path(__file__).resolve().parents[3]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.reconstruction.nn_target_momentum.plot_nebulaplus_reco import (  # noqa: E402
    RECO_BASE,
    find_reco_files,
    load_observable_frame,
    ratio_with_bootstrap,
    rotate_to_reaction_plane,
)

plt.rcParams["figure.autolayout"] = False


DEFAULT_OUTPUT_DIR = Path("docs/reports/samurai_workshop2026/data")
DEFAULT_FIGURE_DIR = Path("docs/reports/samurai_workshop2026/figures")
DEFAULT_TARGETS = ("Sn112E190", "Sn124E190")
DEFAULT_GAMMAS = ("g050", "g060", "g070", "g080")
DEFAULT_POLARIZATIONS = ("ypol",)
DEFAULT_CUTS = ("tight",)
MODES = {
    "truth_cut_truth_obs": {
        "selection_source": "truth",
        "observable_source": "truth",
        "fiducial_source": "truth",
        "label": "truth cut + truth observable",
        "color": "black",
        "marker": "o",
        "linestyle": "-",
        "alpha": 1.0,
        "linewidth": 2.2,
    },
    "truth_cut_reco_obs": {
        "selection_source": "truth",
        "observable_source": "reco",
        "fiducial_source": "reco",
        "label": "truth cut + reco observable",
        "color": "#1f77b4",
        "marker": "^",
        "linestyle": "--",
        "alpha": 0.58,
        "linewidth": 1.6,
    },
    "reco_cut_truth_obs": {
        "selection_source": "reco",
        "observable_source": "truth",
        "fiducial_source": "truth",
        "label": "reco cut + truth observable",
        "color": "#2ca02c",
        "marker": "v",
        "linestyle": "--",
        "alpha": 0.58,
        "linewidth": 1.6,
    },
    "reco_cut_reco_obs": {
        "selection_source": "reco",
        "observable_source": "reco",
        "fiducial_source": "reco",
        "label": "reco cut + reco observable",
        "color": "#d62728",
        "marker": "s",
        "linestyle": "-",
        "alpha": 1.0,
        "linewidth": 2.2,
    },
}
TARGET_LABELS = {
    "Sn112E190": "112Sn",
    "Sn124E190": "124Sn",
}
TARGET_COLORS = {
    "Sn112E190": "#1f77b4",
    "Sn124E190": "#d62728",
}


def _as_bool_series(values: pd.Series | np.ndarray, index: pd.Index) -> pd.Series:
    return pd.Series(np.asarray(values, dtype=bool), index=index, dtype=bool)


def gamma_number(label: str) -> float:
    text = str(label)
    if text.startswith("g"):
        return float(text[1:]) / 100.0
    return float(text)


def ratio_sigma(r_value: float, n_events: int) -> float:
    if not np.isfinite(r_value) or r_value <= 0.0 or n_events <= 0:
        return float("nan")
    return float((1.0 + r_value) * math.sqrt(r_value / float(n_events)))


def finite_sign_values(values: Iterable[float]) -> np.ndarray:
    arr = np.asarray(values, dtype=np.float64)
    arr = arr[np.isfinite(arr)]
    return arr[arr != 0.0]


def safe_divide(numerator: int, denominator: int) -> float:
    return float(numerator / denominator) if denominator > 0 else float("nan")


def add_truth_selection_columns(df: pd.DataFrame, px_limit: float = 60.0) -> pd.DataFrame:
    out = df.copy()
    truth_sum_px = out["truth_pxp"] + out["truth_pxn"]
    truth_sum_py = out["truth_pyp"] + out["truth_pyn"]
    truth_sum_t2 = truth_sum_px * truth_sum_px + truth_sum_py * truth_sum_py
    truth_sum_t = np.sqrt(truth_sum_t2)
    truth_phi = np.arctan2(truth_sum_py, truth_sum_px)

    truth_loose = (np.abs(out["truth_pyp"] - out["truth_pyn"]) < 150.0) & (truth_sum_t2 > 2500.0)
    truth_mid = truth_loose & (truth_sum_t < 200.0)
    truth_tight = truth_mid & ((math.pi - np.abs(truth_phi)) < 0.2)
    truth_px = np.isfinite(out["truth_pxn"]) & (np.abs(out["truth_pxn"]) < float(px_limit))

    out["truth_sum_px"] = truth_sum_px
    out["truth_sum_py"] = truth_sum_py
    out["truth_sum_t2"] = truth_sum_t2
    out["truth_sum_t"] = truth_sum_t
    out["truth_phi"] = truth_phi
    out["truth_loose"] = _as_bool_series(truth_loose, out.index)
    out["truth_mid"] = _as_bool_series(truth_mid, out.index)
    out["truth_tight"] = _as_bool_series(truth_tight, out.index)
    out[f"truth_px{int(px_limit)}"] = _as_bool_series(truth_px, out.index)
    if float(px_limit) == 60.0:
        out["truth_px60"] = out[f"truth_px{int(px_limit)}"]
    return out


def add_reco_selection_columns(df: pd.DataFrame, px_limit: float = 60.0) -> pd.DataFrame:
    out = df.copy()
    reco_valid = (
        (out["nn_status"] == 0)
        & (out["n_reco_neutrons"] > 0)
        & np.isfinite(out["nn_pxp"])
        & np.isfinite(out["nn_pyp"])
        & np.isfinite(out["reco_pxn"])
        & np.isfinite(out["reco_pyn"])
    )
    reco_sum_px = out["nn_pxp"] + out["reco_pxn"]
    reco_sum_py = out["nn_pyp"] + out["reco_pyn"]
    reco_sum_t2 = reco_sum_px * reco_sum_px + reco_sum_py * reco_sum_py
    reco_sum_t = np.sqrt(reco_sum_t2)
    reco_phi = np.arctan2(reco_sum_py, reco_sum_px)

    reco_loose = reco_valid & (np.abs(out["nn_pyp"] - out["reco_pyn"]) < 150.0) & (reco_sum_t2 > 2500.0)
    reco_mid = reco_loose & (reco_sum_t < 200.0)
    reco_tight = reco_mid & ((math.pi - np.abs(reco_phi)) < 0.2)
    reco_px = reco_valid & (np.abs(out["reco_pxn"]) < float(px_limit))

    out["reco_valid"] = _as_bool_series(reco_valid, out.index)
    out["reco_sum_px"] = reco_sum_px
    out["reco_sum_py"] = reco_sum_py
    out["reco_sum_t2"] = reco_sum_t2
    out["reco_sum_t"] = reco_sum_t
    out["reco_phi"] = reco_phi
    out["reco_loose"] = _as_bool_series(reco_loose, out.index)
    out["reco_mid"] = _as_bool_series(reco_mid, out.index)
    out["reco_tight"] = _as_bool_series(reco_tight, out.index)
    out[f"reco_px{int(px_limit)}"] = _as_bool_series(reco_px, out.index)
    if float(px_limit) == 60.0:
        out["reco_px60"] = out[f"reco_px{int(px_limit)}"]
    return out


def add_truth_reco_observable_columns(df: pd.DataFrame) -> pd.DataFrame:
    out = df.copy()
    truth_rot_pxp, truth_rot_pyp, truth_rot_pxn, truth_rot_pyn, phi_truth = rotate_to_reaction_plane(
        out["truth_pxp"].to_numpy(),
        out["truth_pyp"].to_numpy(),
        out["truth_pxn"].to_numpy(),
        out["truth_pyn"].to_numpy(),
    )
    reco_rot_pxp, reco_rot_pyp, reco_rot_pxn, reco_rot_pyn, phi_reco = rotate_to_reaction_plane(
        out["nn_pxp"].to_numpy(),
        out["nn_pyp"].to_numpy(),
        out["reco_pxn"].to_numpy(),
        out["reco_pyn"].to_numpy(),
    )
    out["phi_truth"] = phi_truth
    out["truth_rot_pxp"] = truth_rot_pxp
    out["truth_rot_pyp"] = truth_rot_pyp
    out["truth_rot_pxn"] = truth_rot_pxn
    out["truth_rot_pyn"] = truth_rot_pyn
    out["truth_dpx"] = truth_rot_pxp - truth_rot_pxn
    out["phi_reco"] = phi_reco
    out["reco_rot_pxp"] = reco_rot_pxp
    out["reco_rot_pyp"] = reco_rot_pyp
    out["reco_rot_pxn"] = reco_rot_pxn
    out["reco_rot_pyn"] = reco_rot_pyn
    out["reco_dpx"] = reco_rot_pxp - reco_rot_pxn
    return out


def prepare_closure_frame(df: pd.DataFrame, px_limit: float = 60.0) -> pd.DataFrame:
    out = add_truth_selection_columns(df, px_limit=px_limit)
    out = add_reco_selection_columns(out, px_limit=px_limit)
    out = add_truth_reco_observable_columns(out)
    return out


def mode_mask(df: pd.DataFrame, mode: str, cut: str) -> pd.Series:
    if mode not in MODES:
        raise ValueError(f"unknown selection/observable mode: {mode}")
    config = MODES[mode]
    selection = df[f"{config['selection_source']}_{cut}"]
    fiducial = df[f"{config['fiducial_source']}_px60"]
    return _as_bool_series(selection & fiducial, df.index)


def mode_values(df: pd.DataFrame, mode: str, cut: str) -> np.ndarray:
    config = MODES[mode]
    mask = mode_mask(df, mode, cut).to_numpy()
    return df[f"{config['observable_source']}_dpx"].to_numpy(dtype=np.float64)[mask]


def compute_mode_ratio(
    df: pd.DataFrame,
    mode: str,
    cut: str,
    n_boot: int,
    target: str,
    pol: str,
    gamma: str,
) -> dict[str, object]:
    config = MODES[mode]
    values = mode_values(df, mode, cut)
    result = ratio_with_bootstrap(values, n_boot=n_boot)
    gamma_value = gamma_number(gamma)
    return {
        "target": target,
        "pol": pol,
        "gamma": gamma,
        "gamma_value": gamma_value,
        "cut": cut,
        "selection_observable_mode": mode,
        "selection_source": config["selection_source"],
        "observable_source": config["observable_source"],
        "fiducial_source": config["fiducial_source"],
        "N": result.n_used,
        "n_pos": result.n_pos,
        "n_neg": result.n_neg,
        "R": result.r,
        "R_lo": result.r_lo,
        "R_hi": result.r_hi,
        "sigma_R": ratio_sigma(result.r, result.n_used),
    }


def compute_selection_migration_row(df: pd.DataFrame, target: str, pol: str, gamma: str) -> dict[str, object]:
    t_mask = (df["truth_tight"] & df["truth_px60"]).to_numpy(dtype=bool)
    r_mask = (df["reco_tight"] & df["reco_px60"]).to_numpy(dtype=bool)
    both = t_mask & r_mask
    truth_only = t_mask & ~r_mask
    reco_only = ~t_mask & r_mask
    union = t_mask | r_mask
    n_parent = int(len(df))
    n_truth = int(np.sum(t_mask))
    n_reco = int(np.sum(r_mask))
    n_both = int(np.sum(both))
    n_truth_only = int(np.sum(truth_only))
    n_reco_only = int(np.sum(reco_only))
    row = {
        "target": target,
        "pol": pol,
        "gamma": gamma,
        "N_parent": n_parent,
        "N_truth_selected": n_truth,
        "N_reco_selected": n_reco,
        "N_both": n_both,
        "N_truth_only": n_truth_only,
        "N_reco_only": n_reco_only,
        "selection_efficiency": safe_divide(n_both, n_truth),
        "selection_purity": safe_divide(n_both, n_reco),
        "jaccard_overlap": safe_divide(n_both, int(np.sum(union))),
        "truth_loss_fraction": safe_divide(n_truth_only, n_truth),
        "reco_contamination_fraction": safe_divide(n_reco_only, n_reco),
    }
    for source in ("truth", "reco"):
        for layer in ("loose", "mid", "tight"):
            count = int(np.sum(df[f"{source}_{layer}"].to_numpy(dtype=bool)))
            row[f"{source}_{layer}_count"] = count
            row[f"{source}_{layer}_pass_rate"] = safe_divide(count, n_parent)
        tight_px = (df[f"{source}_tight"] & df[f"{source}_px60"]).to_numpy(dtype=bool)
        count = int(np.sum(tight_px))
        row[f"{source}_tight_px60_count"] = count
        row[f"{source}_tight_px60_pass_rate"] = safe_divide(count, n_parent)
    return row


def layer_migration_rows(df: pd.DataFrame, target: str, pol: str, gamma: str) -> list[dict[str, object]]:
    specs = [
        ("loose", df["truth_loose"].to_numpy(dtype=bool), df["reco_loose"].to_numpy(dtype=bool)),
        ("mid", df["truth_mid"].to_numpy(dtype=bool), df["reco_mid"].to_numpy(dtype=bool)),
        ("tight", df["truth_tight"].to_numpy(dtype=bool), df["reco_tight"].to_numpy(dtype=bool)),
        (
            "tight_px60",
            (df["truth_tight"] & df["truth_px60"]).to_numpy(dtype=bool),
            (df["reco_tight"] & df["reco_px60"]).to_numpy(dtype=bool),
        ),
    ]
    rows: list[dict[str, object]] = []
    n_parent = int(len(df))
    for layer, t_mask, r_mask in specs:
        both = t_mask & r_mask
        truth_only = t_mask & ~r_mask
        reco_only = ~t_mask & r_mask
        union = t_mask | r_mask
        n_truth = int(np.sum(t_mask))
        n_reco = int(np.sum(r_mask))
        n_both = int(np.sum(both))
        rows.append(
            {
                "target": target,
                "pol": pol,
                "gamma": gamma,
                "layer": layer,
                "N_parent": n_parent,
                "N_truth": n_truth,
                "N_reco": n_reco,
                "N_both": n_both,
                "N_truth_only": int(np.sum(truth_only)),
                "N_reco_only": int(np.sum(reco_only)),
                "truth_pass_rate": safe_divide(n_truth, n_parent),
                "reco_pass_rate": safe_divide(n_reco, n_parent),
                "selection_efficiency": safe_divide(n_both, n_truth),
                "selection_purity": safe_divide(n_both, n_reco),
                "jaccard_overlap": safe_divide(n_both, int(np.sum(union))),
            }
        )
    return rows


def build_rx_table(df: pd.DataFrame, cuts: Iterable[str], n_boot: int) -> pd.DataFrame:
    rows: list[dict[str, object]] = []
    for (target, pol, gamma), sub in df.groupby(["target", "pol", "gamma"], sort=True, observed=True):
        for cut in cuts:
            for mode in MODES:
                rows.append(
                    compute_mode_ratio(
                        sub,
                        mode=mode,
                        cut=cut,
                        n_boot=n_boot,
                        target=str(target),
                        pol=str(pol),
                        gamma=str(gamma),
                    )
                )
    return pd.DataFrame(rows).sort_values(["pol", "target", "gamma_value", "cut", "selection_observable_mode"])


def build_selection_migration_table(df: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for (target, pol, gamma), sub in df.groupby(["target", "pol", "gamma"], sort=True, observed=True):
        rows.append(compute_selection_migration_row(sub, str(target), str(pol), str(gamma)))
    return pd.DataFrame(rows).sort_values(["pol", "target", "gamma"])


def build_layer_migration_table(df: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for (target, pol, gamma), sub in df.groupby(["target", "pol", "gamma"], sort=True, observed=True):
        rows.extend(layer_migration_rows(sub, str(target), str(pol), str(gamma)))
    return pd.DataFrame(rows).sort_values(["pol", "target", "gamma", "layer"])


def load_frames(base_dir: Path, pols: list[str], targets: list[str], max_files: int | None) -> tuple[pd.DataFrame, list[str]]:
    frames = []
    input_files: list[str] = []
    for pol in pols:
        if pol != "ypol":
            raise ValueError("this reco-defined closure currently implements the ypol cut definitions only")
        files = find_reco_files(str(base_dir), pol=pol)
        if not files:
            raise RuntimeError(f"no *_reco.root files found for pol={pol} under {base_dir}")
        active_files = files[:max_files] if max_files else files
        input_files.extend(active_files)
        print(f"[load] {pol}: {len(active_files)} files")
        df = load_observable_frame(files, pol, max_files=max_files)
        if df.empty:
            raise RuntimeError(f"load_observable_frame returned no events for pol={pol}")
        df = df[df["target"].astype(str).isin(targets)].copy()
        if df.empty:
            raise RuntimeError(f"no loaded events remain after target filter {targets} for pol={pol}")
        frames.append(df)
        print(f"[load] {pol}: {len(df):,} events after target filter")
    return pd.concat(frames, ignore_index=True), input_files


def target_label(target: str) -> str:
    return TARGET_LABELS.get(target, target)


def yerr_from_rows(rows: pd.DataFrame) -> np.ndarray | pd.Series | None:
    if rows.empty:
        return None
    lo = rows["R_lo"].to_numpy(dtype=np.float64)
    hi = rows["R_hi"].to_numpy(dtype=np.float64)
    r = rows["R"].to_numpy(dtype=np.float64)
    if np.all(np.isfinite(lo)) and np.all(np.isfinite(hi)):
        return np.vstack((np.maximum(r - lo, 0.0), np.maximum(hi - r, 0.0)))
    sigma = rows["sigma_R"].to_numpy(dtype=np.float64)
    return sigma if np.any(np.isfinite(sigma)) else None


def save_figure_pair(fig: plt.Figure, base: Path) -> list[str]:
    base.parent.mkdir(parents=True, exist_ok=True)
    paths = [base.with_suffix(".png"), base.with_suffix(".pdf")]
    fig.savefig(paths[0], dpi=180)
    fig.savefig(paths[1])
    return [str(p) for p in paths]


def plot_rx_truthcut_vs_recocut(rx_df: pd.DataFrame, figure_dir: Path, pol: str, cut: str) -> list[str]:
    sub = rx_df[(rx_df["pol"] == pol) & (rx_df["cut"] == cut)]
    targets = [t for t in DEFAULT_TARGETS if t in set(sub["target"].astype(str))]
    fig, axes = plt.subplots(1, len(targets), figsize=(6.4 * len(targets), 4.5), sharey=False, squeeze=False)
    for ax, target in zip(axes[0], targets):
        tsub = sub[sub["target"] == target]
        for mode, config in MODES.items():
            rows = tsub[tsub["selection_observable_mode"] == mode].sort_values("gamma_value")
            rows = rows[np.isfinite(rows["R"].to_numpy(dtype=np.float64))]
            if rows.empty:
                continue
            ax.errorbar(
                rows["gamma_value"],
                rows["R"],
                yerr=yerr_from_rows(rows),
                color=config["color"],
                marker=config["marker"],
                linestyle=config["linestyle"],
                linewidth=config["linewidth"],
                markersize=5.5,
                capsize=3,
                alpha=config["alpha"],
                label=config["label"],
            )
        ax.set_title(target_label(target))
        ax.set_xlabel(r"$\gamma$")
        ax.set_xticks([0.5, 0.6, 0.7, 0.8])
        ax.grid(True, alpha=0.28)
        ax.axhline(1.0, color="0.65", linewidth=0.9, linestyle=":")
    axes[0, 0].set_ylabel(r"$R_x=N(\Delta p_x^{rot}>0)/N(\Delta p_x^{rot}<0)$")
    axes[0, 0].legend(fontsize=8.5, frameon=False)
    fig.suptitle("Truth-defined and reco-defined selection closure", y=0.99)
    fig.text(
        0.5,
        0.925,
        "All reco-cut quantities are derived only from reconstructed proton, neutron, and event-plane information.",
        ha="center",
        va="center",
        fontsize=10,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.9])
    paths = save_figure_pair(fig, figure_dir / "rx_truthcut_vs_recocut_ypol")
    plt.close(fig)
    return paths


def plot_primary_truth_vs_final_reco(rx_df: pd.DataFrame, figure_dir: Path, pol: str, cut: str) -> list[str]:
    sub = rx_df[
        (rx_df["pol"] == pol)
        & (rx_df["cut"] == cut)
        & (rx_df["selection_observable_mode"].isin(["truth_cut_truth_obs", "reco_cut_reco_obs"]))
    ]
    fig, ax = plt.subplots(figsize=(7.2, 4.7))
    for target in DEFAULT_TARGETS:
        tsub = sub[sub["target"] == target]
        color = TARGET_COLORS.get(target, "C0")
        for mode, marker, linestyle, label_suffix in (
            ("truth_cut_truth_obs", "o", "-", "truth-defined"),
            ("reco_cut_reco_obs", "s", "--", "final reco-defined"),
        ):
            rows = tsub[tsub["selection_observable_mode"] == mode].sort_values("gamma_value")
            rows = rows[np.isfinite(rows["R"].to_numpy(dtype=np.float64))]
            if rows.empty:
                continue
            ax.errorbar(
                rows["gamma_value"],
                rows["R"],
                yerr=yerr_from_rows(rows),
                color=color,
                marker=marker,
                linestyle=linestyle,
                linewidth=2.0,
                markersize=5.5,
                capsize=3,
                label=f"{target_label(target)} {label_suffix}",
            )
    ax.set_xlabel(r"$\gamma$")
    ax.set_ylabel(r"$R_x$")
    ax.set_xticks([0.5, 0.6, 0.7, 0.8])
    ax.grid(True, alpha=0.28)
    ax.axhline(1.0, color="0.65", linewidth=0.9, linestyle=":")
    ax.set_title("Primary truth closure vs final reco-only observable")
    ax.legend(fontsize=8.5, frameon=False)
    fig.text(
        0.5,
        0.02,
        "Parent sample: truth-matched MC p+n breakup channel. Final reco selection uses no truth momentum.",
        ha="center",
        fontsize=9,
    )
    fig.tight_layout(rect=[0, 0.04, 1, 1])
    paths = save_figure_pair(fig, figure_dir / "rx_primary_truth_vs_final_reco")
    plt.close(fig)
    return paths


def plot_selection_efficiency_purity(migration_df: pd.DataFrame, figure_dir: Path, pol: str) -> list[str]:
    sub = migration_df[migration_df["pol"] == pol]
    targets = [t for t in DEFAULT_TARGETS if t in set(sub["target"].astype(str))]
    fig, axes = plt.subplots(1, len(targets), figsize=(6.2 * len(targets), 4.2), sharey=True, squeeze=False)
    for ax, target in zip(axes[0], targets):
        rows = sub[sub["target"] == target].copy()
        rows["gamma_value"] = rows["gamma"].map(gamma_number)
        rows = rows.sort_values("gamma_value")
        for column, marker, color, label in (
            ("selection_efficiency", "o", "#1f77b4", "efficiency"),
            ("selection_purity", "s", "#d62728", "purity"),
            ("jaccard_overlap", "^", "#2ca02c", "Jaccard"),
        ):
            ax.plot(
                rows["gamma_value"],
                rows[column],
                marker=marker,
                color=color,
                linewidth=1.8,
                markersize=5.2,
                label=label,
            )
        ax.set_title(target_label(target))
        ax.set_xlabel(r"$\gamma$")
        ax.set_xticks([0.5, 0.6, 0.7, 0.8])
        ax.set_ylim(0.0, 1.05)
        ax.grid(True, alpha=0.28)
    axes[0, 0].set_ylabel("selection metric")
    axes[0, 0].legend(fontsize=8.5, frameon=False)
    fig.suptitle("Reco selection efficiency and purity vs gamma")
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    paths = save_figure_pair(fig, figure_dir / "selection_efficiency_purity_vs_gamma")
    plt.close(fig)
    return paths


def migration_matrix_counts(df: pd.DataFrame) -> np.ndarray:
    t_mask = (df["truth_tight"] & df["truth_px60"]).to_numpy(dtype=bool)
    r_mask = (df["reco_tight"] & df["reco_px60"]).to_numpy(dtype=bool)
    return np.array(
        [
            [int(np.sum(~t_mask & ~r_mask)), int(np.sum(~t_mask & r_mask))],
            [int(np.sum(t_mask & ~r_mask)), int(np.sum(t_mask & r_mask))],
        ],
        dtype=np.int64,
    )


def plot_selection_migration_matrix(df: pd.DataFrame, figure_dir: Path, pol: str, target: str = "Sn124E190") -> list[str]:
    sub = df[(df["pol"].astype(str) == pol) & (df["target"].astype(str) == target)]
    gammas = [g for g in DEFAULT_GAMMAS if g in set(sub["gamma"].astype(str))]
    fig, axes = plt.subplots(1, len(gammas), figsize=(4.0 * len(gammas), 3.8), squeeze=False)
    last_image = None
    for ax, gamma in zip(axes[0], gammas):
        gsub = sub[sub["gamma"].astype(str) == gamma]
        counts = migration_matrix_counts(gsub)
        row_den = counts.sum(axis=1, keepdims=True)
        probs = np.divide(counts, row_den, out=np.zeros_like(counts, dtype=np.float64), where=row_den > 0)
        last_image = ax.imshow(probs, vmin=0.0, vmax=1.0, cmap="Blues")
        for i in range(2):
            for j in range(2):
                ax.text(j, i, f"{probs[i, j]:.3f}\nN={counts[i, j]}", ha="center", va="center", fontsize=8)
        ax.set_xticks([0, 1])
        ax.set_xticklabels(["reco fail", "reco pass"], rotation=25, ha="right")
        ax.set_yticks([0, 1])
        ax.set_yticklabels(["truth fail", "truth pass"])
        ax.set_title(f"{target_label(target)} {gamma_number(gamma):.1f}")
    fig.subplots_adjust(left=0.055, right=0.88, bottom=0.28, top=0.78, wspace=0.55)
    if last_image is not None:
        cbar_ax = fig.add_axes([0.905, 0.28, 0.014, 0.50])
        fig.colorbar(last_image, cax=cbar_ax, label="P(reco selection | truth selection state)")
    fig.suptitle("Selection migration matrix, ypol tight + px60")
    paths = save_figure_pair(fig, figure_dir / "selection_migration_matrix_ypol")
    plt.close(fig)
    return paths


def write_reproduction_check(
    rx_df: pd.DataFrame,
    old_r_table: Path,
    output_dir: Path,
    cut: str,
    px_limit: float,
) -> pd.DataFrame:
    if not old_r_table.exists():
        return pd.DataFrame()
    old_df = pd.read_csv(old_r_table)
    old = old_df[
        (old_df["stage"] == "reco_plane")
        & (old_df["cut"] == cut)
        & (np.isclose(old_df["px_limit"].astype(float), float(px_limit)))
    ][["pol", "target", "gamma", "N", "n_pos", "n_neg", "R"]].copy()
    old = old.rename(
        columns={
            "N": "old_N",
            "n_pos": "old_n_pos",
            "n_neg": "old_n_neg",
            "R": "old_R",
        }
    )
    new = rx_df[
        (rx_df["cut"] == cut) & (rx_df["selection_observable_mode"] == "truth_cut_reco_obs")
    ][["pol", "target", "gamma", "N", "n_pos", "n_neg", "R"]].copy()
    new = new.rename(
        columns={
            "N": "new_N",
            "n_pos": "new_n_pos",
            "n_neg": "new_n_neg",
            "R": "new_R",
        }
    )
    merged = old.merge(new, on=["pol", "target", "gamma"], how="inner")
    if merged.empty:
        return merged
    merged["delta_N"] = merged["new_N"] - merged["old_N"]
    merged["delta_n_pos"] = merged["new_n_pos"] - merged["old_n_pos"]
    merged["delta_n_neg"] = merged["new_n_neg"] - merged["old_n_neg"]
    merged["delta_R"] = merged["new_R"] - merged["old_R"]
    path = output_dir / "rx_truth_cut_reco_obs_reproduction_check.csv"
    merged.sort_values(["pol", "target", "gamma"]).to_csv(path, index=False)
    return merged


def git_value(args: list[str]) -> str:
    try:
        return subprocess.check_output(["git", *args], cwd=REPO_ROOT, text=True).strip()
    except Exception:
        return ""


def write_metadata(
    output_dir: Path,
    params: argparse.Namespace,
    input_files: list[str],
    outputs: dict[str, list[str] | str],
    reproduction_df: pd.DataFrame,
) -> None:
    metadata = {
        "run_time_utc": datetime.now(timezone.utc).isoformat(),
        "git": {
            "branch": git_value(["branch", "--show-current"]),
            "commit": git_value(["rev-parse", "HEAD"]),
            "status_short": git_value(["status", "--short"]),
        },
        "parameters": {
            "base_dir": str(params.base_dir),
            "pols": params.pols,
            "targets": params.targets,
            "cuts": params.cuts,
            "px_limit": params.px_limit,
            "bootstrap": params.bootstrap,
            "max_files": params.max_files,
            "output_dir": str(params.output_dir),
            "figure_dir": str(params.figure_dir),
            "old_r_table": str(params.old_r_table),
        },
        "input_files": input_files,
        "input_file_count": len(input_files),
        "parent_sample": "parent sample is truth-matched p+n breakup MC",
        "analysis_description": "reco-defined selection closure within a truth-matched simulated p+n channel",
        "loader_note": "load_observable_frame also applies its existing reconstructed-proton status and finite-momentum prefilter before this closure analysis.",
        "reco_selection_code_audit": {
            "reco_valid_columns": [
                "nn_status",
                "n_reco_neutrons",
                "nn_pxp",
                "nn_pyp",
                "reco_pxn",
                "reco_pyn",
            ],
            "reco_cut_columns": [
                "nn_pxp",
                "nn_pyp",
                "reco_pxn",
                "reco_pyn",
                "reco_valid",
            ],
            "final_reco_mode": "reco_cut_reco_obs uses reco_tight, reco_px60, phi_reco, reco_rot_pxp, reco_rot_pxn, and reco_dpx only.",
        },
        "outputs": outputs,
    }
    if not reproduction_df.empty:
        sn124 = reproduction_df[
            (reproduction_df["pol"] == "ypol") & (reproduction_df["target"] == "Sn124E190")
        ].copy()
        metadata["reproduction_check"] = {
            "old_stage": "reco_plane",
            "new_mode": "truth_cut_reco_obs",
            "rows": reproduction_df.to_dict(orient="records"),
            "sn124_ypol_max_abs_delta_R": float(sn124["delta_R"].abs().max()) if not sn124.empty else float("nan"),
            "sn124_ypol_counts_match": bool(
                not sn124.empty
                and (sn124["delta_N"].abs().max() == 0)
                and (sn124["delta_n_pos"].abs().max() == 0)
                and (sn124["delta_n_neg"].abs().max() == 0)
            ),
        }
    (output_dir / "rx_truth_reco_selection_metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )


def split_cli_values(values: list[str]) -> list[str]:
    out: list[str] = []
    for value in values:
        out.extend([part for part in str(value).split(",") if part])
    return out


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare truth-defined and reco-defined R_x selections.")
    parser.add_argument("--base-dir", type=Path, default=Path(RECO_BASE))
    parser.add_argument("--pols", nargs="+", default=list(DEFAULT_POLARIZATIONS))
    parser.add_argument("--targets", nargs="+", default=list(DEFAULT_TARGETS))
    parser.add_argument("--cuts", nargs="+", default=list(DEFAULT_CUTS))
    parser.add_argument("--px-limit", type=float, default=60.0)
    parser.add_argument("--bootstrap", type=int, default=500)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--figure-dir", type=Path, default=DEFAULT_FIGURE_DIR)
    parser.add_argument("--max-files", type=int, default=None)
    parser.add_argument("--old-r-table", type=Path, default=None)
    args = parser.parse_args(argv)
    args.pols = split_cli_values(args.pols)
    args.targets = split_cli_values(args.targets)
    args.cuts = split_cli_values(args.cuts)
    if args.old_r_table is None:
        args.old_r_table = (
            args.base_dir
            / "checks"
            / "observables"
            / "folding_diagnostics"
            / "r_ladder_by_cell.csv"
        )
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    base_dir = args.base_dir
    if not base_dir.exists():
        raise SystemExit(f"[ERROR] base directory does not exist: {base_dir}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.figure_dir.mkdir(parents=True, exist_ok=True)

    raw_df, input_files = load_frames(base_dir, args.pols, args.targets, args.max_files)
    df = prepare_closure_frame(raw_df, px_limit=args.px_limit)
    rx_df = build_rx_table(df, args.cuts, args.bootstrap)
    migration_df = build_selection_migration_table(df)
    layer_df = build_layer_migration_table(df)

    rx_path = args.output_dir / "rx_truth_vs_reco_selection.csv"
    migration_path = args.output_dir / "selection_migration.csv"
    layer_path = args.output_dir / "selection_layer_migration.csv"
    rx_df.to_csv(rx_path, index=False)
    migration_df.to_csv(migration_path, index=False)
    layer_df.to_csv(layer_path, index=False)

    figure_paths: list[str] = []
    if "ypol" in args.pols and "tight" in args.cuts:
        figure_paths.extend(plot_rx_truthcut_vs_recocut(rx_df, args.figure_dir, "ypol", "tight"))
        figure_paths.extend(plot_primary_truth_vs_final_reco(rx_df, args.figure_dir, "ypol", "tight"))
        figure_paths.extend(plot_selection_efficiency_purity(migration_df, args.figure_dir, "ypol"))
        figure_paths.extend(plot_selection_migration_matrix(df, args.figure_dir, "ypol", "Sn124E190"))

    reproduction_df = write_reproduction_check(
        rx_df,
        old_r_table=args.old_r_table,
        output_dir=args.output_dir,
        cut="tight",
        px_limit=args.px_limit,
    )
    outputs: dict[str, list[str] | str] = {
        "rx_table": str(rx_path),
        "selection_migration": str(migration_path),
        "selection_layer_migration": str(layer_path),
        "figures": figure_paths,
    }
    if not reproduction_df.empty:
        outputs["reproduction_check"] = str(args.output_dir / "rx_truth_cut_reco_obs_reproduction_check.csv")
    write_metadata(args.output_dir, args, input_files, outputs, reproduction_df)

    print(f"[write] {rx_path}")
    print(f"[write] {migration_path}")
    print(f"[write] {layer_path}")
    if not reproduction_df.empty:
        print(f"[write] {args.output_dir / 'rx_truth_cut_reco_obs_reproduction_check.csv'}")
    print(f"[write] {args.output_dir / 'rx_truth_reco_selection_metadata.json'}")
    for path in figure_paths:
        print(f"[write] {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
