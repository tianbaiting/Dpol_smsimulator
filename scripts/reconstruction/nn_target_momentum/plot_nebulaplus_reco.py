#!/usr/bin/env python3
"""Plot NEBULA-Plus joint reconstruction quality report.

Reads _reco.root files directly via uproot, generates error histograms,
efficiency plots, and event-level R observable plots.

Usage:
    python3 scripts/reconstruction/nn_target_momentum/plot_nebulaplus_reco.py
"""

import argparse
import json
import math
import os
import sys
from dataclasses import dataclass
import numpy as np
import pandas as pd
import uproot
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from pathlib import Path

RECO_BASE = os.getenv(
    "RECO_BASE",
    "/home/tian/workspace/sshDir/spana03/Dpol_smsimulator"
    "/data/reconstruction/nebulaplus_nn_joint_20260609_012854",
)
FIG_DIR = os.path.join(RECO_BASE, "checks", "figures")
OBS_DIR = os.path.join(RECO_BASE, "checks", "observables")
NEUTRON_MASS_MEV = 939.565

BRANCHES = [
    "truth_proton_p4",
    "truth_has_proton",
    "reco_proton_px",
    "reco_proton_py",
    "reco_proton_pz",
    "reco_proton_status",
]

OBSERVABLE_BRANCHES = [
    "truth_has_proton",
    "truth_has_neutron",
    "truth_proton_p4",
    "truth_neutron_p4",
    "reco_proton_px",
    "reco_proton_py",
    "reco_proton_pz",
    "reco_proton_status",
    "recoEvent/neutrons/neutrons.beta",
    "recoEvent/neutrons/neutrons.direction",
    "recoEvent/neutrons/neutrons.hitMultiplicity",
]

plt.rcParams.update({
    "figure.figsize": (8, 5),
    "figure.dpi": 150,
    "font.size": 12,
    "axes.titlesize": 14,
    "axes.labelsize": 12,
    "legend.fontsize": 10,
    "figure.autolayout": True,
})


@dataclass(frozen=True)
class RatioResult:
    r: float
    r_lo: float
    r_hi: float
    n_pos: int
    n_neg: int
    n_used: int


def find_reco_files(base_dir, pol=None):
    out = []
    for p in sorted(Path(base_dir).rglob("*_reco.root")):
        path_s = str(p)
        if pol == "ypol" and "y_pol" not in path_s:
            continue
        if pol == "zpol" and "z_pol" not in path_s:
            continue
        out.append(str(p))
    return out


def first_or_value(arr, default=np.nan, dtype=np.float64):
    import awkward as ak

    first = ak.firsts(arr)
    filled = ak.fill_none(first, default)
    return np.asarray(filled, dtype=dtype)


def load_data(files, max_files=None):
    import awkward as ak

    all_chunks = []
    target = files[:max_files] if max_files else files
    for i, path in enumerate(target):
        try:
            f = uproot.open(path)
        except Exception as e:
            print(f"  [WARN] cannot open {path}: {e}")
            continue
        if "recoTree" not in f:
            print(f"  [WARN] no recoTree in {path}")
            f.close()
            continue

        tree = f["recoTree"]
        need = [b for b in BRANCHES if b in tree.keys()]
        if len(need) < len(BRANCHES):
            missing = set(BRANCHES) - set(need)
            print(f"  [WARN] missing branches {missing} in {path}")
            f.close()
            continue

        arrs = tree.arrays(BRANCHES)
        n = len(arrs)

        tp4 = arrs["truth_proton_p4"]
        tpx = np.asarray(tp4.fP.fX, dtype=np.float64)
        tpy = np.asarray(tp4.fP.fY, dtype=np.float64)
        tpz = np.asarray(tp4.fP.fZ, dtype=np.float64)
        has_p = np.asarray(arrs["truth_has_proton"], dtype=bool)

        rpx_all = arrs["reco_proton_px"]
        rpy_all = arrs["reco_proton_py"]
        rpz_all = arrs["reco_proton_pz"]
        rstat_all = arrs["reco_proton_status"]

        rpx = first_or_value(rpx_all)
        rpy = first_or_value(rpy_all)
        rpz = first_or_value(rpz_all)
        rstat = first_or_value(rstat_all, default=-1, dtype=np.int32)

        tag = 0 if "y_pol" in path else 1
        source = np.full(n, tag, dtype=np.int32)

        all_chunks.append((tpx, tpy, tpz, rpx, rpy, rpz, has_p, rstat, source))
        f.close()

        if (i + 1) % 20 == 0 or (i + 1) == len(target):
            total_so_far = sum(len(c[0]) for c in all_chunks)
            print(f"  loaded {i+1}/{len(target)} files, {total_so_far:,} events so far")

    if not all_chunks:
        return {
            "truth_px": np.array([]),
            "truth_py": np.array([]),
            "truth_pz": np.array([]),
            "reco_px": np.array([]),
            "reco_py": np.array([]),
            "reco_pz": np.array([]),
            "has_proton": np.array([], dtype=bool),
            "reco_status": np.array([], dtype=np.int32),
            "source": np.array([], dtype=np.int32),
        }

    arrays = [np.concatenate([c[i] for c in all_chunks]) for i in range(9)]
    return {
        "truth_px": arrays[0],
        "truth_py": arrays[1],
        "truth_pz": arrays[2],
        "reco_px": arrays[3],
        "reco_py": arrays[4],
        "reco_pz": arrays[5],
        "has_proton": arrays[6],
        "reco_status": arrays[7],
        "source": arrays[8],
    }


def parse_cell_from_path(path):
    p = Path(path)
    cell = p.parent.name
    body = cell[2:] if cell.startswith("d+") else cell
    hel = body[-3:]
    target_gamma = body[:-3]
    g_idx = target_gamma.rfind("g")
    target = target_gamma[:g_idx]
    gamma = target_gamma[g_idx:]
    pol = "ypol" if hel.startswith("y") or "y_pol" in str(p) else "zpol"
    stem = p.stem
    b_segment = ""
    if stem.startswith("dbreakb"):
        b_segment = stem.replace("_reco", "").replace("dbreak", "")
    return {
        "tag": cell,
        "target": target,
        "gamma": gamma,
        "hel": hel,
        "pol": pol,
        "b_segment": b_segment,
    }


