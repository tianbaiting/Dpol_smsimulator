#!/usr/bin/env python3
"""Generate detector-folding diagnostics for NEBULA-Plus observables."""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

REPO_ROOT = Path(__file__).resolve().parents[3]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.reconstruction.nn_target_momentum.plot_nebulaplus_reco import (
    FIG_DIR,
    OBS_DIR,
    RECO_BASE,
    find_reco_files,
    load_observable_frame,
    ratio_with_bootstrap,
    save_figure_pair,
)


DPX_BINS = np.linspace(-300.0, 300.0, 81)
PXN_BINS = np.linspace(-180.0, 180.0, 73)
PYN_BINS = np.linspace(-180.0, 180.0, 73)
PHI_BINS = np.linspace(-math.pi, math.pi, 73)


def px_label(px_limit):
    limit = float(px_limit)
    return f"px{int(limit)}" if limit.is_integer() else f"px{limit:g}"


def ensure_diagnostic_columns(df):
    if "truth_dpx" not in df:
        df["truth_dpx"] = df["truth_rot_pxp"] - df["truth_rot_pxn"]
    if "reco_plane_dpx" not in df:
        df["reco_plane_dpx"] = df["reco_plane_nn_pxp"] - df["reco_plane_reco_pxn"]
    if "reco_truth_plane_dpx" not in df and {"nn_rot_pxp", "reco_rot_pxn"}.issubset(df.columns):
        df["reco_truth_plane_dpx"] = df["nn_rot_pxp"] - df["reco_rot_pxn"]
    if "dphi_reco_truth" not in df and {"phi_reco_event", "phi_event"}.issubset(df.columns):
        raw = df["phi_reco_event"].to_numpy() - df["phi_event"].to_numpy()
        df["dphi_reco_truth"] = np.arctan2(np.sin(raw), np.cos(raw))
    return df


def truth_pxn_fiducial_mask(df, px_limit):
    return pd.Series(np.isfinite(df["truth_pxn"]) & (np.abs(df["truth_pxn"]) < float(px_limit)), index=df.index, dtype=bool)


def neutron_hit_mask(df):
    mask = (df["n_reco_neutrons"] > 0) & np.isfinite(df["reco_pxn"])
    if "reco_pyn" in df:
        mask = mask & np.isfinite(df["reco_pyn"])
    return pd.Series(mask, index=df.index, dtype=bool)


def reco_pxn_fiducial_mask(df, px_limit):
    return pd.Series(neutron_hit_mask(df) & (np.abs(df["reco_pxn"]) < float(px_limit)), index=df.index, dtype=bool)


def event_masks(df, px_limit):
    ensure_diagnostic_columns(df)
    truth = truth_pxn_fiducial_mask(df, px_limit)
    hit = neutron_hit_mask(df)
    reco = reco_pxn_fiducial_mask(df, px_limit)
    return {
        "truth_fiducial": truth,
        "hit": hit,
        "hit_truth_fiducial": truth & hit,
        "reco_fiducial": reco,
    }


def finite_pair_mask(x, y):
    return np.isfinite(x) & np.isfinite(y)


def compute_efficiency_1d(df, x_col, denominator_mask, numerator_mask, bins):
    x = df[x_col].to_numpy(dtype=np.float64)
    den_mask = denominator_mask.to_numpy(dtype=bool) & np.isfinite(x)
    num_mask = numerator_mask.to_numpy(dtype=bool) & den_mask
    den, edges = np.histogram(x[den_mask], bins=bins)
    num, _ = np.histogram(x[num_mask], bins=bins)
    den = den.astype(np.int64)
    num = num.astype(np.int64)
    eff = np.full_like(den, np.nan, dtype=np.float64)
    err = np.full_like(den, np.nan, dtype=np.float64)
    valid = den > 0
    eff[valid] = num[valid] / den[valid]
    err[valid] = np.sqrt(eff[valid] * (1.0 - eff[valid]) / den[valid])
    return pd.DataFrame(
        {
            "bin_low": edges[:-1],
            "bin_high": edges[1:],
            "bin_center": 0.5 * (edges[:-1] + edges[1:]),
            "den": den,
            "num": num,
            "eff": eff,
            "err": err,
        }
    )


