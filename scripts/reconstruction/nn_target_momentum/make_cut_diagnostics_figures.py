#!/usr/bin/env python3
"""Cut-system diagnostics for the gamma-constraint report.

Reads ``*_reco.root`` files directly via uproot (no CSV dependence) and produces:

1. R-vs-gamma 3x3 grids (cuts x fiducials) for y-pol and z-pol, truth and reco.
2. Delta-px truth-vs-reco 1D overlays for the working point and a comparison.
3. Phase-space density maps (pxn-vs-dpx, pxn-vs-pyn, dpx-vs-dpy) for loose-px100
   versus tight-px60, showing how the visible phase space changes with the cut.

Reuses the loader / cut definitions from ``plot_nebulaplus_reco.py`` so the
diagnostic stays consistent with the rest of the reconstruction pipeline.

Usage:
    python3 scripts/reconstruction/nn_target_momentum/make_cut_diagnostics_figures.py
"""

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
    RECO_BASE,
    find_reco_files,
    load_observable_frame,
    ratio_with_bootstrap,
)

DEFAULT_OUT_DIR = Path(
    "docs/reports/gamma_constraint_20260611/figures/cut_diagnostics"
)

SN124_TARGET = "Sn124E190"
GAMMA_LABELS = ["g050", "g060", "g070", "g080"]
GAMMA_VALUES = {"g050": 0.5, "g060": 0.6, "g070": 0.7, "g080": 0.8}
CUTS = ["loose", "mid", "tight"]
FIDUCIALS = [("px60", 60.0), ("px80", 80.0), ("px100", 100.0)]
POLARIZATIONS = ["ypol", "zpol"]

# Bin definitions reused from plot_detector_folding_diagnostics.py
DPX_BINS = np.linspace(-300.0, 300.0, 81)
PXN_BINS = np.linspace(-180.0, 180.0, 73)
PYN_BINS = np.linspace(-180.0, 180.0, 73)
PXP_BINS = np.linspace(-180.0, 180.0, 73)


def gamma_number(label: str) -> float:
    return GAMMA_VALUES[label]


def truth_dpx(df: pd.DataFrame) -> np.ndarray:
    return df["truth_rot_pxp"].to_numpy() - df["truth_rot_pxn"].to_numpy()


def reco_dpx(df: pd.DataFrame) -> np.ndarray:
    return df["reco_plane_nn_pxp"].to_numpy() - df["reco_plane_reco_pxn"].to_numpy()


def neutron_hit_mask(df: pd.DataFrame) -> pd.Series:
    mask = (
        (df["n_reco_neutrons"] > 0)
        & np.isfinite(df["reco_pxn"])
        & np.isfinite(df["reco_pyn"])
    )
    return pd.Series(mask, index=df.index, dtype=bool)


def truth_fiducial_mask(df: pd.DataFrame, limit: float) -> pd.Series:
    return pd.Series(
        np.isfinite(df["truth_pxn"]) & (np.abs(df["truth_pxn"]) < limit),
        index=df.index,
        dtype=bool,
    )


def reco_fiducial_mask(df: pd.DataFrame, limit: float) -> pd.Series:
    return pd.Series(
        neutron_hit_mask(df).to_numpy() & (np.abs(df["reco_pxn"]) < limit),
        index=df.index,
        dtype=bool,
    )


def load_frame(pol: str, reco_base: Path, max_files: int | None) -> pd.DataFrame:
    files = find_reco_files(str(reco_base), pol=pol)
    if not files:
        raise RuntimeError(f"no reco files found for pol={pol} under {reco_base}")
    print(f"[load] {pol}: {len(files)} root files")
    df = load_observable_frame(files, pol, max_files=max_files)
    if df.empty:
        raise RuntimeError(f"empty frame for pol={pol}")
    df["truth_dpx"] = truth_dpx(df)
    df["reco_dpx"] = reco_dpx(df)
    sub = df[df["target"] == SN124_TARGET].copy()
    print(f"[load] {pol}: {len(sub):,} Sn124 events ({len(df):,} total)")
    return sub