def rotate_to_reaction_plane(pxp, pyp, pxn, pyn):
    pxp = np.asarray(pxp, dtype=np.float64)
    pyp = np.asarray(pyp, dtype=np.float64)
    pxn = np.asarray(pxn, dtype=np.float64)
    pyn = np.asarray(pyn, dtype=np.float64)
    phi = np.arctan2(pyp + pyn, pxp + pxn)
    cos_a = np.cos(-phi)
    sin_a = np.sin(-phi)
    rot_pxp = cos_a * pxp - sin_a * pyp
    rot_pyp = sin_a * pxp + cos_a * pyp
    rot_pxn = cos_a * pxn - sin_a * pyn
    rot_pyn = sin_a * pxn + cos_a * pyn
    return rot_pxp, rot_pyp, rot_pxn, rot_pyn, phi


def compute_cut_masks(df, pol):
    sum_px = df["truth_pxp"] + df["truth_pxn"]
    sum_py = df["truth_pyp"] + df["truth_pyn"]
    sum_xy2 = sum_px * sum_px + sum_py * sum_py
    phi = np.arctan2(sum_py, sum_px)

    if pol == "ypol":
        loose = (np.abs(df["truth_pyp"] - df["truth_pyn"]) < 150.0) & (sum_xy2 > 2500.0)
        mid = loose & (np.sqrt(sum_xy2) < 200.0)
        tight = mid & ((math.pi - np.abs(phi)) < 0.2)
    elif pol == "zpol":
        loose = ((df["truth_pzp"] + df["truth_pzn"]) > 1150.0) & (
            np.abs(df["truth_pzp"] - df["truth_pzn"]) < 150.0
        )
        mid = loose & ((df["truth_pxp"] + df["truth_pxn"]) < 200.0) & (sum_xy2 > 2500.0)
        tight = mid & ((math.pi - np.abs(phi)) < 0.5)
    else:
        raise ValueError(f"unknown polarization: {pol}")

    return {
        "loose": pd.Series(loose, index=df.index, dtype=bool),
        "mid": pd.Series(mid, index=df.index, dtype=bool),
        "tight": pd.Series(tight, index=df.index, dtype=bool),
    }


def truth_pxn_fiducial_mask(df, limit):
    return pd.Series(np.abs(df["truth_pxn"]) < float(limit), index=df.index, dtype=bool)


def reco_pxn_fiducial_mask(df, limit):
    mask = (df["n_reco_neutrons"] > 0) & np.isfinite(df["reco_pxn"]) & (np.abs(df["reco_pxn"]) < float(limit))
    if "reco_pyn" in df:
        mask = mask & np.isfinite(df["reco_pyn"])
    return pd.Series(mask, index=df.index, dtype=bool)


def fiducial_px60_mask(df):
    return truth_pxn_fiducial_mask(df, 60.0)


def fiducial_px100_mask(df):
    return truth_pxn_fiducial_mask(df, 100.0)


def reco_pxn_px60_mask(df):
    return reco_pxn_fiducial_mask(df, 60.0)


def reco_pxn_px100_mask(df):
    return reco_pxn_fiducial_mask(df, 100.0)


def add_observable_columns(df):
    df = df.copy()
    tr_pxp, tr_pyp, tr_pxn, tr_pyn, phi = rotate_to_reaction_plane(
        df["truth_pxp"].to_numpy(),
        df["truth_pyp"].to_numpy(),
        df["truth_pxn"].to_numpy(),
        df["truth_pyn"].to_numpy(),
    )
    reco_plane_nn_pxp, reco_plane_nn_pyp, reco_plane_reco_pxn, reco_plane_reco_pyn, phi_reco = rotate_to_reaction_plane(
        df["nn_pxp"].to_numpy(),
        df["nn_pyp"].to_numpy(),
        df["reco_pxn"].to_numpy(),
        df["reco_pyn"].to_numpy(),
    )
    cos_a = np.cos(-phi)
    sin_a = np.sin(-phi)
    nn_pxp = cos_a * df["nn_pxp"].to_numpy() - sin_a * df["nn_pyp"].to_numpy()
    nn_pyp = sin_a * df["nn_pxp"].to_numpy() + cos_a * df["nn_pyp"].to_numpy()
    reco_pxn = cos_a * df["reco_pxn"].to_numpy() - sin_a * df["reco_pyn"].to_numpy()
    reco_pyn = sin_a * df["reco_pxn"].to_numpy() + cos_a * df["reco_pyn"].to_numpy()
    df["phi_event"] = phi
    df["truth_rot_pxp"] = tr_pxp
    df["truth_rot_pyp"] = tr_pyp
    df["truth_rot_pxn"] = tr_pxn
    df["truth_rot_pyn"] = tr_pyn
    df["nn_rot_pxp"] = nn_pxp
    df["nn_rot_pyp"] = nn_pyp
    df["reco_rot_pxn"] = reco_pxn
    df["reco_rot_pyn"] = reco_pyn
    df["phi_reco_event"] = phi_reco
    df["reco_plane_nn_pxp"] = reco_plane_nn_pxp
    df["reco_plane_nn_pyp"] = reco_plane_nn_pyp
    df["reco_plane_reco_pxn"] = reco_plane_reco_pxn
    df["reco_plane_reco_pyn"] = reco_plane_reco_pyn
    return df


def finite_values(values):
    arr = np.asarray(values, dtype=np.float64)
    return arr[np.isfinite(arr)]