def compute_efficiency_2d(df, x_col, y_col, denominator_mask, numerator_mask, x_bins, y_bins):
    ensure_diagnostic_columns(df)
    x = df[x_col].to_numpy(dtype=np.float64)
    y = df[y_col].to_numpy(dtype=np.float64)
    finite = finite_pair_mask(x, y)
    den_mask = denominator_mask.to_numpy(dtype=bool) & finite
    num_mask = numerator_mask.to_numpy(dtype=bool) & den_mask
    den, _, _ = np.histogram2d(x[den_mask], y[den_mask], bins=(x_bins, y_bins))
    num, _, _ = np.histogram2d(x[num_mask], y[num_mask], bins=(x_bins, y_bins))
    eff = np.full_like(den, np.nan, dtype=np.float64)
    np.divide(num, den, out=eff, where=den > 0)
    return den, num, eff


def compute_response_matrix(df, truth_col, reco_col, mask, truth_bins, reco_bins):
    ensure_diagnostic_columns(df)
    truth = df[truth_col].to_numpy(dtype=np.float64)
    reco = df[reco_col].to_numpy(dtype=np.float64)
    valid = mask.to_numpy(dtype=bool) & finite_pair_mask(truth, reco)
    hist, _, _ = np.histogram2d(truth[valid], reco[valid], bins=(truth_bins, reco_bins))
    counts = hist.T.astype(np.float64)
    prob = np.full_like(counts, np.nan, dtype=np.float64)
    col_sum = counts.sum(axis=0)
    np.divide(counts, col_sum[None, :], out=prob, where=col_sum[None, :] > 0)
    return counts, prob


def sign_array(values):
    values = np.asarray(values, dtype=np.float64)
    out = np.full(values.shape, "", dtype=object)
    out[values < 0.0] = "negative"
    out[values > 0.0] = "positive"
    return out


def compute_sign_migration(df, mask):
    ensure_diagnostic_columns(df)
    truth = df["truth_dpx"].to_numpy(dtype=np.float64)
    reco = df["reco_plane_dpx"].to_numpy(dtype=np.float64)
    valid = mask.to_numpy(dtype=bool) & finite_pair_mask(truth, reco) & (truth != 0.0) & (reco != 0.0)
    truth_sign = sign_array(truth[valid])
    reco_sign = sign_array(reco[valid])
    rows = []
    for sign in ("negative", "positive"):
        sel = truth_sign == sign
        n_total = int(np.sum(sel))
        n_neg = int(np.sum(sel & (reco_sign == "negative")))
        n_pos = int(np.sum(sel & (reco_sign == "positive")))
        rows.append(
            {
                "truth_sign": sign,
                "N_truth_sign": n_total,
                "N_reco_negative": n_neg,
                "N_reco_positive": n_pos,
                "P_reco_negative": n_neg / n_total if n_total > 0 else float("nan"),
                "P_reco_positive": n_pos / n_total if n_total > 0 else float("nan"),
            }
        )
    return pd.DataFrame(rows)


def ratio_row(stage, values, n_boot=0):
    result = ratio_with_bootstrap(values, n_boot=n_boot)
    return {
        "stage": stage,
        "N": result.n_used,
        "n_pos": result.n_pos,
        "n_neg": result.n_neg,
        "R": result.r,
        "R_lo": result.r_lo,
        "R_hi": result.r_hi,
    }


def compute_r_ladder(df, masks, n_boot=0):
    ensure_diagnostic_columns(df)
    rows = []
    truth = df.loc[masks["truth_fiducial"], "truth_dpx"].to_numpy()
    rows.append(ratio_row("truth", truth, n_boot=n_boot))

    hit_truth = df.loc[masks["hit_truth_fiducial"], "truth_dpx"].to_numpy()
    rows.append(ratio_row("hit_truth", hit_truth, n_boot=n_boot))

    if "reco_truth_plane_dpx" in df:
        reco_truth_plane = df.loc[masks["reco_fiducial"], "reco_truth_plane_dpx"].to_numpy()
        rows.append(ratio_row("reco_truth_plane", reco_truth_plane, n_boot=n_boot))

    reco_plane = df.loc[masks["reco_fiducial"], "reco_plane_dpx"].to_numpy()
    rows.append(ratio_row("reco_plane", reco_plane, n_boot=n_boot))
    return pd.DataFrame(rows)