# --------------------------------------------------------------------------- #
# Figure 1: R-vs-gamma 3x3 grid
# --------------------------------------------------------------------------- #
def _ratio_error_bars(values: np.ndarray) -> tuple[float, float, float, float]:
    res = ratio_with_bootstrap(values, n_boot=500)
    r = res.r
    if not np.isfinite(res.r_lo) or not np.isfinite(res.r_hi):
        # delta-method fallback: sigma(R) ~ (1+R) * sqrt(R / N)
        n = res.n_used
        if r > 0 and n > 0:
            err = (1.0 + r) * math.sqrt(r / n)
        else:
            err = float("nan")
        return r, err, err, float(res.n_used)
    return r, r - res.r_lo, res.r_hi - r, float(res.n_used)


def compute_r_vs_gamma(df: pd.DataFrame, cut: str, limit: float) -> pd.DataFrame:
    rows = []
    truth_fid = truth_fiducial_mask(df, limit)
    reco_fid = reco_fiducial_mask(df, limit)
    cut_mask = df[cut].to_numpy(dtype=bool)
    for glabel in GAMMA_LABELS:
        gamma_sel = (df["gamma"] == glabel).to_numpy()
        t_mask = gamma_sel & cut_mask & truth_fid.to_numpy()
        r_mask = gamma_sel & cut_mask & reco_fid.to_numpy()
        t_vals = df["truth_dpx"].to_numpy()[t_mask]
        r_vals = df["reco_dpx"].to_numpy()[r_mask]
        t_vals = t_vals[np.isfinite(t_vals) & (t_vals != 0.0)]
        r_vals = r_vals[np.isfinite(r_vals) & (r_vals != 0.0)]
        tr, tlo, thi, tn = _ratio_error_bars(t_vals)
        rr, rlo, rhi, rn = _ratio_error_bars(r_vals)
        rows.append(
            {
                "gamma": gamma_number(glabel),
                "R_truth": tr, "R_truth_lo": tlo, "R_truth_hi": thi, "N_truth": tn,
                "R_reco": rr, "R_reco_lo": rlo, "R_reco_hi": rhi, "N_reco": rn,
            }
        )
    return pd.DataFrame(rows)


def plot_r_vs_gamma_grid(
    frames: dict[str, pd.DataFrame], out_dir: Path
) -> list[Path]:
    out_paths = []
    for pol in POLARIZATIONS:
        df = frames[pol]
        fig, axes = plt.subplots(
            len(CUTS), len(FIDUCIALS), figsize=(13.0, 10.5), sharex=True
        )
        for r, cut in enumerate(CUTS):
            for c, (fname, limit) in enumerate(FIDUCIALS):
                ax = axes[r, c]
                tbl = compute_r_vs_gamma(df, cut, limit)
                x = tbl["gamma"].to_numpy()
                ax.errorbar(
                    x, tbl["R_truth"],
                    yerr=[tbl["R_truth_lo"], tbl["R_truth_hi"]],
                    color="black", marker="o", ms=4, lw=1.4, capsize=2.5,
                    label="truth", zorder=3,
                )
                ax.errorbar(
                    x, tbl["R_reco"],
                    yerr=[tbl["R_reco_lo"], tbl["R_reco_hi"]],
                    color="C0", marker="s", ms=4, lw=1.4, capsize=2.5,
                    label="folded reco", zorder=3,
                )
                ax.axhline(1.0, color="gray", ls=":", lw=0.8, alpha=0.7)
                ax.set_title(f"{cut} | {fname}", fontsize=10)
                if r == len(CUTS) - 1:
                    ax.set_xlabel(r"$\gamma$")
                if c == 0:
                    ax.set_ylabel("R")
                ax.grid(True, alpha=0.25)
                ymin = min(np.nanmin(tbl["R_truth"]), np.nanmin(tbl["R_reco"]))
                ymax = max(np.nanmax(tbl["R_truth"]), np.nanmax(tbl["R_reco"]))
                if np.isfinite(ymin) and np.isfinite(ymax) and ymax > ymin:
                    pad = 0.12 * (ymax - ymin) + 0.5
                    ax.set_ylim(max(0.0, ymin - pad), ymax + pad)
                if r == 0 and c == 0:
                    ax.legend(fontsize=8, loc="best")
        fig.suptitle(
            f"R($\\gamma$) across cuts and visible windows — Sn124 {pol}, "
            "truth vs folded reco",
            y=0.995,
        )
        fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.985))
        path = out_dir / f"r_vs_gamma_cut_grid_{pol}.png"
        fig.savefig(path, dpi=160)
        fig.savefig(path.with_suffix(".pdf"))
        plt.close(fig)
        out_paths.append(path)
        print(f"[fig] {path}")
    return out_paths