def build_variant_dpx(df):
    truth = df["truth_rot_pxp"].to_numpy() - df["truth_rot_pxn"].to_numpy()
    neutron_ok = (df["n_reco_neutrons"].to_numpy() > 0) & np.isfinite(df["reco_rot_pxn"].to_numpy())
    mixed_neutron = np.where(neutron_ok, df["reco_rot_pxn"].to_numpy(), df["truth_rot_pxn"].to_numpy())
    mixed = df["nn_rot_pxp"].to_numpy() - mixed_neutron
    full = df["nn_rot_pxp"].to_numpy()[neutron_ok] - df["reco_rot_pxn"].to_numpy()[neutron_ok]
    return {
        "truth": finite_values(truth),
        "mixed": finite_values(mixed),
        "full": finite_values(full),
    }


def build_reco_plane_dpx(df, truth_mask, reco_mask):
    truth_sub = df[truth_mask]
    reco_sub = df[reco_mask]
    truth = truth_sub["truth_rot_pxp"].to_numpy() - truth_sub["truth_rot_pxn"].to_numpy()
    full_reco = reco_sub["reco_plane_nn_pxp"].to_numpy() - reco_sub["reco_plane_reco_pxn"].to_numpy()
    return {
        "truth": finite_values(truth),
        "full_reco": finite_values(full_reco),
    }


def compute_pxn_sign_efficiency(df, mask, pol, cut, fiducial, target="", gamma=""):
    sub = df[mask]
    px = sub["truth_pxn"].to_numpy()
    hit = (sub["n_reco_neutrons"].to_numpy() > 0) & np.isfinite(sub["reco_pxn"].to_numpy())
    pos = px > 0.0
    neg = px < 0.0

    def ratio(num, den):
        return float(num / den) if den > 0 else float("nan")

    n = int(len(sub))
    n_pos = int(np.sum(pos))
    n_neg = int(np.sum(neg))
    n_hit = int(np.sum(hit))
    n_hit_pos = int(np.sum(hit & pos))
    n_hit_neg = int(np.sum(hit & neg))
    eps_pos = ratio(n_hit_pos, n_pos)
    eps_neg = ratio(n_hit_neg, n_neg)
    avg = 0.5 * (eps_pos + eps_neg)
    diff = eps_pos - eps_neg if np.isfinite(eps_pos) and np.isfinite(eps_neg) else float("nan")
    rel_diff = diff / avg if np.isfinite(diff) and avg != 0.0 else float("nan")
    return {
        "pol": pol,
        "target": target,
        "gamma": gamma,
        "cut": cut,
        "fiducial": fiducial,
        "N": n,
        "N_pos": n_pos,
        "N_neg": n_neg,
        "N_hit": n_hit,
        "N_hit_pos": n_hit_pos,
        "N_hit_neg": n_hit_neg,
        "eps_all": ratio(n_hit, n),
        "eps_pos": eps_pos,
        "eps_neg": eps_neg,
        "eff_diff": diff,
        "rel_eff_diff": rel_diff,
    }


def ratio_with_bootstrap(values, n_boot=500, rng_seed=20260609):
    arr = finite_values(values)
    signs = arr[arr != 0.0]
    n_pos = int(np.sum(signs > 0.0))
    n_neg = int(np.sum(signs < 0.0))
    n_used = n_pos + n_neg
    if n_used == 0:
        return RatioResult(float("nan"), float("nan"), float("nan"), n_pos, n_neg, n_used)
    if n_neg == 0:
        return RatioResult(float("inf"), float("nan"), float("nan"), n_pos, n_neg, n_used)

    r = n_pos / n_neg
    if n_boot <= 0 or n_used < 50:
        return RatioResult(float(r), float("nan"), float("nan"), n_pos, n_neg, n_used)

    rng = np.random.default_rng(rng_seed)
    p_pos = n_pos / n_used
    boot_pos = rng.binomial(n_used, p_pos, size=n_boot)
    boot_neg = n_used - boot_pos
    valid = boot_neg > 0
    if not np.any(valid):
        return RatioResult(float(r), float("nan"), float("nan"), n_pos, n_neg, n_used)
    boot_r = boot_pos[valid] / boot_neg[valid]
    lo, hi = np.percentile(boot_r, [16.0, 84.0])
    return RatioResult(float(r), float(lo), float(hi), n_pos, n_neg, n_used)


def build_r_table_for_mask(df, pol, n_boot, fiducial_name, fiducial_mask):
    rows = []
    for (target, gamma), sub in df.groupby(["target", "gamma"], sort=True, observed=True):
        sub_fid = sub[fiducial_mask.loc[sub.index].to_numpy()]
        for cut in ("loose", "mid", "tight"):
            sub_cut = sub_fid[sub_fid[cut]]
            variants = build_variant_dpx(sub_cut)
            for variant, values in variants.items():
                result = ratio_with_bootstrap(values, n_boot=n_boot)
                rows.append(
                    {
                        "pol": pol,
                        "target": target,
                        "gamma": gamma,
                        "cut": cut,
                        "fiducial": fiducial_name,
                        "variant": variant,
                        "N": result.n_used,
                        "n_pos": result.n_pos,
                        "n_neg": result.n_neg,
                        "R": result.r,
                        "R_lo": result.r_lo,
                        "R_hi": result.r_hi,
                    }
                )
    return pd.DataFrame(rows)


def build_pxn_sign_efficiency_table(df, pol, fiducial_masks):
    rows = []
    for (target, gamma), sub in df.groupby(["target", "gamma"], sort=True, observed=True):
        base_index = sub.index
        for cut in ("loose", "mid", "tight"):
            cut_mask = pd.Series(sub[cut].to_numpy(), index=base_index, dtype=bool)
            for fiducial, mask in fiducial_masks.items():
                local_mask = cut_mask & mask.loc[base_index]
                rows.append(
                    compute_pxn_sign_efficiency(
                        sub,
                        local_mask,
                        pol=pol,
                        target=target,
                        gamma=gamma,
                        cut=cut,
                        fiducial=fiducial,
                    )
                )
    return pd.DataFrame(rows)