def stage_values(df, masks):
    ensure_diagnostic_columns(df)
    out = {
        "truth": df.loc[masks["truth_fiducial"], "truth_dpx"].to_numpy(),
        "hit_truth": df.loc[masks["hit_truth_fiducial"], "truth_dpx"].to_numpy(),
        "reco_plane": df.loc[masks["reco_fiducial"], "reco_plane_dpx"].to_numpy(),
    }
    if "reco_truth_plane_dpx" in df:
        out["reco_truth_plane"] = df.loc[masks["reco_fiducial"], "reco_truth_plane_dpx"].to_numpy()
    return out


def write_matrix_csv(path, matrix, x_edges, y_edges, value_name):
    rows = []
    for ix in range(len(x_edges) - 1):
        for iy in range(len(y_edges) - 1):
            rows.append(
                {
                    "x_low": x_edges[ix],
                    "x_high": x_edges[ix + 1],
                    "y_low": y_edges[iy],
                    "y_high": y_edges[iy + 1],
                    value_name: matrix[ix, iy],
                }
            )
    pd.DataFrame(rows).to_csv(path, index=False)


def write_efficiency_2d_csv(path, den, num, eff, x_edges, y_edges):
    rows = []
    for ix in range(len(x_edges) - 1):
        for iy in range(len(y_edges) - 1):
            rows.append(
                {
                    "x_low": x_edges[ix],
                    "x_high": x_edges[ix + 1],
                    "y_low": y_edges[iy],
                    "y_high": y_edges[iy + 1],
                    "den": den[ix, iy],
                    "num": num[ix, iy],
                    "eff": eff[ix, iy],
                }
            )
    pd.DataFrame(rows).to_csv(path, index=False)