# --------------------------------------------------------------------------- #
# Figure 2: delta-px truth vs reco 1D overlay
# --------------------------------------------------------------------------- #
def plot_dpx_truth_vs_reco_overlay(
    frames: dict[str, pd.DataFrame], out_dir: Path
) -> list[Path]:
    specs = [
        ("tight", "px60", "tight_px60"),
        ("mid", "px60", "mid_px60"),
    ]
    out_paths = []
    for pol in POLARIZATIONS:
        df = frames[pol]
        fig, axes = plt.subplots(1, len(specs), figsize=(12.5, 4.6), sharey=True)
        for ax, (cut, fname, stem) in zip(axes, specs):
            limit = dict(FIDUCIALS)[fname]
            truth_fid = truth_fiducial_mask(df, limit)
            reco_fid = reco_fiducial_mask(df, limit)
            cut_mask = df[cut].to_numpy(dtype=bool)
            t_vals = df["truth_dpx"].to_numpy()[cut_mask & truth_fid.to_numpy()]
            r_vals = df["reco_dpx"].to_numpy()[cut_mask & reco_fid.to_numpy()]
            t_vals = t_vals[np.isfinite(t_vals)]
            r_vals = r_vals[np.isfinite(r_vals)]
            ax.hist(
                t_vals, bins=DPX_BINS, density=True, histtype="step",
                lw=1.8, color="black", label=f"truth (N={len(t_vals):,})",
            )
            ax.hist(
                r_vals, bins=DPX_BINS, density=True, histtype="step",
                lw=1.8, color="C0", label=f"reco (N={len(r_vals):,})",
            )
            ylim = ax.get_ylim()
            ax.axvspan(0.0, DPX_BINS[-1], color="C3", alpha=0.06)
            ax.axvspan(DPX_BINS[0], 0.0, color="C1", alpha=0.06)
            ax.set_ylim(ylim)
            ax.axvline(0.0, color="gray", ls="--", lw=0.9)
            tn_pos = float(np.sum(t_vals > 0))
            tn_neg = float(np.sum(t_vals < 0))
            rn_pos = float(np.sum(r_vals > 0))
            rn_neg = float(np.sum(r_vals < 0))
            tR = tn_pos / tn_neg if tn_neg > 0 else float("inf")
            rR = rn_pos / rn_neg if rn_neg > 0 else float("inf")
            ax.set_title(f"{pol} {cut} {fname}\nR$_{{truth}}$={tR:.2f}  R$_{{reco}}$={rR:.2f}")
            ax.set_xlabel(r"$\Delta p_x$ [MeV/$c$]")
            if ax is axes[0]:
                ax.set_ylabel("normalized density")
            ax.set_xlim(DPX_BINS[0], DPX_BINS[-1])
            ax.grid(True, alpha=0.25)
            ax.legend(fontsize=8, loc="upper center")
        fig.suptitle(
            rf"$\Delta p_x$ truth vs reco — Sn124 {pol}", y=1.0
        )
        fig.tight_layout()
        path = out_dir / f"dpx_truth_vs_reco_overlay_{pol}.png"
        fig.savefig(path, dpi=160)
        fig.savefig(path.with_suffix(".pdf"))
        plt.close(fig)
        out_paths.append(path)
        print(f"[fig] {path}")
    return out_paths