def load_observable_frame(files, pol, max_files=None):
    import awkward as ak

    chunks = []
    target = files[:max_files] if max_files else files
    for i, path in enumerate(target):
        try:
            f = uproot.open(path)
        except Exception as e:
            print(f"  [WARN] cannot open {path}: {e}")
            continue
        if "recoTree" not in f:
            print(f"  [WARN] no recoTree in {path}")
            f.close()
            continue
        tree = f["recoTree"]
        missing = [b for b in OBSERVABLE_BRANCHES if b not in tree.keys()]
        if missing:
            print(f"  [WARN] missing observable branches {missing} in {path}")
            f.close()
            continue

        arrs = tree.arrays(OBSERVABLE_BRANCHES)
        n = len(arrs)
        if n == 0:
            f.close()
            continue

        tp4 = arrs["truth_proton_p4"]
        tn4 = arrs["truth_neutron_p4"]
        beta = arrs["recoEvent/neutrons/neutrons.beta"]
        direction = ak.firsts(arrs["recoEvent/neutrons/neutrons.direction"])
        first_beta = first_or_value(beta)
        valid_beta = np.isfinite(first_beta) & (first_beta > 0.0) & (first_beta < 1.0)
        gamma_n = np.full(n, np.nan)
        gamma_n[valid_beta] = 1.0 / np.sqrt(1.0 - first_beta[valid_beta] * first_beta[valid_beta])
        reco_p_mag = NEUTRON_MASS_MEV * gamma_n * first_beta
        dir_x = np.asarray(ak.fill_none(direction.fX, np.nan), dtype=np.float64)
        dir_y = np.asarray(ak.fill_none(direction.fY, np.nan), dtype=np.float64)
        dir_z = np.asarray(ak.fill_none(direction.fZ, np.nan), dtype=np.float64)
        meta = parse_cell_from_path(path)

        chunk = pd.DataFrame(
            {
                "tag": meta["tag"],
                "target": meta["target"],
                "gamma": meta["gamma"],
                "hel": meta["hel"],
                "pol": meta["pol"],
                "b_segment": meta["b_segment"],
                "truth_has_proton": np.asarray(arrs["truth_has_proton"], dtype=bool),
                "truth_has_neutron": np.asarray(arrs["truth_has_neutron"], dtype=bool),
                "truth_pxp": np.asarray(tp4.fP.fX, dtype=np.float64),
                "truth_pyp": np.asarray(tp4.fP.fY, dtype=np.float64),
                "truth_pzp": np.asarray(tp4.fP.fZ, dtype=np.float64),
                "truth_pxn": np.asarray(tn4.fP.fX, dtype=np.float64),
                "truth_pyn": np.asarray(tn4.fP.fY, dtype=np.float64),
                "truth_pzn": np.asarray(tn4.fP.fZ, dtype=np.float64),
                "nn_pxp": first_or_value(arrs["reco_proton_px"]),
                "nn_pyp": first_or_value(arrs["reco_proton_py"]),
                "nn_pzp": first_or_value(arrs["reco_proton_pz"]),
                "nn_status": first_or_value(arrs["reco_proton_status"], default=-1, dtype=np.int32),
                "n_reco_neutrons": np.asarray(ak.num(beta, axis=-1), dtype=np.int32),
                "reco_pxn": reco_p_mag * dir_x,
                "reco_pyn": reco_p_mag * dir_y,
                "reco_pzn": reco_p_mag * dir_z,
                "hit_mult_n": first_or_value(
                    arrs["recoEvent/neutrons/neutrons.hitMultiplicity"],
                    default=-1,
                    dtype=np.int32,
                ),
            }
        )
        chunks.append(chunk)
        f.close()

        if (i + 1) % 20 == 0 or (i + 1) == len(target):
            total_so_far = sum(len(c) for c in chunks)
            print(f"  observable {pol}: loaded {i+1}/{len(target)} files, {total_so_far:,} events")

    if not chunks:
        return pd.DataFrame()

    df = pd.concat(chunks, ignore_index=True)
    for col in ("tag", "target", "gamma", "hel", "pol", "b_segment"):
        df[col] = df[col].astype("category")
    df = df[(df["pol"] == pol) & df["truth_has_proton"] & df["truth_has_neutron"]].copy()
    nn_ok = (
        (df["nn_status"] == 0)
        & np.isfinite(df["nn_pxp"])
        & np.isfinite(df["nn_pyp"])
        & np.isfinite(df["nn_pzp"])
    )
    df = df[nn_ok].copy()
    masks = compute_cut_masks(df, pol)
    for name, mask in masks.items():
        df[name] = mask.to_numpy()
    return add_observable_columns(df)


def build_tables(df, pol, n_boot):
    pass_rows = []
    r_rows = []
    for (target, gamma), sub in df.groupby(["target", "gamma"], sort=True, observed=True):
        pass_rows.append(
            {
                "pol": pol,
                "target": target,
                "gamma": gamma,
                "N_raw": int(len(sub)),
                "N_loose": int(sub["loose"].sum()),
                "N_mid": int(sub["mid"].sum()),
                "N_tight": int(sub["tight"].sum()),
                "N_NEBULA_in_tight": int(((sub["tight"]) & (sub["n_reco_neutrons"] > 0)).sum()),
            }
        )
        for cut in ("loose", "mid", "tight"):
            sub_cut = sub[sub[cut]]
            variants = build_variant_dpx(sub_cut)
            for variant, values in variants.items():
                result = ratio_with_bootstrap(values, n_boot=n_boot)
                r_rows.append(
                    {
                        "pol": pol,
                        "target": target,
                        "gamma": gamma,
                        "cut": cut,
                        "variant": variant,
                        "N": result.n_used,
                        "n_pos": result.n_pos,
                        "n_neg": result.n_neg,
                        "R": result.r,
                        "R_lo": result.r_lo,
                        "R_hi": result.r_hi,
                    }
                )
    return pd.DataFrame(pass_rows), pd.DataFrame(r_rows)


def save_figure_pair(fig, path_no_suffix):
    path_no_suffix = Path(path_no_suffix)
    fig.savefig(path_no_suffix.with_suffix(".png"), dpi=130)
    fig.savefig(path_no_suffix.with_suffix(".pdf"))