def plot_efficiency_1d(df, masks, pol, cut, px_limit, out_base):
    fig, axes = plt.subplots(1, 2, figsize=(12, 4), squeeze=False)
    variables = [
        ("truth_pxn", PXN_BINS, r"$p_{x,n}^{truth}$ (MeV/c)"),
        ("truth_dpx", DPX_BINS, r"$\Delta p_x^{truth}$ (MeV/c)"),
    ]
    for ax, (col, bins, xlabel) in zip(axes[0], variables):
        for name, mask, color in [
            ("neutron hit", masks["hit_truth_fiducial"], "C0"),
            (f"reco {px_label(px_limit)}", masks["reco_fiducial"], "C3"),
        ]:
            table = compute_efficiency_1d(df, col, masks["truth_fiducial"], mask, bins)
            ax.errorbar(table["bin_center"], table["eff"], yerr=table["err"], color=color, lw=1.2, marker=".", ms=3, label=name)
        ax.set_xlabel(xlabel)
        ax.set_ylabel("efficiency")
        ax.set_ylim(-0.05, 1.05)
        ax.grid(True, alpha=0.3)
    axes[0, 0].legend(fontsize=9)
    fig.suptitle(f"{pol} {cut}: neutron efficiency ({px_label(px_limit)})")
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_efficiency_2d(df, masks, pol, cut, px_limit, out_base, min_bin_count=20):
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.5), squeeze=False)
    specs = [
        ("truth_pxn", "truth_dpx", PXN_BINS, DPX_BINS, r"$p_{x,n}^{truth}$", r"$\Delta p_x^{truth}$"),
        ("truth_pxn", "truth_pyn", PXN_BINS, PYN_BINS, r"$p_{x,n}^{truth}$", r"$p_{y,n}^{truth}$"),
    ]
    for ax, (x_col, y_col, x_bins, y_bins, xlabel, ylabel) in zip(axes[0], specs):
        den, _, eff = compute_efficiency_2d(df, x_col, y_col, masks["truth_fiducial"], masks["reco_fiducial"], x_bins, y_bins)
        eff_plot = eff.T.copy()
        eff_plot[den.T < min_bin_count] = np.nan
        im = ax.imshow(
            eff_plot,
            origin="lower",
            aspect="auto",
            extent=[x_bins[0], x_bins[-1], y_bins[0], y_bins[-1]],
            vmin=0.0,
            vmax=1.0,
            cmap="viridis",
        )
        ax.set_xlabel(f"{xlabel} (MeV/c)")
        ax.set_ylabel(f"{ylabel} (MeV/c)")
        ax.grid(False)
        fig.colorbar(im, ax=ax, label="efficiency")
    fig.suptitle(f"{pol} {cut}: 2D reco fiducial efficiency ({px_label(px_limit)})")
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_response_matrix(matrix, x_edges, y_edges, title, xlabel, ylabel, out_base):
    fig, ax = plt.subplots(figsize=(6, 5))
    im = ax.imshow(
        matrix,
        origin="lower",
        aspect="auto",
        extent=[x_edges[0], x_edges[-1], y_edges[0], y_edges[-1]],
        vmin=0.0,
        vmax=1.0,
        cmap="magma",
    )
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    fig.colorbar(im, ax=ax, label=r"$P(reco|truth)$")
    fig.tight_layout()
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_sign_migration(table, title, out_base):
    matrix = np.array(
        [
            [table.loc[table["truth_sign"] == "negative", "P_reco_negative"].iloc[0], table.loc[table["truth_sign"] == "positive", "P_reco_negative"].iloc[0]],
            [table.loc[table["truth_sign"] == "negative", "P_reco_positive"].iloc[0], table.loc[table["truth_sign"] == "positive", "P_reco_positive"].iloc[0]],
        ],
        dtype=np.float64,
    )
    fig, ax = plt.subplots(figsize=(4.8, 4.2))
    im = ax.imshow(matrix, origin="lower", vmin=0.0, vmax=1.0, cmap="Blues")
    ax.set_xticks([0, 1])
    ax.set_yticks([0, 1])
    ax.set_xticklabels(["truth -", "truth +"])
    ax.set_yticklabels(["reco -", "reco +"])
    ax.set_title(title)
    for iy in range(2):
        for ix in range(2):
            value = matrix[iy, ix]
            label = "nan" if not np.isfinite(value) else f"{value:.2f}"
            ax.text(ix, iy, label, ha="center", va="center", color="black")
    fig.colorbar(im, ax=ax, label=r"$P(reco sign|truth sign)$")
    fig.tight_layout()
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_r_ladder(table, title, out_base):
    order = ["truth", "hit_truth", "reco_truth_plane", "reco_plane"]
    sub = table.set_index("stage").reindex([s for s in order if s in set(table["stage"])])
    fig, ax = plt.subplots(figsize=(7, 4))
    x = np.arange(len(sub))
    ax.plot(x, sub["R"], marker="o", lw=1.5)
    if np.isfinite(sub["R_lo"]).any() and np.isfinite(sub["R_hi"]).any():
        lower = sub["R"].to_numpy() - sub["R_lo"].to_numpy()
        upper = sub["R_hi"].to_numpy() - sub["R"].to_numpy()
        ax.errorbar(x, sub["R"], yerr=np.vstack([lower, upper]), fmt="none", capsize=3, color="black", alpha=0.6)
    ax.axhline(1.0, color="gray", linestyle=":", linewidth=0.8)
    ax.set_xticks(x)
    ax.set_xticklabels(sub.index, rotation=20, ha="right")
    ax.set_ylabel(r"$R = N(\Delta p_x>0)/N(\Delta p_x<0)$")
    ax.set_title(title)
    ax.grid(True, axis="y", alpha=0.3)
    fig.tight_layout()
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_r_vs_gamma(r_cell_df, pol, cut, px_limit, out_base):
    stages = ["truth", "hit_truth", "reco_truth_plane", "reco_plane"]
    targets = sorted(r_cell_df["target"].unique())
    if not targets:
        return
    fig, axes = plt.subplots(1, len(targets), figsize=(6 * len(targets), 4), squeeze=False, sharey=False)
    for i, target in enumerate(targets):
        ax = axes[0, i]
        sub = r_cell_df[(r_cell_df["target"] == target) & (r_cell_df["cut"] == cut) & (r_cell_df["px_limit"] == px_limit)]
        for stage in stages:
            v = sub[sub["stage"] == stage].sort_values("gamma")
            if v.empty:
                continue
            ax.plot(v["gamma"], v["R"], marker="o", lw=1.2, label=stage)
        ax.axhline(1.0, color="gray", linestyle=":", linewidth=0.8)
        ax.set_title(target)
        ax.set_xlabel("gamma")
        ax.set_ylabel("R")
        ax.grid(True, alpha=0.3)
        if i == 0:
            ax.legend(fontsize=8)
    fig.suptitle(f"{pol} {cut}: R ladder vs gamma ({px_label(px_limit)})")
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_dpx_ladder_hist(df, masks, pol, cut, px_limit, out_base):
    values = stage_values(df, masks)
    fig, ax = plt.subplots(figsize=(7, 4.5))
    styles = {
        "truth": ("black", "-", 1.5),
        "hit_truth": ("C2", "-", 1.2),
        "reco_truth_plane": ("C1", "--", 1.2),
        "reco_plane": ("C0", "--", 1.2),
    }
    for stage, arr in values.items():
        arr = arr[np.isfinite(arr)]
        if len(arr) == 0:
            continue
        color, ls, lw = styles.get(stage, ("C7", "-", 1.0))
        ax.hist(arr, bins=DPX_BINS, histtype="step", color=color, linestyle=ls, linewidth=lw, label=f"{stage} N={len(arr)}")
    ax.axvline(0.0, color="gray", linestyle=":", linewidth=0.8)
    ax.set_xlabel(r"$\Delta p_x$ (MeV/c)")
    ax.set_ylabel("counts")
    ax.set_title(f"{pol} {cut}: Delta px ladder ({px_label(px_limit)})")
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_dphi(df, masks, pol, cut, px_limit, out_base):
    if "dphi_reco_truth" not in df:
        return
    vals = df.loc[masks["reco_fiducial"], "dphi_reco_truth"].to_numpy(dtype=np.float64)
    vals = vals[np.isfinite(vals)]
    if len(vals) == 0:
        return
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.hist(vals, bins=PHI_BINS, histtype="stepfilled", alpha=0.75, color="C4")
    ax.axvline(0.0, color="black", linestyle=":", linewidth=0.8)
    ax.set_xlabel(r"$\phi_{reco}-\phi_{truth}$ (rad)")
    ax.set_ylabel("counts")
    ax.set_title(f"{pol} {cut}: reaction-plane migration ({px_label(px_limit)})")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    save_figure_pair(fig, out_base)
    plt.close(fig)