# --------------------------------------------------------------------------- #
# Figure 3: phase-space density maps
# --------------------------------------------------------------------------- #
PHASE_SPECS = [
    (
        "pxn_vs_dpx",
        "truth_pxn", PXN_BINS,
        "truth_dpx", DPX_BINS,
        r"truth $p_{x,n}$ [MeV/$c$]",
        r"truth $\Delta p_x$ [MeV/$c$]",
    ),
    (
        "pxn_vs_pyn",
        "truth_pxn", PXN_BINS,
        "truth_pyn", PYN_BINS,
        r"truth $p_{x,n}$ [MeV/$c$]",
        r"truth $p_{y,n}$ [MeV/$c$]",
    ),
    (
        "dpx_vs_dpy",
        "truth_dpx", DPX_BINS,
        "truth_pyp_minus_pyn", None,
        r"truth $\Delta p_x$ [MeV/$c$]",
        r"truth $p_{y,p}-p_{y,n}$ [MeV/$c$]",
    ),
]


def _get_column(df: pd.DataFrame, name: str) -> np.ndarray:
    if name == "truth_pyp_minus_pyn":
        return df["truth_pyp"].to_numpy() - df["truth_pyn"].to_numpy()
    return df[name].to_numpy()


def plot_phase_density_maps(
    frames: dict[str, pd.DataFrame], out_dir: Path
) -> list[Path]:
    row_specs = [
        ("loose", "px100", "loose | px100"),
        ("tight", "px60", "tight | px60"),
    ]
    out_paths = []
    for stem, xcol, xbins, ycol, ybins, xlabel, ylabel in PHASE_SPECS:
        if ybins is None:
            ybins = xbins.copy()
        fig, axes = plt.subplots(
            len(row_specs), len(POLARIZATIONS), figsize=(12.2, 8.2),
            sharex=True, sharey=True,
        )
        fig.subplots_adjust(left=0.10, right=0.86, top=0.90, bottom=0.12, wspace=0.12, hspace=0.18)
        last_mesh = None
        for r, (cut, fname, rowtitle) in enumerate(row_specs):
            limit = dict(FIDUCIALS)[fname]
            for c, pol in enumerate(POLARIZATIONS):
                ax = axes[r, c]
                df = frames[pol]
                cut_mask = df[cut].to_numpy(dtype=bool)
                fid_mask = truth_fiducial_mask(df, limit).to_numpy()
                sel = cut_mask & fid_mask
                x = _get_column(df, xcol)[sel]
                y = _get_column(df, ycol)[sel]
                ok = np.isfinite(x) & np.isfinite(y)
                last_mesh = ax.hist2d(
                    x[ok], y[ok], bins=[xbins, ybins],
                    cmap="magma", cmin=1,
                )[-1]
                ax.axhline(0.0, color="white", lw=0.7, alpha=0.8)
                ax.axvline(0.0, color="white", lw=0.7, alpha=0.8)
                if r == 0:
                    ax.set_title(pol, fontsize=11)
                if c == 0:
                    ax.set_ylabel(rowtitle + f"\n{ylabel}")
                if r == len(row_specs) - 1:
                    ax.set_xlabel(xlabel)
        fig.suptitle(
            f"Phase-space density ({stem}) — Sn124, truth level",
            y=0.955,
        )
        assert last_mesh is not None
        cax = fig.add_axes((0.89, 0.15, 0.022, 0.72))
        fig.colorbar(last_mesh, cax=cax, label="events / bin")
        path = out_dir / f"phase_density_{stem}.png"
        fig.savefig(path, dpi=160)
        fig.savefig(path.with_suffix(".pdf"))
        plt.close(fig)
        out_paths.append(path)
        print(f"[fig] {path}")
    return out_paths