def plot_r_vs_gamma(r_df, pol, cut, out_base, title_suffix=""):
    targets = sorted(r_df["target"].unique())
    if not targets:
        return
    share = "all" if pol == "ypol" else False
    fig, axes = plt.subplots(1, len(targets), figsize=(6 * len(targets), 4), sharey=share, squeeze=False)
    styles = [
        ("truth", "black", "o", "-"),
        ("mixed", "C1", "s", "--"),
        ("full", "C0", "^", ":"),
    ]
    for i, target in enumerate(targets):
        ax = axes[0, i]
        sub = r_df[(r_df["target"] == target) & (r_df["cut"] == cut)]
        plotted_any = False
        for variant, color, marker, ls in styles:
            v = sub[sub["variant"] == variant].sort_values("gamma")
            v = v[np.isfinite(v["R"].to_numpy())]
            if v.empty:
                continue
            plotted_any = True
            yerr = None
            if np.isfinite(v["R_lo"].to_numpy()).all() and np.isfinite(v["R_hi"].to_numpy()).all():
                yerr = np.vstack((v["R"].to_numpy() - v["R_lo"].to_numpy(), v["R_hi"].to_numpy() - v["R"].to_numpy()))
            ax.errorbar(
                v["gamma"],
                v["R"],
                yerr=yerr,
                color=color,
                marker=marker,
                ls=ls,
                label=variant,
                lw=1.5,
                ms=5,
                capsize=3,
            )
        ax.axhline(1.0, color="gray", linestyle=":", linewidth=0.8)
        ax.set_title(target)
        ax.set_xlabel("gamma")
        if i == 0 or not share:
            ax.set_ylabel(r"$R = N(\Delta p_x^{rot}>0)/N(\Delta p_x^{rot}<0)$")
        ax.grid(True, alpha=0.3)
        if i == 0 and plotted_any:
            ax.legend(fontsize=9)
    fig.suptitle(f"{pol} R vs gamma ({cut} cut{title_suffix})")
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_px_diff_hist(df, pol, cut, out_base, title_suffix=""):
    targets = sorted(df["target"].unique())
    gammas = sorted(df["gamma"].unique())
    if not targets or not gammas:
        return
    fig, axes = plt.subplots(len(targets), len(gammas), figsize=(4 * len(gammas), 3 * len(targets)), squeeze=False)
    for i, target in enumerate(targets):
        for j, gamma in enumerate(gammas):
            ax = axes[i, j]
            sub = df[(df["target"] == target) & (df["gamma"] == gamma) & df[cut]]
            if sub.empty:
                ax.set_title(f"{target} {gamma} empty", fontsize=9)
                continue
            variants = build_variant_dpx(sub)
            ax.hist(variants["truth"], bins=80, range=(-300, 300), histtype="step", color="black", lw=1.4, label=f"truth N={len(variants['truth'])}")
            ax.hist(variants["mixed"], bins=80, range=(-300, 300), histtype="step", color="C1", lw=1.2, label=f"mixed N={len(variants['mixed'])}")
            if len(variants["full"]) > 0:
                ax.hist(variants["full"], bins=80, range=(-300, 300), histtype="step", color="C0", lw=1.2, ls="--", label=f"full N={len(variants['full'])}")
            ax.axvline(0.0, color="gray", linestyle=":", linewidth=0.8)
            ax.set_title(f"{target} {gamma}", fontsize=9)
            ax.set_xlabel(r"$\Delta p_x^{rot}$ (MeV/c)")
            ax.set_ylabel("counts")
            ax.grid(True, alpha=0.3)
            if i == 0 and j == 0:
                ax.legend(fontsize=7)
    fig.suptitle(f"{pol} $\\Delta p_x^{{rot}}$ ({cut} cut{title_suffix})")
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_reco_plane_px_diff_hist(df, pol, cut, out_base, px_limit=60.0):
    targets = sorted(df["target"].unique())
    gammas = sorted(df["gamma"].unique())
    if not targets or not gammas:
        return
    label = f"px{int(px_limit)}" if float(px_limit).is_integer() else f"px{px_limit:g}"
    fig, axes = plt.subplots(len(targets), len(gammas), figsize=(4 * len(gammas), 3 * len(targets)), squeeze=False)
    for i, target in enumerate(targets):
        for j, gamma in enumerate(gammas):
            ax = axes[i, j]
            sub = df[(df["target"] == target) & (df["gamma"] == gamma) & df[cut]]
            if sub.empty:
                ax.set_title(f"{target} {gamma} empty", fontsize=9)
                continue
            variants = build_reco_plane_dpx(
                sub,
                truth_pxn_fiducial_mask(sub, px_limit),
                reco_pxn_fiducial_mask(sub, px_limit),
            )
            ax.hist(
                variants["truth"],
                bins=80,
                range=(-300, 300),
                histtype="step",
                color="black",
                lw=1.4,
                label=f"truth cut N={len(variants['truth'])}",
            )
            if len(variants["full_reco"]) > 0:
                ax.hist(
                    variants["full_reco"],
                    bins=80,
                    range=(-300, 300),
                    histtype="step",
                    color="C0",
                    lw=1.2,
                    ls="--",
                    label=f"reco cut N={len(variants['full_reco'])}",
                )
            ax.axvline(0.0, color="gray", linestyle=":", linewidth=0.8)
            ax.set_title(f"{target} {gamma}", fontsize=9)
            ax.set_xlabel(r"$\Delta p_x$ (MeV/c)")
            ax.set_ylabel("counts")
            ax.grid(True, alpha=0.3)
            if i == 0 and j == 0:
                ax.legend(fontsize=7)
    fig.suptitle(f"{pol} $\\Delta p_x$: truth-cut vs reco-plane reco-cut ({cut}, {label})")
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_px_diff_cuts_overlay(df, pol, target, out_base):
    gammas = sorted(df["gamma"].unique())
    if not gammas:
        return
    fig, axes = plt.subplots(len(gammas), 2, figsize=(12, 3.3 * len(gammas)), squeeze=False)
    cut_styles = [
        ("none", "C0", "no cut"),
        ("loose", "C2", "loose"),
        ("mid", "C1", "mid"),
        ("tight", "C3", "tight"),
    ]
    for i, gamma in enumerate(gammas):
        base = df[(df["target"] == target) & (df["gamma"] == gamma)]
        for col, variant in enumerate(("truth", "mixed")):
            ax = axes[i, col]
            for cut, color, label in cut_styles:
                sub = base if cut == "none" else base[base[cut]]
                if sub.empty:
                    continue
                values = build_variant_dpx(sub)[variant]
                if len(values) == 0:
                    continue
                ax.hist(values, bins=80, range=(-300, 300), histtype="step", color=color, lw=1.4, label=f"{label} N={len(values)}")
            ax.axvline(0.0, color="gray", linestyle=":", linewidth=0.8)
            ax.set_title(f"{target} {gamma} {variant}", fontsize=9)
            ax.set_xlabel(r"$\Delta p_x^{rot}$ (MeV/c)")
            ax.set_ylabel("counts")
            ax.set_yscale("log")
            ax.grid(True, alpha=0.3)
            if i == 0:
                ax.legend(fontsize=7)
    fig.suptitle(f"{pol} {target}: $\\Delta p_x^{{rot}}$ cut overlay")
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    save_figure_pair(fig, out_base)
    plt.close(fig)