def add_metadata(df, pol, cut, px_limit, target="pooled", gamma="pooled"):
    df = df.copy()
    df.insert(0, "px_limit", float(px_limit))
    df.insert(0, "cut", cut)
    df.insert(0, "gamma", gamma)
    df.insert(0, "target", target)
    df.insert(0, "pol", pol)
    return df


def run_diagnostics(base_dir, fig_dir, out_dir, pols, cuts, px_limits, max_files=None, n_boot=0, min_bin_count=20):
    fig_dir = Path(fig_dir)
    out_dir = Path(out_dir)
    fig_dir.mkdir(parents=True, exist_ok=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    all_eff_1d = []
    all_sign = []
    all_r = []
    all_r_cell = []
    all_survival = []

    for pol in pols:
        files = find_reco_files(base_dir, pol=pol)
        print(f"\nFolding diagnostics: {pol}, {len(files)} files")
        if not files:
            print(f"  [WARN] no files for {pol}")
            continue
        df = load_observable_frame(files, pol, max_files=max_files)
        ensure_diagnostic_columns(df)
        print(f"  {pol}: {len(df):,} events after truth+NN filters")
        if df.empty:
            continue

        for cut in cuts:
            if cut not in df:
                print(f"  [WARN] no cut column {cut} for {pol}")
                continue
            cut_df = df[df[cut]].copy()
            if cut_df.empty:
                continue
            ensure_diagnostic_columns(cut_df)
            for px_limit in px_limits:
                label = px_label(px_limit)
                masks = event_masks(cut_df, px_limit)

                survival = {
                    "pol": pol,
                    "target": "pooled",
                    "gamma": "pooled",
                    "cut": cut,
                    "px_limit": float(px_limit),
                    "N_cut": int(len(cut_df)),
                    "N_truth_fiducial": int(masks["truth_fiducial"].sum()),
                    "N_hit_truth_fiducial": int(masks["hit_truth_fiducial"].sum()),
                    "N_reco_fiducial": int(masks["reco_fiducial"].sum()),
                }
                all_survival.append(survival)

                for x_col, bins in (("truth_pxn", PXN_BINS), ("truth_dpx", DPX_BINS)):
                    for mode, numerator in (("hit", masks["hit_truth_fiducial"]), ("reco_fiducial", masks["reco_fiducial"])):
                        eff = compute_efficiency_1d(cut_df, x_col, masks["truth_fiducial"], numerator, bins)
                        eff.insert(0, "mode", mode)
                        eff.insert(0, "variable", x_col)
                        all_eff_1d.append(add_metadata(eff, pol, cut, px_limit))

                plot_efficiency_1d(cut_df, masks, pol, cut, px_limit, fig_dir / f"efficiency_1d_{pol}_{cut}_{label}")
                plot_efficiency_2d(
                    cut_df,
                    masks,
                    pol,
                    cut,
                    px_limit,
                    fig_dir / f"efficiency_2d_{pol}_{cut}_{label}",
                    min_bin_count=min_bin_count,
                )
                for x_col, y_col, x_bins, y_bins, suffix in [
                    ("truth_pxn", "truth_dpx", PXN_BINS, DPX_BINS, "pxn_vs_dpx"),
                    ("truth_pxn", "truth_pyn", PXN_BINS, PYN_BINS, "pxn_vs_pyn"),
                ]:
                    den2, num2, eff2 = compute_efficiency_2d(
                        cut_df,
                        x_col,
                        y_col,
                        masks["truth_fiducial"],
                        masks["reco_fiducial"],
                        x_bins,
                        y_bins,
                    )
                    write_efficiency_2d_csv(
                        out_dir / f"efficiency_2d_{suffix}_{pol}_{cut}_{label}.csv",
                        den2,
                        num2,
                        eff2,
                        x_bins,
                        y_bins,
                    )

                dpx_counts, dpx_prob = compute_response_matrix(cut_df, "truth_dpx", "reco_plane_dpx", masks["reco_fiducial"], DPX_BINS, DPX_BINS)
                plot_response_matrix(
                    dpx_prob,
                    DPX_BINS,
                    DPX_BINS,
                    f"{pol} {cut}: Delta px response ({label})",
                    r"$\Delta p_x^{truth}$ (MeV/c)",
                    r"$\Delta p_x^{reco}$ (MeV/c)",
                    fig_dir / f"response_dpx_{pol}_{cut}_{label}",
                )
                write_matrix_csv(out_dir / f"response_dpx_counts_{pol}_{cut}_{label}.csv", dpx_counts, DPX_BINS, DPX_BINS, "count")
                write_matrix_csv(out_dir / f"response_dpx_prob_{pol}_{cut}_{label}.csv", dpx_prob, DPX_BINS, DPX_BINS, "prob")

                px_counts, px_prob = compute_response_matrix(cut_df, "truth_pxn", "reco_pxn", masks["reco_fiducial"], PXN_BINS, PXN_BINS)
                plot_response_matrix(
                    px_prob,
                    PXN_BINS,
                    PXN_BINS,
                    f"{pol} {cut}: neutron px response ({label})",
                    r"$p_{x,n}^{truth}$ (MeV/c)",
                    r"$p_{x,n}^{reco}$ (MeV/c)",
                    fig_dir / f"response_pxn_{pol}_{cut}_{label}",
                )
                write_matrix_csv(out_dir / f"response_pxn_counts_{pol}_{cut}_{label}.csv", px_counts, PXN_BINS, PXN_BINS, "count")
                write_matrix_csv(out_dir / f"response_pxn_prob_{pol}_{cut}_{label}.csv", px_prob, PXN_BINS, PXN_BINS, "prob")

                sign = compute_sign_migration(cut_df, masks["reco_fiducial"])
                all_sign.append(add_metadata(sign, pol, cut, px_limit))
                plot_sign_migration(sign, f"{pol} {cut}: sign migration ({label})", fig_dir / f"sign_migration_{pol}_{cut}_{label}")

                ladder = compute_r_ladder(cut_df, masks, n_boot=n_boot)
                all_r.append(add_metadata(ladder, pol, cut, px_limit))
                plot_r_ladder(ladder, f"{pol} {cut}: R ladder ({label})", fig_dir / f"r_ladder_{pol}_{cut}_{label}")
                plot_dpx_ladder_hist(cut_df, masks, pol, cut, px_limit, fig_dir / f"dpx_ladder_hist_{pol}_{cut}_{label}")
                plot_dphi(cut_df, masks, pol, cut, px_limit, fig_dir / f"dphi_reco_truth_{pol}_{cut}_{label}")

                for (target, gamma), cell in cut_df.groupby(["target", "gamma"], sort=True, observed=True):
                    cell = cell.copy()
                    ensure_diagnostic_columns(cell)
                    cell_masks = event_masks(cell, px_limit)
                    cell_ladder = compute_r_ladder(cell, cell_masks, n_boot=n_boot)
                    all_r_cell.append(add_metadata(cell_ladder, pol, cut, px_limit, target=str(target), gamma=str(gamma)))

    if all_eff_1d:
        pd.concat(all_eff_1d, ignore_index=True).to_csv(out_dir / "efficiency_1d.csv", index=False)
    if all_sign:
        pd.concat(all_sign, ignore_index=True).to_csv(out_dir / "sign_migration.csv", index=False)
    if all_r:
        pd.concat(all_r, ignore_index=True).to_csv(out_dir / "r_ladder.csv", index=False)
    if all_r_cell:
        r_cell = pd.concat(all_r_cell, ignore_index=True)
        r_cell.to_csv(out_dir / "r_ladder_by_cell.csv", index=False)
        for pol in pols:
            for cut in cuts:
                for px_limit in px_limits:
                    plot_r_vs_gamma(r_cell, pol, cut, float(px_limit), fig_dir / f"r_ladder_vs_gamma_{pol}_{cut}_{px_label(px_limit)}")
    if all_survival:
        pd.DataFrame(all_survival).to_csv(out_dir / "survival_summary.csv", index=False)

    print(f"\nFolding diagnostic tables saved to {out_dir}")
    print(f"Folding diagnostic figures saved to {fig_dir}")


def parse_args():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base-dir", default=RECO_BASE, help="reconstruction output base directory")
    ap.add_argument(
        "--fig-dir",
        default=str(Path(FIG_DIR) / "folding_diagnostics"),
        help="output directory for figures",
    )
    ap.add_argument(
        "--out-dir",
        default=str(Path(OBS_DIR) / "folding_diagnostics"),
        help="output directory for tables",
    )
    ap.add_argument("--pols", nargs="+", default=["ypol", "zpol"], choices=["ypol", "zpol"])
    ap.add_argument("--cuts", nargs="+", default=["loose", "mid", "tight"], choices=["loose", "mid", "tight"])
    ap.add_argument("--px-limits", nargs="+", type=float, default=[60.0, 80.0, 100.0])
    ap.add_argument("--max-files", type=int, default=None)
    ap.add_argument("--bootstrap", type=int, default=0)
    ap.add_argument("--min-bin-count", type=int, default=20)
    return ap.parse_args()


def main():
    args = parse_args()
    run_diagnostics(
        base_dir=args.base_dir,
        fig_dir=args.fig_dir,
        out_dir=args.out_dir,
        pols=args.pols,
        cuts=args.cuts,
        px_limits=args.px_limits,
        max_files=args.max_files,
        n_boot=args.bootstrap,
        min_bin_count=args.min_bin_count,
    )


if __name__ == "__main__":
    main()