# --------------------------------------------------------------------------- #
# Figure 4: neutron efficiency across event-quality cuts (none/loose/mid/tight)
# --------------------------------------------------------------------------- #
CUT_CURVES = [
    ("none", None, "gray", "no event cut"),
    ("loose", "loose", "C2", "loose"),
    ("mid", "mid", "C1", "mid"),
    ("tight", "tight", "C0", "tight"),
]


def _binned_efficiency(
    x: np.ndarray, den_mask: np.ndarray, num_mask: np.ndarray, bins: np.ndarray, min_den: int = 20
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    finite = np.isfinite(x)
    den = den_mask & finite
    num = num_mask & finite
    den_c, _ = np.histogram(x[den], bins=bins)
    num_c, _ = np.histogram(x[num], bins=bins)
    centers = 0.5 * (bins[:-1] + bins[1:])
    eff = np.full(len(den_c), np.nan)
    err = np.full(len(den_c), np.nan)
    valid = den_c >= min_den
    eff[valid] = num_c[valid] / den_c[valid]
    err[valid] = np.sqrt(eff[valid] * (1.0 - eff[valid]) / den_c[valid])
    return centers, eff, err


def plot_neutron_efficiency_1d_by_cut(
    frames: dict[str, pd.DataFrame], out_dir: Path
) -> list[Path]:
    out_paths = []
    variables = [
        ("truth_pxn", PXN_BINS, r"truth $p_{x,n}$ [MeV/$c$]"),
        ("truth_dpx", DPX_BINS, r"truth $\Delta p_x$ [MeV/$c$]"),
    ]
    modes = [("hit", "neutron hit efficiency"), ("reco", "reco-fiducial efficiency")]
    for pol in POLARIZATIONS:
        df = frames[pol]
        n = len(df)
        truth_fid = truth_fiducial_mask(df, 60.0).to_numpy()
        hit = neutron_hit_mask(df).to_numpy()
        reco_fid = reco_fiducial_mask(df, 60.0).to_numpy()
        fig, axes = plt.subplots(2, 2, figsize=(12.5, 8.0), sharex="col", sharey=True)
        for col, (xcol, xbins, xlabel) in enumerate(variables):
            x = df[xcol].to_numpy()
            for row, (mode, ylabel) in enumerate(modes):
                ax = axes[row, col]
                num_base = hit if mode == "hit" else reco_fid
                for _, cut_col, color, label in CUT_CURVES:
                    cut_mask = (
                        np.ones(n, dtype=bool) if cut_col is None else df[cut_col].to_numpy(dtype=bool)
                    )
                    den_mask = cut_mask & truth_fid
                    num_mask = cut_mask & truth_fid & num_base
                    centers, eff, err = _binned_efficiency(x, den_mask, num_mask, xbins)
                    valid = np.isfinite(eff)
                    ax.errorbar(
                        centers[valid], eff[valid], yerr=err[valid],
                        color=color, marker="o", ms=3, lw=1.4, capsize=2,
                        label=label, alpha=0.9,
                    )
                ax.axhline(1.0, color="gray", ls=":", lw=0.7)
                ax.set_ylim(0, 1.05)
                ax.set_ylabel(ylabel)
                if row == 1:
                    ax.set_xlabel(xlabel)
                ax.grid(True, alpha=0.25)
                if row == 0 and col == 0:
                    ax.legend(fontsize=8, loc="lower right")
        fig.suptitle(
            f"Sn124 {pol}: neutron efficiency by event-quality cut (px60 fiducial, bins N$\\geq$20)",
            y=0.995,
        )
        fig.tight_layout(rect=(0, 0, 1, 0.985))
        path = out_dir / f"neutron_efficiency_1d_by_cut_{pol}.png"
        fig.savefig(path, dpi=160)
        fig.savefig(path.with_suffix(".pdf"))
        plt.close(fig)
        out_paths.append(path)
        print(f"[fig] {path}")
    return out_paths


def plot_neutron_efficiency_2d_by_cut(
    frames: dict[str, pd.DataFrame], out_dir: Path
) -> list[Path]:
    out_paths = []
    cut_rows = [
        ("none", None, "no event cut"),
        ("loose", "loose", "loose"),
        ("mid", "mid", "mid"),
        ("tight", "tight", "tight"),
    ]
    eff_specs = [
        (
            "pxn_vs_dpx",
            "truth_pxn", PXN_BINS, r"truth $p_{x,n}$ [MeV/$c$]",
            "truth_dpx", DPX_BINS, r"truth $\Delta p_x$ [MeV/$c$]",
        ),
        (
            "pxn_vs_pyn",
            "truth_pxn", PXN_BINS, r"truth $p_{x,n}$ [MeV/$c$]",
            "truth_pyn", PYN_BINS, r"truth $p_{y,n}$ [MeV/$c$]",
        ),
    ]
    for stem, xcol, xbins, xlabel, ycol, ybins, ylabel in eff_specs:
        fig, axes = plt.subplots(
            len(cut_rows), len(POLARIZATIONS), figsize=(11.0, 13.5), sharex=True, sharey=True
        )
        fig.subplots_adjust(left=0.11, right=0.86, top=0.93, bottom=0.07, wspace=0.10, hspace=0.15)
        last_mesh = None
        for r, (_, cut_col, rowtitle) in enumerate(cut_rows):
            for c, pol in enumerate(POLARIZATIONS):
                ax = axes[r, c]
                df = frames[pol]
                cut_mask = (
                    np.ones(len(df), dtype=bool) if cut_col is None else df[cut_col].to_numpy(dtype=bool)
                )
                truth_fid = truth_fiducial_mask(df, 60.0).to_numpy()
                hit = neutron_hit_mask(df).to_numpy()
                den_mask = cut_mask & truth_fid
                num_mask = den_mask & hit
                x = df[xcol].to_numpy()
                y = df[ycol].to_numpy()
                den_h, _, _ = np.histogram2d(x[den_mask], y[den_mask], bins=[xbins, ybins])
                num_h, _, _ = np.histogram2d(x[num_mask], y[num_mask], bins=[xbins, ybins])
                eff = np.full(den_h.shape, np.nan)
                np.divide(num_h, den_h, out=eff, where=den_h > 0)
                last_mesh = ax.pcolormesh(
                    xbins, ybins, eff.T, shading="auto", vmin=0.0, vmax=1.0, cmap="viridis"
                )
                ax.axhline(0.0, color="white", lw=0.7, alpha=0.8)
                ax.axvline(0.0, color="white", lw=0.7, alpha=0.8)
                if r == 0:
                    ax.set_title(pol, fontsize=11)
                if c == 0:
                    ax.set_ylabel(f"{rowtitle}\n{ylabel}")
                if r == len(cut_rows) - 1:
                    ax.set_xlabel(xlabel)
        var_label = stem.replace("_", " ")
        fig.suptitle(
            f"Neutron hit efficiency ({var_label}) by event-quality cut — Sn124, px60 fiducial",
            y=0.965,
        )
        assert last_mesh is not None
        cax = fig.add_axes((0.89, 0.10, 0.022, 0.80))
        fig.colorbar(last_mesh, cax=cax, label="efficiency")
        path = out_dir / f"neutron_efficiency_2d_{stem}_by_cut.png"
        fig.savefig(path, dpi=160)
        fig.savefig(path.with_suffix(".pdf"))
        plt.close(fig)
        out_paths.append(path)
        print(f"[fig] {path}")
    return out_paths


# --------------------------------------------------------------------------- #
# Figure 5: neutron efficiency WITHOUT px fiducial — full truth_pxn range
# --------------------------------------------------------------------------- #
def plot_neutron_efficiency_1d_nofiducial(
    frames: dict[str, pd.DataFrame], out_dir: Path
) -> list[Path]:
    """1D neutron efficiency across the full pxn range, no px fiducial.

    Shows why the px60 cut is needed: NEBULA detection efficiency drops at
    large |pxn| and especially on the negative-pxn side.
    """
    out_paths = []
    variables = [
        ("truth_pxn", PXN_BINS, r"truth $p_{x,n}$ [MeV/$c$]"),
        ("truth_dpx", DPX_BINS, r"truth $\Delta p_x$ [MeV/$c$]"),
    ]
    modes = [("hit", "neutron hit efficiency"), ("reco", "reco-fiducial efficiency")]
    for pol in POLARIZATIONS:
        df = frames[pol]
        n = len(df)
        hit = neutron_hit_mask(df).to_numpy()
        reco_fid = reco_fiducial_mask(df, 60.0).to_numpy()
        fig, axes = plt.subplots(2, 2, figsize=(12.5, 8.0), sharex="col", sharey=True)
        for col, (xcol, xbins, xlabel) in enumerate(variables):
            x = df[xcol].to_numpy()
            for row, (mode, ylabel) in enumerate(modes):
                ax = axes[row, col]
                num_base = hit if mode == "hit" else reco_fid
                for _, cut_col, color, label in CUT_CURVES:
                    cut_mask = (
                        np.ones(n, dtype=bool) if cut_col is None else df[cut_col].to_numpy(dtype=bool)
                    )
                    den_mask = cut_mask
                    num_mask = cut_mask & num_base
                    centers, eff, err = _binned_efficiency(x, den_mask, num_mask, xbins)
                    valid = np.isfinite(eff)
                    ax.errorbar(
                        centers[valid], eff[valid], yerr=err[valid],
                        color=color, marker="o", ms=3, lw=1.4, capsize=2,
                        label=label, alpha=0.9,
                    )
                ax.axhline(1.0, color="gray", ls=":", lw=0.7)
                if "pxn" in xcol:
                    ax.axvline(60.0, color="red", ls="--", lw=0.8, alpha=0.5)
                    ax.axvline(-60.0, color="red", ls="--", lw=0.8, alpha=0.5)
                ax.set_ylim(0, 1.05)
                ax.set_ylabel(ylabel)
                if row == 1:
                    ax.set_xlabel(xlabel)
                ax.grid(True, alpha=0.25)
                if row == 0 and col == 0:
                    ax.legend(fontsize=8, loc="lower right")
        fig.suptitle(
            f"Sn124 {pol}: neutron efficiency, NO px fiducial "
            r"(red dashed = $\pm$60 MeV/$c$ window, bins N$\geq$20)",
            y=0.995,
        )
        fig.tight_layout(rect=(0, 0, 1, 0.985))
        path = out_dir / f"neutron_efficiency_1d_nofiducial_{pol}.png"
        fig.savefig(path, dpi=160)
        fig.savefig(path.with_suffix(".pdf"))
        plt.close(fig)
        out_paths.append(path)
        print(f"[fig] {path}")
    return out_paths


def plot_neutron_efficiency_2d_nofiducial(
    frames: dict[str, pd.DataFrame], out_dir: Path
) -> list[Path]:
    """2D neutron hit efficiency without px fiducial — full phase space."""
    out_paths = []
    cut_rows = [
        ("none", None, "no event cut"),
        ("loose", "loose", "loose"),
        ("mid", "mid", "mid"),
        ("tight", "tight", "tight"),
    ]
    eff_specs = [
        (
            "pxn_vs_pyn",
            "truth_pxn", PXN_BINS, r"truth $p_{x,n}$ [MeV/$c$]",
            "truth_pyn", PYN_BINS, r"truth $p_{y,n}$ [MeV/$c$]",
        ),
        (
            "pxn_vs_dpx",
            "truth_pxn", PXN_BINS, r"truth $p_{x,n}$ [MeV/$c$]",
            "truth_dpx", DPX_BINS, r"truth $\Delta p_x$ [MeV/$c$]",
        ),
    ]
    for stem, xcol, xbins, xlabel, ycol, ybins, ylabel in eff_specs:
        fig, axes = plt.subplots(
            len(cut_rows), len(POLARIZATIONS), figsize=(11.0, 13.5), sharex=True, sharey=True
        )
        fig.subplots_adjust(left=0.11, right=0.86, top=0.93, bottom=0.07, wspace=0.10, hspace=0.15)
        last_mesh = None
        for r, (_, cut_col, rowtitle) in enumerate(cut_rows):
            for c, pol in enumerate(POLARIZATIONS):
                ax = axes[r, c]
                df = frames[pol]
                cut_mask = (
                    np.ones(len(df), dtype=bool) if cut_col is None else df[cut_col].to_numpy(dtype=bool)
                )
                hit = neutron_hit_mask(df).to_numpy()
                den_mask = cut_mask
                num_mask = den_mask & hit
                x = df[xcol].to_numpy()
                y = df[ycol].to_numpy()
                den_h, _, _ = np.histogram2d(x[den_mask], y[den_mask], bins=[xbins, ybins])
                num_h, _, _ = np.histogram2d(x[num_mask], y[num_mask], bins=[xbins, ybins])
                eff = np.full(den_h.shape, np.nan)
                np.divide(num_h, den_h, out=eff, where=den_h > 0)
                last_mesh = ax.pcolormesh(
                    xbins, ybins, eff.T, shading="auto", vmin=0.0, vmax=1.0, cmap="viridis"
                )
                ax.axhline(0.0, color="white", lw=0.7, alpha=0.8)
                ax.axvline(0.0, color="white", lw=0.7, alpha=0.8)
                if "pxn" in xcol:
                    ax.axvline(60.0, color="red", ls="--", lw=0.7, alpha=0.6)
                    ax.axvline(-60.0, color="red", ls="--", lw=0.7, alpha=0.6)
                if r == 0:
                    ax.set_title(pol, fontsize=11)
                if c == 0:
                    ax.set_ylabel(f"{rowtitle}\n{ylabel}")
                if r == len(cut_rows) - 1:
                    ax.set_xlabel(xlabel)
        var_label = stem.replace("_", " ")
        fig.suptitle(
            f"Neutron hit efficiency ({var_label}), NO px fiducial — Sn124 "
            r"(red dashed = $\pm$60)",
            y=0.965,
        )
        assert last_mesh is not None
        cax = fig.add_axes((0.89, 0.10, 0.022, 0.80))
        fig.colorbar(last_mesh, cax=cax, label="efficiency")
        path = out_dir / f"neutron_efficiency_2d_{stem}_nofiducial.png"
        fig.savefig(path, dpi=160)
        fig.savefig(path.with_suffix(".pdf"))
        plt.close(fig)
        out_paths.append(path)
        print(f"[fig] {path}")
    return out_paths


# --------------------------------------------------------------------------- #
def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--reco-base", type=Path, default=Path(RECO_BASE))
    p.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    p.add_argument("--max-files", type=int, default=None)
    p.add_argument(
        "--skip", nargs="*", default=[],
        choices=["grid", "dpx", "phase", "eff1d", "eff2d", "effnofid"],
        help="figure groups to skip",
    )
    return p.parse_args()


def main() -> None:
    args = parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    frames = {}
    for pol in POLARIZATIONS:
        frames[pol] = load_frame(pol, args.reco_base, args.max_files)

    if "grid" not in args.skip:
        plot_r_vs_gamma_grid(frames, args.out_dir)
    if "dpx" not in args.skip:
        plot_dpx_truth_vs_reco_overlay(frames, args.out_dir)
    if "phase" not in args.skip:
        plot_phase_density_maps(frames, args.out_dir)
    if "eff1d" not in args.skip:
        plot_neutron_efficiency_1d_by_cut(frames, args.out_dir)
    if "eff2d" not in args.skip:
        plot_neutron_efficiency_2d_by_cut(frames, args.out_dir)
    if "effnofid" not in args.skip:
        plot_neutron_efficiency_1d_nofiducial(frames, args.out_dir)
        plot_neutron_efficiency_2d_nofiducial(frames, args.out_dir)

    print(f"[done] cut diagnostics saved to {args.out_dir}")


if __name__ == "__main__":
    main()