def plot_pxn_sign_efficiency(eff_df, pol, cut, fiducial, out_base):
    sub = eff_df[(eff_df["pol"] == pol) & (eff_df["cut"] == cut) & (eff_df["fiducial"] == fiducial)]
    targets = sorted(sub["target"].unique())
    if not targets:
        return
    fig, axes = plt.subplots(1, len(targets), figsize=(6 * len(targets), 4), sharey=True, squeeze=False)
    for i, target in enumerate(targets):
        ax = axes[0, i]
        v = sub[sub["target"] == target].sort_values("gamma")
        x = np.arange(len(v))
        width = 0.36
        ax.bar(x - width / 2, v["eps_pos"], width=width, color="C3", alpha=0.8, label=r"$p_x^n>0$")
        ax.bar(x + width / 2, v["eps_neg"], width=width, color="C0", alpha=0.8, label=r"$p_x^n<0$")
        ax.set_xticks(x)
        ax.set_xticklabels(v["gamma"])
        ax.set_ylim(0.0, 1.0)
        ax.set_title(target)
        ax.set_xlabel("gamma")
        ax.grid(True, axis="y", alpha=0.3)
        if i == 0:
            ax.set_ylabel("neutron reco efficiency")
            ax.legend(fontsize=9)
        for xi, rel in zip(x, v["rel_eff_diff"]):
            if np.isfinite(rel):
                ax.text(xi, 0.96, f"{rel:+.2f}", ha="center", va="top", fontsize=8)
    fig.suptitle(f"{pol} neutron efficiency by sign ({cut} cut, {fiducial})")
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    save_figure_pair(fig, out_base)
    plt.close(fig)


def write_joined_if_requested(df, out_dir, pol, write_joined):
    if not write_joined:
        return
    out_dir = Path(out_dir)
    try:
        df.to_parquet(out_dir / f"joined_{pol}.parquet", index=False)
        print(f"  wrote {out_dir / f'joined_{pol}.parquet'}")
    except Exception as e:
        print(f"  [WARN] cannot write joined_{pol}.parquet: {e}")


def run_observable_analysis(base_dir, fig_dir, out_dir, pols, max_files=None, n_boot=500, write_joined=False):
    out_dir = Path(out_dir)
    fig_dir = Path(fig_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    fig_dir.mkdir(parents=True, exist_ok=True)

    all_pass = []
    all_r = []
    all_r_px100 = []
    all_eff = []
    summaries = {}
    for pol in pols:
        files = find_reco_files(base_dir, pol=pol)
        print(f"\nObservable analysis: {pol}, {len(files)} files")
        if not files:
            print(f"  [WARN] no files for {pol}")
            continue
        df = load_observable_frame(files, pol, max_files=max_files)
        print(f"  {pol}: {len(df):,} events after truth+NN filters")
        if df.empty:
            continue
        pass_df, r_df = build_tables(df, pol, n_boot=n_boot)
        px100_mask = fiducial_px100_mask(df)
        fiducial_masks = {
            "baseline": pd.Series(True, index=df.index, dtype=bool),
            "px100": px100_mask,
        }
        r_px100_df = build_r_table_for_mask(df, pol, n_boot=n_boot, fiducial_name="px100", fiducial_mask=px100_mask)
        eff_df = build_pxn_sign_efficiency_table(df, pol, fiducial_masks)
        pass_df.to_csv(out_dir / f"cell_passrates_{pol}.csv", index=False)
        r_df.to_csv(out_dir / f"r_table_{pol}.csv", index=False)
        r_px100_df.to_csv(out_dir / f"r_table_px100_{pol}.csv", index=False)
        eff_df.to_csv(out_dir / f"efficiency_pxn_sign_px100_{pol}.csv", index=False)
        write_joined_if_requested(df, out_dir, pol, write_joined)
        all_pass.append(pass_df)
        all_r.append(r_df)
        all_r_px100.append(r_px100_df)
        all_eff.append(eff_df)
        summaries[pol] = {
            "n_files": len(files[:max_files] if max_files else files),
            "n_events_after_filter": int(len(df)),
            "n_loose": int(df["loose"].sum()),
            "n_mid": int(df["mid"].sum()),
            "n_tight": int(df["tight"].sum()),
            "n_nebula_in_tight": int(((df["tight"]) & (df["n_reco_neutrons"] > 0)).sum()),
            "n_px100": int(px100_mask.sum()),
            "n_loose_px100": int((df["loose"] & px100_mask).sum()),
            "n_mid_px100": int((df["mid"] & px100_mask).sum()),
            "n_tight_px100": int((df["tight"] & px100_mask).sum()),
        }

        for cut in ("loose", "mid", "tight"):
            plot_px_diff_hist(df, pol, cut, fig_dir / f"px_diff_hist_{pol}_{cut}")
            plot_r_vs_gamma(r_df, pol, cut, fig_dir / f"r_vs_gamma_{pol}_{cut}")
            plot_px_diff_hist(df[px100_mask], pol, cut, fig_dir / f"px_diff_hist_{pol}_{cut}_px100", title_suffix=", px100")
            plot_r_vs_gamma(r_px100_df, pol, cut, fig_dir / f"r_vs_gamma_{pol}_{cut}_px100", title_suffix=", px100")
            plot_reco_plane_px_diff_hist(df, pol, cut, fig_dir / f"px_diff_hist_{pol}_{cut}_reco_plane_px60", px_limit=60.0)
            plot_pxn_sign_efficiency(eff_df, pol, cut, "px100", fig_dir / f"efficiency_pxn_sign_{pol}_{cut}_px100")
        for target in sorted(df["target"].unique()):
            plot_px_diff_cuts_overlay(df, pol, target, fig_dir / f"px_diff_cuts_overlay_{pol}_{target}")

    if all_pass:
        pd.concat(all_pass, ignore_index=True).to_csv(out_dir / "cell_passrates.csv", index=False)
    if all_r:
        pd.concat(all_r, ignore_index=True).to_csv(out_dir / "r_table.csv", index=False)
    if all_r_px100:
        pd.concat(all_r_px100, ignore_index=True).to_csv(out_dir / "r_table_px100.csv", index=False)
    if all_eff:
        pd.concat(all_eff, ignore_index=True).to_csv(out_dir / "efficiency_pxn_sign_px100.csv", index=False)
    (out_dir / "summary.json").write_text(json.dumps(summaries, indent=2), encoding="utf-8")
    print(f"\nObservable tables saved to {out_dir}")


def plot_error_hist(ax, data, label, color, bins_range=None):
    if bins_range is None:
        std = np.nanstd(data)
        rng = min(5 * std, 100)
        bins_range = (-rng, rng)
    ax.hist(
        data, bins=120, range=bins_range,
        density=True, alpha=0.75, color=color, edgecolor="none",
    )
    mae = np.nanmean(np.abs(data))
    rmse = np.sqrt(np.nanmean(data ** 2))
    bias = np.nanmean(data)
    sigma = np.nanstd(data)
    p95 = np.nanpercentile(np.abs(data), 95)
    ax.axvline(bias, color="k", ls="--", lw=1, alpha=0.7)
    text = (
        f"MAE = {mae:.3f}\n"
        f"RMSE = {rmse:.3f}\n"
        f"Bias = {bias:.3f}\n"
        f"$\\sigma$ = {sigma:.3f}\n"
        f"P95 = {p95:.3f}"
    )
    ax.text(
        0.97, 0.95, text,
        transform=ax.transAxes, fontsize=10,
        verticalalignment="top", horizontalalignment="right",
        bbox=dict(boxstyle="round,pad=0.3", facecolor="wheat", alpha=0.8),
    )
    ax.set_xlabel(f"$\\Delta {label}$ (MeV/c)")
    ax.set_ylabel("Density")
    ax.set_title(f"$\\Delta {label}$ distribution")
    return mae, rmse, bias, sigma, p95


def plot_overlay_hist(ax, data_ypol, data_zpol, label, color_ypol, color_zpol, bins_range=None):
    if bins_range is None:
        std = max(np.nanstd(data_ypol), np.nanstd(data_zpol))
        rng = min(5 * std, 100)
        bins_range = (-rng, rng)
    ax.hist(data_ypol, bins=120, range=bins_range, density=True, alpha=0.6, color=color_ypol, label="ypol", edgecolor="none")
    ax.hist(data_zpol, bins=120, range=bins_range, density=True, alpha=0.6, color=color_zpol, label="zpol", edgecolor="none")
    ax.legend()
    ax.set_xlabel(f"$\\Delta {label}$ (MeV/c)")
    ax.set_ylabel("Density")
    ax.set_title(f"$\\Delta {label}$: ypol vs zpol")


def run_quality_plots(base_dir, fig_dir, pol=None, max_files=None):
    files = find_reco_files(base_dir, pol=pol)
    print(f"Found {len(files)} _reco.root files under {base_dir}")
    if not files:
        print("[ERROR] no _reco.root files found")
        sys.exit(1)

    fig_dir = Path(fig_dir)
    fig_dir.mkdir(parents=True, exist_ok=True)

    print("Loading data from ROOT files ...")
    data = load_data(files, max_files=max_files)
    n = len(data["truth_px"])
    print(f"Total {n:,} events loaded")
    if n == 0:
        print("[ERROR] no events loaded")
        sys.exit(1)

    has_p = data["has_proton"]
    valid = np.isfinite(data["reco_px"]) & np.isfinite(data["reco_py"]) & np.isfinite(data["reco_pz"])
    matched = has_p & valid & (data["reco_status"] == 0)
    n_matched = matched.sum()

    dpx = data["reco_px"] - data["truth_px"]
    dpy = data["reco_py"] - data["truth_py"]
    dpz = data["reco_pz"] - data["truth_pz"]
    truth_p = np.sqrt(data["truth_px"]**2 + data["truth_py"]**2 + data["truth_pz"]**2)
    reco_p = np.sqrt(data["reco_px"]**2 + data["reco_py"]**2 + data["reco_pz"]**2)
    dp = reco_p - truth_p

    dpx_m = dpx[matched]
    dpy_m = dpy[matched]
    dpz_m = dpz[matched]
    dp_m = dp[matched]

    is_ypol = data["source"] == 0
    is_zpol = data["source"] == 1

    # --- Figure 1: dpx ---
    fig, ax = plt.subplots()
    stats_px = plot_error_hist(ax, dpx_m, "p_x", "#1f77b4")
    fig.savefig(fig_dir / "error_px.pdf")
    fig.savefig(fig_dir / "error_px.png")
    plt.close(fig)

    # --- Figure 2: dpy ---
    fig, ax = plt.subplots()
    stats_py = plot_error_hist(ax, dpy_m, "p_y", "#ff7f0e")
    fig.savefig(fig_dir / "error_py.pdf")
    fig.savefig(fig_dir / "error_py.png")
    plt.close(fig)

    # --- Figure 3: dpz ---
    fig, ax = plt.subplots()
    stats_pz = plot_error_hist(ax, dpz_m, "p_z", "#2ca02c")
    fig.savefig(fig_dir / "error_pz.pdf")
    fig.savefig(fig_dir / "error_pz.png")
    plt.close(fig)

    # --- Figure 4: dp ---
    fig, ax = plt.subplots()
    stats_p = plot_error_hist(ax, dp_m, "|p|", "#d62728")
    fig.savefig(fig_dir / "error_p.pdf")
    fig.savefig(fig_dir / "error_p.png")
    plt.close(fig)

    # --- Figure 5: Efficiency ---
    eff = n_matched / n * 100 if n > 0 else 0
    fig, ax = plt.subplots(figsize=(6, 4))
    bars = ax.bar(
        ["Matched", "Unmatched"],
        [n_matched, n - n_matched],
        color=["#2ca02c", "#d62728"],
        edgecolor="k",
    )
    ax.set_ylabel("Events")
    ax.set_title(f"Reconstruction Efficiency: {eff:.1f}% ({n_matched:,}/{n:,})")
    for bar, count in zip(bars, [n_matched, n - n_matched]):
        ax.text(
            bar.get_x() + bar.get_width() / 2, bar.get_height() + n * 0.01,
            f"{count:,}", ha="center", va="bottom", fontsize=12,
        )
    fig.savefig(fig_dir / "efficiency.pdf")
    fig.savefig(fig_dir / "efficiency.png")
    plt.close(fig)

    # --- Figure 6: ypol vs zpol overlay ---
    for tag, data_arr, color_y, color_z, lbl in [
        ("px", dpx, "#1f77b4", "#9467bd", "p_x"),
        ("py", dpy, "#ff7f0e", "#8c564b", "p_y"),
        ("pz", dpz, "#2ca02c", "#d62728", "p_z"),
    ]:
        fig, ax = plt.subplots()
        plot_overlay_hist(
            ax,
            data_arr[matched & is_ypol],
            data_arr[matched & is_zpol],
            lbl, color_y, color_z,
        )
        fig.savefig(fig_dir / f"overlay_{tag}.pdf")
        fig.savefig(fig_dir / f"overlay_{tag}.png")
        plt.close(fig)

    # --- Summary ---
    print(f"\n{'='*50}")
    print(f"  NEBULA-Plus Joint Reconstruction Summary")
    print(f"{'='*50}")
    print(f"  Total events:      {n:,}")
    print(f"  Matched events:    {n_matched:,}")
    print(f"  Efficiency:        {eff:.1f}%")
    print(f"")
    print(f"  dpx: MAE={stats_px[0]:.3f}  RMSE={stats_px[1]:.3f}  Bias={stats_px[2]:.3f}  sigma={stats_px[3]:.3f}")
    print(f"  dpy: MAE={stats_py[0]:.3f}  RMSE={stats_py[1]:.3f}  Bias={stats_py[2]:.3f}  sigma={stats_py[3]:.3f}")
    print(f"  dpz: MAE={stats_pz[0]:.3f}  RMSE={stats_pz[1]:.3f}  Bias={stats_pz[2]:.3f}  sigma={stats_pz[3]:.3f}")
    print(f"  d|p|: MAE={stats_p[0]:.3f}  RMSE={stats_p[1]:.3f}  Bias={stats_p[2]:.3f}  sigma={stats_p[3]:.3f}")
    print(f"{'='*50}")

    for tag, label_short in [("ypol", "y-pol"), ("zpol", "z-pol")]:
        mask_pol = (matched & (is_ypol if tag == "ypol" else is_zpol))
        n_pol = mask_pol.sum()
        total_pol = (is_ypol if tag == "ypol" else is_zpol).sum()
        eff_pol = n_pol / total_pol * 100 if total_pol > 0 else 0
        print(f"\n  {label_short}: {n_pol:,}/{total_pol:,} matched ({eff_pol:.1f}%)")
        for arr, lbl in [(dpx, "px"), (dpy, "py"), (dpz, "pz")]:
            sub = arr[mask_pol]
            print(f"    d{lbl}: mean={np.nanmean(sub):.3f}  sigma={np.nanstd(sub):.3f}")

    print(f"\nQuality figures saved to {fig_dir}")


def parse_args(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--base-dir", default=RECO_BASE)
    ap.add_argument("--fig-dir", default=None)
    ap.add_argument("--out-dir", default=None)
    ap.add_argument("--pol", choices=["both", "ypol", "zpol"], default="both")
    ap.add_argument("--max-files", type=int, default=None)
    ap.add_argument("--bootstrap", type=int, default=500)
    ap.add_argument("--write-joined", action="store_true")
    ap.add_argument("--skip-quality", action="store_true")
    ap.add_argument("--skip-observables", action="store_true")
    return ap.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    base_dir = Path(args.base_dir)
    fig_dir = Path(args.fig_dir) if args.fig_dir else base_dir / "checks" / "figures"
    out_dir = Path(args.out_dir) if args.out_dir else base_dir / "checks" / "observables"
    pols = ["ypol", "zpol"] if args.pol == "both" else [args.pol]
    quality_pol = None if args.pol == "both" else args.pol

    if not args.skip_quality:
        run_quality_plots(base_dir, fig_dir, pol=quality_pol, max_files=args.max_files)
    if not args.skip_observables:
        run_observable_analysis(
            base_dir,
            fig_dir,
            out_dir,
            pols,
            max_files=args.max_files,
            n_boot=args.bootstrap,
            write_joined=args.write_joined,
        )


if __name__ == "__main__":
    main()
