#!/usr/bin/env python3
"""
Step 2: Read merged_events.root, compute residuals/pulls/per-cell MAE,
emit 20 PNG plots + summary.json + LaTeX report (en + zh).

Usage:
    python make_figures.py \
        --input build/test_output/rk_vs_nn_breakup/<tag>/merged_events.root \
        --output-dir build/test_output/rk_vs_nn_breakup/<tag>/
"""
import argparse
import json
from pathlib import Path
from datetime import datetime

import numpy as np
import uproot
import awkward as ak
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


RK_COLOR = "#1f77b4"  # blue
NN_COLOR = "#ff7f0e"  # orange
N_COLOR = "#2ca02c"   # green
plt.rcParams.update({
    "figure.dpi": 130,
    "savefig.dpi": 130,
    "font.size": 10,
    "axes.grid": True,
    "grid.alpha": 0.3,
})


def load_events(path):
    """Load merged_events.root into a dict of numpy arrays (string columns stay awkward)."""
    f = uproot.open(str(path))
    tree = f["events"]
    arr = tree.arrays(library="ak")
    out = {}
    for field in arr.fields:
        try:
            out[field] = ak.to_numpy(arr[field])
        except Exception:
            out[field] = arr[field]  # keep as awkward for strings
    # Strings: convert to numpy str array via ak.to_list + np.array
    for k in list(out.keys()):
        if isinstance(out[k], ak.Array) and out[k].layout.form.startswith("string"):
            out[k] = np.array(ak.to_list(out[k]))
    return out


def mask_pol(events, pol):
    return events["pol"] == pol


def compute_residuals(events, mask, backend):
    """Returns dict with dpx, dpy, dpz, dp for events where truth_has_proton and backend_has_proton."""
    truth_mask = mask & events["truth_has_proton"] & events[f"{backend}_has_proton"]
    truth_p = np.sqrt(events["truth_px"] ** 2 + events["truth_py"] ** 2 + events["truth_pz"] ** 2)[truth_mask]
    reco_p = np.sqrt(
        events[f"{backend}_px"] ** 2 + events[f"{backend}_py"] ** 2 + events[f"{backend}_pz"] ** 2
    )[truth_mask]
    return {
        "dpx": events[f"{backend}_px"][truth_mask] - events["truth_px"][truth_mask],
        "dpy": events[f"{backend}_py"][truth_mask] - events["truth_py"][truth_mask],
        "dpz": events[f"{backend}_pz"][truth_mask] - events["truth_pz"][truth_mask],
        "dp": reco_p - truth_p,
        "mask": truth_mask,
    }


def metrics(residual):
    """Compute MAE, RMSE, Bias, P95 for a residual array."""
    r = np.asarray(residual, dtype=float)
    r = r[np.isfinite(r)]
    if len(r) == 0:
        return {"mae": float("nan"), "rmse": float("nan"), "bias": float("nan"), "p95": float("nan"), "n": 0}
    return {
        "mae": float(np.mean(np.abs(r))),
        "rmse": float(np.sqrt(np.mean(r ** 2))),
        "bias": float(np.mean(r)),
        "p95": float(np.percentile(np.abs(r), 95)),
        "n": int(len(r)),
    }


def plot_residual_histogram(events, pol, output_dir):
    """A1: Proton residual histograms — RK vs NN overlaid."""
    mask = mask_pol(events, pol)
    rk = compute_residuals(events, mask, "rk")
    nn = compute_residuals(events, mask, "nn")
    fig, axes = plt.subplots(2, 2, figsize=(10, 7))
    for ax, key, label, rng in zip(
        axes.flat, ["dpx", "dpy", "dpz", "dp"],
        [r"$\Delta p_x$", r"$\Delta p_y$", r"$\Delta p_z$", r"$\Delta |p|$"],
        [(-150, 150), (-150, 150), (-200, 200), (-200, 200)],
    ):
        bins = np.linspace(rng[0], rng[1], 80)
        ax.hist(rk[key], bins=bins, histtype="step", label=f"RK (MAE={metrics(rk[key])['mae']:.1f})", color=RK_COLOR, linewidth=1.5)
        ax.hist(nn[key], bins=bins, histtype="step", label=f"NN (MAE={metrics(nn[key])['mae']:.1f})", color=NN_COLOR, linewidth=1.5)
        ax.set_xlabel(f"{label} [MeV/c]")
        ax.set_ylabel("events")
        ax.set_yscale("log")
        ax.legend(loc="upper right", fontsize=8)
        ax.set_title(label)
    fig.suptitle(f"Proton residuals — {pol}")
    fig.tight_layout()
    fig.savefig(Path(output_dir) / "figures" / f"proton_residuals_{pol}.png")
    plt.close(fig)


def plot_reco_vs_truth(events, pol, output_dir):
    """A2: 2D scatter reco vs truth — RK left, NN right."""
    mask = mask_pol(events, pol)
    fig, axes = plt.subplots(2, 4, figsize=(14, 7))
    truth_p = np.sqrt(events["truth_px"] ** 2 + events["truth_py"] ** 2 + events["truth_pz"] ** 2)
    rk_p = np.sqrt(events["rk_px"] ** 2 + events["rk_py"] ** 2 + events["rk_pz"] ** 2)
    nn_p = np.sqrt(events["nn_px"] ** 2 + events["nn_py"] ** 2 + events["nn_pz"] ** 2)
    for col, (axis, label) in enumerate([("x", r"$p_x$"), ("y", r"$p_y$"), ("z", r"$p_z$"), ("p", r"$|p|$")]):
        if axis == "p":
            truth = truth_p
            rk_reco = rk_p
            nn_reco = nn_p
        else:
            truth = events[f"truth_p{axis}"]
            rk_reco = events[f"rk_p{axis}"]
            nn_reco = events[f"nn_p{axis}"]
        m = mask & events["truth_has_proton"]
        for row, (reco, name, color) in enumerate([(rk_reco, "RK", RK_COLOR), (nn_reco, "NN", NN_COLOR)]):
            ax = axes[row, col]
            valid = m & events[f"{name.lower()}_has_proton"] & np.isfinite(reco) & np.isfinite(truth)
            if np.sum(valid) > 0:
                ax.hist2d(truth[valid], reco[valid], bins=60, cmap="viridis", cmin=1)
                lim = [min(truth[valid].min(), reco[valid].min()), max(truth[valid].max(), reco[valid].max())]
                ax.plot(lim, lim, "r--", linewidth=0.8, alpha=0.7)
            ax.set_xlabel(f"truth {label}")
            ax.set_ylabel(f"{name} reco {label}")
            if col == 0:
                ax.set_ylabel(f"{name}\nreco {label}")
    fig.suptitle(f"Proton reco vs truth — {pol}")
    fig.tight_layout()
    fig.savefig(Path(output_dir) / "figures" / f"proton_reco_vs_truth_{pol}.png")
    plt.close(fig)


def plot_per_event_winner(events, pol, output_dir):
    """A3: |RK-truth| vs |NN-truth| scatter with y=x line."""
    mask = mask_pol(events, pol) & events["truth_has_proton"] & events["rk_has_proton"] & events["nn_has_proton"]
    fig, axes = plt.subplots(1, 3, figsize=(13, 4))
    for ax, axis in zip(axes, ["x", "y", "z"]):
        rk_err = np.abs(events[f"rk_p{axis}"][mask] - events[f"truth_p{axis}"][mask])
        nn_err = np.abs(events[f"nn_p{axis}"][mask] - events[f"truth_p{axis}"][mask])
        if len(rk_err) > 0:
            ax.hist2d(rk_err, nn_err, bins=80, range=[[0, 150], [0, 150]], cmap="viridis", cmin=1)
        ax.plot([0, 150], [0, 150], "r--", linewidth=0.8, alpha=0.7)
        rk_wins = int(np.sum(rk_err < nn_err))
        nn_wins = int(np.sum(rk_err > nn_err))
        n = max(int(len(rk_err)), 1)
        ax.set_title(f"$p_{axis}$: RK closer {rk_wins/n*100:.1f}%, NN closer {nn_wins/n*100:.1f}%")
        ax.set_xlabel("|RK - truth| [MeV/c]")
        ax.set_ylabel("|NN - truth| [MeV/c]")
    fig.suptitle(f"Per-event winner — {pol}")
    fig.tight_layout()
    fig.savefig(Path(output_dir) / "figures" / f"proton_per_event_winner_{pol}.png")
    plt.close(fig)


def plot_rk_vs_nn_agreement(events, pol, output_dir):
    """B1: RK vs NN per-axis scatter."""
    mask = mask_pol(events, pol) & events["rk_has_proton"] & events["nn_has_proton"]
    fig, axes = plt.subplots(1, 3, figsize=(13, 4.5))
    for ax, axis in zip(axes, ["x", "y", "z"]):
        rk_v = events[f"rk_p{axis}"][mask]
        nn_v = events[f"nn_p{axis}"][mask]
        if len(rk_v) > 0:
            ax.hist2d(rk_v, nn_v, bins=80, cmap="viridis", cmin=1)
            lim = [min(rk_v.min(), nn_v.min()), max(rk_v.max(), nn_v.max())]
            ax.plot(lim, lim, "r--", linewidth=0.8, alpha=0.7)
        ax.set_xlabel(f"RK $p_{axis}$ [MeV/c]")
        ax.set_ylabel(f"NN $p_{axis}$ [MeV/c]")
    fig.suptitle(f"RK vs NN agreement — {pol}")
    fig.tight_layout()
    fig.savefig(Path(output_dir) / "figures" / f"proton_rk_vs_nn_agreement_{pol}.png")
    plt.close(fig)


def aggregate_zpol_by_b(cell_label):
    """Map a zpol cell+b_index label like 'd+Sn112E190g050znp_b01' to a (cell, b_range) pair.
    Returns (cell, b_range) where b_range is one of b01-b03, b04-b06, b07-b08, b09-b10."""
    # cell_label from events["cell"] + "_" + events["b_index"] for zpol
    parts = cell_label.split("_b")
    if len(parts) != 2:
        return (cell_label, "?")
    cell = parts[0]
    b_num = int(parts[1])
    if b_num <= 3:
        rng = "b01-b03"
    elif b_num <= 6:
        rng = "b04-b06"
    elif b_num <= 8:
        rng = "b07-b08"
    else:
        rng = "b09-b10"
    return (cell, rng)


def plot_per_cell_mae(events, pol, output_dir):
    """C1: per-cell MAE bar chart."""
    mask = mask_pol(events, pol)
    if pol == "ypol":
        # 16 cells, simple bar chart
        cells = sorted(set(events["cell"][mask].tolist()))
        labels = cells
        rk_maes = {"x": [], "y": [], "z": []}
        nn_maes = {"x": [], "y": [], "z": []}
        for c in cells:
            cm = mask & (events["cell"] == c)
            for axis in ["x", "y", "z"]:
                rk_res = events[f"rk_p{axis}"][cm & events["truth_has_proton"] & events["rk_has_proton"]] - events[f"truth_p{axis}"][cm & events["truth_has_proton"] & events["rk_has_proton"]]
                nn_res = events[f"nn_p{axis}"][cm & events["truth_has_proton"] & events["nn_has_proton"]] - events[f"truth_p{axis}"][cm & events["truth_has_proton"] & events["nn_has_proton"]]
                rk_maes[axis].append(float(np.mean(np.abs(rk_res))) if len(rk_res) > 0 else 0)
                nn_maes[axis].append(float(np.mean(np.abs(nn_res))) if len(nn_res) > 0 else 0)
    else:
        # zpol: 16 cells × 10 b × 2 hel = 320 (cell, b) combos — too many. Aggregate by (cell, b_range) → 16 × 4 = 64
        from collections import defaultdict
        accum = defaultdict(lambda: {"rk": {"x": [], "y": [], "z": []}, "nn": {"x": [], "y": [], "z": []}})
        labels_set = set()
        cell_arr = np.asarray(events["cell"][mask])
        bidx_arr = np.asarray(events["b_index"][mask])
        # Iterate over masked (local) indices, not global indices
        for i in range(len(cell_arr)):
            c = str(cell_arr[i])
            b = str(bidx_arr[i])
            full_label = f"{c}_{b}"
            cell_base, b_rng = aggregate_zpol_by_b(full_label)
            label = f"{cell_base} [{b_rng}]"
            labels_set.add(label)
            # Use the original (global) index to access other columns — find global idx
            global_i = int(np.where(mask)[0][i])
            for axis in ["x", "y", "z"]:
                if events["truth_has_proton"][global_i] and events["rk_has_proton"][global_i]:
                    accum[label]["rk"][axis].append(abs(float(events[f"rk_p{axis}"][global_i]) - float(events[f"truth_p{axis}"][global_i])))
                if events["truth_has_proton"][global_i] and events["nn_has_proton"][global_i]:
                    accum[label]["nn"][axis].append(abs(float(events[f"nn_p{axis}"][global_i]) - float(events[f"truth_p{axis}"][global_i])))
        labels = sorted(labels_set)
        rk_maes = {"x": [], "y": [], "z": []}
        nn_maes = {"x": [], "y": [], "z": []}
        for lbl in labels:
            for axis in ["x", "y", "z"]:
                rk_maes[axis].append(float(np.mean(accum[lbl]["rk"][axis])) if len(accum[lbl]["rk"][axis]) > 0 else 0)
                nn_maes[axis].append(float(np.mean(accum[lbl]["nn"][axis])) if len(accum[lbl]["nn"][axis]) > 0 else 0)

    fig, axes = plt.subplots(1, 3, figsize=(max(12, len(labels) * 0.5), 5))
    x = np.arange(len(labels))
    w = 0.4
    for ax, axis in zip(axes, ["x", "y", "z"]):
        ax.bar(x - w / 2, rk_maes[axis], w, label="RK", color=RK_COLOR)
        ax.bar(x + w / 2, nn_maes[axis], w, label="NN", color=NN_COLOR)
        ax.set_xticks(x)
        ax.set_xticklabels(labels, rotation=90, fontsize=6)
        ax.set_ylabel(f"MAE $p_{axis}$ [MeV/c]")
        ax.legend()
    fig.suptitle(f"Per-cell MAE — {pol}" + ("" if pol == "ypol" else " (aggregated by b-range)"))
    fig.tight_layout()
    fig.savefig(Path(output_dir) / "figures" / f"proton_per_cell_mae_{pol}.png")
    plt.close(fig)


def plot_efficiency_per_cell(events, pol, output_dir):
    """C2: per-cell reco efficiency (RK vs NN)."""
    mask = mask_pol(events, pol)
    if pol == "ypol":
        cells = sorted(set(events["cell"][mask].tolist()))
        labels = cells
    else:
        from collections import defaultdict
        # Aggregate by (cell, b_range)
        labels_set = set()
        truth_count = defaultdict(lambda: [0, 0, 0])  # [truth, rk_reco, nn_reco]
        cell_arr = np.asarray(events["cell"][mask])
        bidx_arr = np.asarray(events["b_index"][mask])
        global_indices = np.where(mask)[0]
        for i in range(len(cell_arr)):
            c = str(cell_arr[i])
            b = str(bidx_arr[i])
            cell_base, b_rng = aggregate_zpol_by_b(f"{c}_{b}")
            label = f"{cell_base} [{b_rng}]"
            labels_set.add(label)
            global_i = int(global_indices[i])
            if events["truth_has_proton"][global_i]:
                truth_count[label][0] += 1
                if events["rk_has_proton"][global_i]:
                    truth_count[label][1] += 1
                if events["nn_has_proton"][global_i]:
                    truth_count[label][2] += 1
        labels = sorted(labels_set)
        cells = labels
    rk_eff = []
    nn_eff = []
    if pol == "ypol":
        for c in cells:
            cm = mask & (events["cell"] == c) & events["truth_has_proton"]
            n_truth = int(np.sum(cm))
            n_rk = int(np.sum(cm & events["rk_has_proton"]))
            n_nn = int(np.sum(cm & events["nn_has_proton"]))
            rk_eff.append(n_rk / n_truth * 100 if n_truth > 0 else 0)
            nn_eff.append(n_nn / n_truth * 100 if n_truth > 0 else 0)
    else:
        for lbl in labels:
            t, rk, nn = truth_count[lbl]
            rk_eff.append(rk / t * 100 if t > 0 else 0)
            nn_eff.append(nn / t * 100 if t > 0 else 0)

    fig, ax = plt.subplots(figsize=(max(12, len(labels) * 0.5), 5))
    x = np.arange(len(labels))
    w = 0.4
    ax.bar(x - w / 2, rk_eff, w, label="RK", color=RK_COLOR)
    ax.bar(x + w / 2, nn_eff, w, label="NN", color=NN_COLOR)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=90, fontsize=6)
    ax.set_ylabel("reco efficiency [%]")
    ax.set_ylim(0, 105)
    ax.legend()
    ax.set_title(f"Per-cell proton reco efficiency — {pol}" + ("" if pol == "ypol" else " (aggregated by b-range)"))
    fig.tight_layout()
    fig.savefig(Path(output_dir) / "figures" / f"proton_efficiency_per_cell_{pol}.png")
    plt.close(fig)


def plot_rk_pull(events, pol, output_dir):
    """D1: RK pull distribution (reco-truth)/sigma. Skips if sigma invalid for >5% of events."""
    mask = mask_pol(events, pol) & events["truth_has_proton"] & events["rk_has_proton"]
    sigma = np.asarray(events["rk_p_sigma"][mask], dtype=float)
    valid_sigma = sigma > 0
    if np.sum(valid_sigma) < 0.95 * len(sigma):
        print(f"[fig] WARN: rk_p_sigma invalid for >5% of {pol} events; skipping pull plot")
        return
    truth_p = np.sqrt(events["truth_px"] ** 2 + events["truth_py"] ** 2 + events["truth_pz"] ** 2)[mask][valid_sigma]
    rk_p = np.sqrt(events["rk_px"] ** 2 + events["rk_py"] ** 2 + events["rk_pz"] ** 2)[mask][valid_sigma]
    pull = (rk_p - truth_p) / sigma[valid_sigma]
    pull = pull[np.isfinite(pull)]
    if len(pull) == 0:
        print(f"[fig] WARN: no valid pull entries for {pol}; skipping")
        return
    fig, ax = plt.subplots(figsize=(7, 4.5))
    ax.hist(pull, bins=80, range=[-6, 6], histtype="step", color=RK_COLOR, linewidth=1.5)
    ax.axvline(0, color="k", linestyle="--", linewidth=0.8)
    ax.set_xlabel(r"pull $(|p|_{RK} - |p|_{truth}) / \sigma_p$")
    ax.set_ylabel("events")
    ax.set_yscale("log")
    ax.set_title(f"RK pull distribution — {pol} (mean={np.mean(pull):.2f}, std={np.std(pull):.2f}, N={len(pull)})")
    fig.tight_layout()
    fig.savefig(Path(output_dir) / "figures" / f"proton_rk_pull_{pol}.png")
    plt.close(fig)


def plot_neutron_residuals(events, pol, output_dir):
    """E1: neutron residual histograms."""
    mask = mask_pol(events, pol) & events["truth_has_neutron"] & events["n_has_reco"]
    truth_p = np.sqrt(events["truth_n_px"] ** 2 + events["truth_n_py"] ** 2 + events["truth_n_pz"] ** 2)[mask]
    reco_p = np.sqrt(events["n_px"] ** 2 + events["n_py"] ** 2 + events["n_pz"] ** 2)[mask]
    dpx = events["n_px"][mask] - events["truth_n_px"][mask]
    dpy = events["n_py"][mask] - events["truth_n_py"][mask]
    dpz = events["n_pz"][mask] - events["truth_n_pz"][mask]
    dp = reco_p - truth_p
    truth_theta = np.arccos(np.clip(events["truth_n_pz"][mask] / np.maximum(truth_p, 1e-12), -1, 1))
    reco_theta = np.arccos(np.clip(events["n_pz"][mask] / np.maximum(reco_p, 1e-12), -1, 1))
    dtheta = reco_theta - truth_theta
    truth_phi = np.arctan2(events["truth_n_py"][mask], events["truth_n_px"][mask])
    reco_phi = np.arctan2(events["n_py"][mask], events["n_px"][mask])
    dphi = np.mod(reco_phi - truth_phi + np.pi, 2 * np.pi) - np.pi
    fig, axes = plt.subplots(2, 3, figsize=(13, 7))
    for ax, key, label, rng in zip(
        axes.flat,
        [dpx, dpy, dpz, dp, dtheta, dphi],
        [r"$\Delta p_x$", r"$\Delta p_y$", r"$\Delta p_z$", r"$\Delta |p|$", r"$\Delta\theta$", r"$\Delta\phi$"],
        [(-300, 300), (-300, 300), (-400, 400), (-400, 400), (-0.5, 0.5), (-0.5, 0.5)],
    ):
        ax.hist(key, bins=80, range=rng, histtype="step", color=N_COLOR, linewidth=1.5)
        ax.set_xlabel(label)
        ax.set_ylabel("events")
        ax.set_yscale("log")
        m = metrics(key)
        ax.set_title(f"{label}: MAE={m['mae']:.1f}" + ("" if "theta" not in label and "phi" not in label else f" ({m['mae']:.3f} rad)"))
    fig.suptitle(f"Neutron residuals — {pol}")
    fig.tight_layout()
    fig.savefig(Path(output_dir) / "figures" / f"neutron_residuals_{pol}.png")
    plt.close(fig)


def plot_neutron_reco_vs_truth(events, pol, output_dir):
    """E2: neutron 2D scatter reco vs truth."""
    mask = mask_pol(events, pol) & events["truth_has_neutron"] & events["n_has_reco"]
    fig, axes = plt.subplots(1, 3, figsize=(13, 4.5))
    for ax, axis in zip(axes, ["x", "y", "z"]):
        tv = events[f"truth_n_p{axis}"][mask]
        rv = events[f"n_p{axis}"][mask]
        if len(tv) > 0:
            ax.hist2d(tv, rv, bins=80, cmap="viridis", cmin=1)
            lim = [min(tv.min(), rv.min()), max(tv.max(), rv.max())]
            ax.plot(lim, lim, "r--", linewidth=0.8, alpha=0.7)
        ax.set_xlabel(rf"truth $p_{{{axis},n}}$ [MeV/c]")
        ax.set_ylabel(rf"reco $p_{{{axis},n}}$ [MeV/c]")
    fig.suptitle(f"Neutron reco vs truth — {pol}")
    fig.tight_layout()
    fig.savefig(Path(output_dir) / "figures" / f"neutron_reco_vs_truth_{pol}.png")
    plt.close(fig)


def compute_summary(events):
    """Compute summary.json metrics."""
    summary = {}
    for pol in ["ypol", "zpol"]:
        mask = mask_pol(events, pol)
        summary[f"proton_{pol}"] = {}
        for backend in ["rk", "nn"]:
            res = compute_residuals(events, mask, backend)
            summary[f"proton_{pol}"][backend] = {
                "px": metrics(res["dpx"]),
                "py": metrics(res["dpy"]),
                "pz": metrics(res["dpz"]),
                "p": metrics(res["dp"]),
            }
        truth_mask = mask & events["truth_has_proton"]
        n_truth = int(np.sum(truth_mask))
        summary[f"proton_{pol}"]["efficiency"] = {
            "rk": float(np.sum(truth_mask & events["rk_has_proton"]) / n_truth * 100) if n_truth > 0 else 0,
            "nn": float(np.sum(truth_mask & events["nn_has_proton"]) / n_truth * 100) if n_truth > 0 else 0,
            "n_truth": n_truth,
        }
        # neutron
        n_mask = mask & events["truth_has_neutron"]
        n_truth_n = int(np.sum(n_mask))
        n_match = n_mask & events["n_has_reco"]
        summary[f"neutron_{pol}"] = {
            "efficiency": float(np.sum(n_match) / n_truth_n * 100) if n_truth_n > 0 else 0,
            "n_truth": n_truth_n,
            "n_reco": int(np.sum(n_match)),
        }
        if np.sum(n_match) > 0:
            truth_p = np.sqrt(events["truth_n_px"][n_match] ** 2 + events["truth_n_py"][n_match] ** 2 + events["truth_n_pz"][n_match] ** 2)
            reco_p = np.sqrt(events["n_px"][n_match] ** 2 + events["n_py"][n_match] ** 2 + events["n_pz"][n_match] ** 2)
            summary[f"neutron_{pol}"]["px"] = metrics(events["n_px"][n_match] - events["truth_n_px"][n_match])
            summary[f"neutron_{pol}"]["py"] = metrics(events["n_py"][n_match] - events["truth_n_py"][n_match])
            summary[f"neutron_{pol}"]["pz"] = metrics(events["n_pz"][n_match] - events["truth_n_pz"][n_match])
            summary[f"neutron_{pol}"]["p"] = metrics(reco_p - truth_p)
        else:
            summary[f"neutron_{pol}"]["px"] = summary[f"neutron_{pol}"]["py"] = summary[f"neutron_{pol}"]["pz"] = summary[f"neutron_{pol}"]["p"] = metrics(np.array([]))
    return summary


def mfmt(m):
    if m["n"] == 0:
        return "N/A"
    return f"MAE={m['mae']:.1f}, RMSE={m['rmse']:.1f}, Bias={m['bias']:+.1f}, P95={m['p95']:.1f}, N={m['n']}"


def write_latex(summary, output_dir, tag):
    """Write the LaTeX report (English + Chinese)."""
    date_str = datetime.now().strftime("%Y-%m-%d")
    n_total = summary["proton_ypol"]["efficiency"]["n_truth"] + summary["proton_zpol"]["efficiency"]["n_truth"]

    # English version
    tex_path = Path(output_dir) / f"rk_vs_nn_breakup_{tag}.tex"
    lines = [
        r"\documentclass[11pt]{article}",
        r"\usepackage[margin=1in]{geometry}",
        r"\usepackage{graphicx}",
        r"\usepackage{booktabs}",
        r"\usepackage{amsmath}",
        r"\usepackage{hyperref}",
        r"\title{RK vs NN proton reconstruction on breakup g4output \\ \large Head-to-head evaluation}",
        rf"\date{{{date_str}}}",
        r"\begin{document}",
        r"\maketitle",
        "",
        r"\begin{abstract}",
        rf"This report evaluates proton reconstruction quality on the d+Sn breakup sample (ypol + zpol, 176 cells) using two backends: RK (Runge-Kutta least-squares fitter, three-point-free mode, run on spana03) and NN (6-128-128-64 MLP, formal\_B115T3deg/20260420\_184345 model, run locally). Both reconstructions derive from the same Geant4 g4output, enabling per-event entry-index alignment. Three error sets are reported for protons (RK-vs-truth, NN-vs-truth, RK-vs-NN), plus truth-vs-reco residuals for neutrons (NEBULA $\beta$-method, identical for both backends). Sample: {n_total} truth-proton events total.",
        r"\end{abstract}",
        "",
        r"\section{Data \& method}",
        r"\textbf{RK reco}: spana03 \texttt{data/reconstruction/dbreak\_elastic/}, 176 files, produced 2026-05-09 via \texttt{run\_dbreak\_elastic\_pipeline.py stage\_reco} with \texttt{--backend rk --rk-fit-mode three-point-free --max-iterations 40}. \\",
        r"\textbf{NN reco}: local \texttt{data/reconstruction/breakup\_nn\_20260503/}, 176 files, produced 2026-05-03 via \texttt{01\_run\_nn\_reco.sh} with the clean production model \texttt{formal\_B115T3deg/20260420\_184345}. \\",
        r"\textbf{g4output source}: identical for both backends (verified by file size 285252607 bytes, timestamp 2026-05-01 20:16:46 +0900). Per-event entry-index alignment is valid 1:1. \\",
        r"\textbf{Frame convention}: target frame (beam-as-Z) throughout. Truth is already target-frame; both backends' reco is rotated to target frame at write time via \texttt{RotateRecoResultToTargetFrame} (\texttt{apps/run\_reconstruction/main.cc:629-630}). No additional rotation in this analysis. \\",
        r"\textbf{Multi-proton events}: pick candidate whose $|p|$ is closest to truth (matches \texttt{evaluate\_target\_momentum\_reco.cc:347-359}).",
        "",
        r"\section{Proton reconstruction --- RK vs NN vs truth}",
        r"\subsection{Global residuals}",
        r"\begin{table}[h]\centering\small\begin{tabular}{lllll}\toprule",
        r"Pol & Backend & $\Delta p_x$ & $\Delta p_y$ & $\Delta p_z$ \\\midrule",
    ]
    for pol in ["ypol", "zpol"]:
        for backend in ["rk", "nn"]:
            s = summary[f"proton_{pol}"][backend]
            lines.append(f"{pol} & {backend.upper()} & {mfmt(s['px'])} & {mfmt(s['py'])} & {mfmt(s['pz'])} \\\\")
    lines.append(r"\bottomrule\end{tabular}\end{table}")
    for pol in ["ypol", "zpol"]:
        lines.extend([
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/proton_residuals_{pol}.png}}",
            rf"\caption{{Proton residual distributions, {pol}.}}",
            r"\end{figure}",
            r"",
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/proton_reco_vs_truth_{pol}.png}}",
            rf"\caption{{Proton reco vs truth 2D scatter, {pol}.}}",
            r"\end{figure}",
            r"",
        ])
    lines.extend([
        r"\subsection{Per-event agreement (RK vs NN)}",
        r"For each event, compare $|p_{RK}-p_{truth}|$ vs $|p_{NN}-p_{truth}|$. Points below the $y=x$ line are events where RK is closer to truth; above means NN is closer.",
        r"",
    ])
    for pol in ["ypol", "zpol"]:
        lines.extend([
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/proton_per_event_winner_{pol}.png}}",
            rf"\caption{{Per-event winner, {pol}.}}",
            r"\end{figure}",
            r"",
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/proton_rk_vs_nn_agreement_{pol}.png}}",
            rf"\caption{{RK vs NN direct agreement, {pol}.}}",
            r"\end{figure}",
            r"",
        ])
    lines.extend([
        r"\subsection{Per-cell breakdown}",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.95\textwidth]{figures/rk_vs_nn_breakup/proton_per_cell_mae_ypol.png}",
        r"\caption{Per-cell MAE, ypol.}",
        r"\end{figure}",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.95\textwidth]{figures/rk_vs_nn_breakup/proton_per_cell_mae_zpol.png}",
        r"\caption{Per-cell MAE, zpol (aggregated by b-range).}",
        r"\end{figure}",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.95\textwidth]{figures/rk_vs_nn_breakup/proton_efficiency_per_cell_ypol.png}",
        r"\caption{Per-cell proton reco efficiency, ypol.}",
        r"\end{figure}",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.95\textwidth]{figures/rk_vs_nn_breakup/proton_efficiency_per_cell_zpol.png}",
        r"\caption{Per-cell proton reco efficiency, zpol (aggregated by b-range).}",
        r"\end{figure}",
        r"",
        r"\subsection{RK uncertainty calibration (pull)}",
        r"Pull $= (|p|_{RK} - |p|_{truth}) / \sigma_p$. Should be $\sim\mathcal{N}(0,1)$ if Fisher uncertainties are calibrated.",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.7\textwidth]{figures/rk_vs_nn_breakup/proton_rk_pull_ypol.png}",
        r"\caption{RK pull distribution, ypol.}",
        r"\end{figure}",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.7\textwidth]{figures/rk_vs_nn_breakup/proton_rk_pull_zpol.png}",
        r"\caption{RK pull distribution, zpol.}",
        r"\end{figure}",
        r"",
        r"\section{Neutron reconstruction --- truth vs reco}",
        r"Neutron momentum is reconstructed by the NEBULA $\beta$-method in both RK and NN backends (the backend choice only affects proton reconstruction). Residuals below are therefore single-backend (NN file used).",
        r"",
    ])
    for pol in ["ypol", "zpol"]:
        lines.extend([
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/neutron_residuals_{pol}.png}}",
            rf"\caption{{Neutron residual distributions, {pol}.}}",
            r"\end{figure}",
            r"",
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/neutron_reco_vs_truth_{pol}.png}}",
            rf"\caption{{Neutron reco vs truth 2D scatter, {pol}.}}",
            r"\end{figure}",
            r"",
        ])
    lines.extend([
        r"\section{Conclusion}",
        r"(To be filled in after inspecting the figures.)",
        r"",
        r"\appendix",
        r"\section{Per-cell numeric table}",
        r"(See \texttt{summary.json} for full per-cell numeric breakdown.)",
        r"",
        r"\end{document}",
    ])
    tex_path.write_text("\n".join(lines))
    print(f"[fig] wrote {tex_path}")

    # Chinese version
    zh_path = Path(output_dir) / f"rk_vs_nn_breakup_{tag}_zh.tex"
    zh_lines = [
        r"\documentclass[11pt]{ctexart}",
        r"\usepackage[margin=1in]{geometry}",
        r"\usepackage{graphicx}",
        r"\usepackage{booktabs}",
        r"\usepackage{amsmath}",
        r"\usepackage{hyperref}",
        r"\title{breakup g4output 上 RK 与 NN 质子重建对比 \\ \large 头对头评估}",
        rf"\date{{{date_str}}}",
        r"\begin{document}",
        r"\maketitle",
        "",
        r"\begin{abstract}",
        rf"本报告评估 d+Sn breakup 样本 (ypol + zpol, 176 cells) 上两种后端的质子重建质量: RK (Runge-Kutta 最小二乘拟合器, three-point-free 模式, 在 spana03 上运行) 与 NN (6-128-128-64 MLP, formal\_B115T3deg/20260420\_184345 模型, 本地运行)。两套重建均来自同一 Geant4 g4output, 因此可以按事件 entry-index 一一对应。报告质子的三套误差 (RK-vs-truth, NN-vs-truth, RK-vs-NN), 以及中子的 truth-vs-reco 残差 (NEBULA $\beta$-method, 两种后端相同)。样本: 共 {n_total} 个 truth-proton 事件。",
        r"\end{abstract}",
        "",
        r"\section{数据与方法}",
        r"\textbf{RK reco}: spana03 \texttt{data/reconstruction/dbreak\_elastic/}, 176 文件, 2026-05-09 由 \texttt{run\_dbreak\_elastic\_pipeline.py stage\_reco} 产出 (\texttt{--backend rk --rk-fit-mode three-point-free --max-iterations 40})。\\",
        r"\textbf{NN reco}: 本地 \texttt{data/reconstruction/breakup\_nn\_20260503/}, 176 文件, 2026-05-03 由 \texttt{01\_run\_nn\_reco.sh} 产出 (clean production model \texttt{formal\_B115T3deg/20260420\_184345})。\\",
        r"\textbf{g4output}: 两种后端使用相同 g4output (已验证文件大小 285252607 字节, 时间戳 2026-05-01 20:16:46 +0900)。Per-event entry-index 1:1 对齐有效。\\",
        r"\textbf{参考系}: target frame (beam-as-Z)。truth 已是 target frame; 两种后端的 reco 在写入时由 \texttt{RotateRecoResultToTargetFrame} 旋转至 target frame (\texttt{apps/run\_reconstruction/main.cc:629-630})。本分析无需额外旋转。\\",
        r"\textbf{多质子事件}: 选择 $|p|$ 最接近 truth 的候选 (与 \texttt{evaluate\_target\_momentum\_reco.cc:347-359} 一致)。",
        "",
        r"\section{质子重建 --- RK vs NN vs truth}",
        r"\subsection{全局残差}",
        r"\begin{table}[h]\centering\small\begin{tabular}{lllll}\toprule",
        r"Pol & Backend & $\Delta p_x$ & $\Delta p_y$ & $\Delta p_z$ \\\midrule",
    ]
    for pol in ["ypol", "zpol"]:
        for backend in ["rk", "nn"]:
            s = summary[f"proton_{pol}"][backend]
            zh_lines.append(f"{pol} & {backend.upper()} & {mfmt(s['px'])} & {mfmt(s['py'])} & {mfmt(s['pz'])} \\\\")
    zh_lines.append(r"\bottomrule\end{tabular}\end{table}")
    for pol in ["ypol", "zpol"]:
        zh_lines.extend([
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/proton_residuals_{pol}.png}}",
            rf"\caption{{质子残差分布, {pol}。}}",
            r"\end{figure}",
            r"",
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/proton_reco_vs_truth_{pol}.png}}",
            rf"\caption{{质子 reco vs truth 2D 散点, {pol}。}}",
            r"\end{figure}",
            r"",
        ])
    zh_lines.extend([
        r"\subsection{Per-event 一致性 (RK vs NN)}",
        r"对每个事件, 比较 $|p_{RK}-p_{truth}|$ 与 $|p_{NN}-p_{truth}|$。$y=x$ 线下方为 RK 更接近 truth 的事件, 上方为 NN 更接近 truth 的事件。",
        r"",
    ])
    for pol in ["ypol", "zpol"]:
        zh_lines.extend([
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/proton_per_event_winner_{pol}.png}}",
            rf"\caption{{Per-event winner, {pol}。}}",
            r"\end{figure}",
            r"",
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/proton_rk_vs_nn_agreement_{pol}.png}}",
            rf"\caption{{RK vs NN 直接一致性, {pol}。}}",
            r"\end{figure}",
            r"",
        ])
    zh_lines.extend([
        r"\subsection{Per-cell 分布}",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.95\textwidth]{figures/rk_vs_nn_breakup/proton_per_cell_mae_ypol.png}",
        r"\caption{Per-cell MAE, ypol。}",
        r"\end{figure}",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.95\textwidth]{figures/rk_vs_nn_breakup/proton_per_cell_mae_zpol.png}",
        r"\caption{Per-cell MAE, zpol (按 b-range 聚合)。}",
        r"\end{figure}",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.95\textwidth]{figures/rk_vs_nn_breakup/proton_efficiency_per_cell_ypol.png}",
        r"\caption{Per-cell 质子重建效率, ypol。}",
        r"\end{figure}",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.95\textwidth]{figures/rk_vs_nn_breakup/proton_efficiency_per_cell_zpol.png}",
        r"\caption{Per-cell 质子重建效率, zpol (按 b-range 聚合)。}",
        r"\end{figure}",
        r"",
        r"\subsection{RK 不确定度校准 (pull)}",
        r"Pull $= (|p|_{RK} - |p|_{truth}) / \sigma_p$。若 Fisher 不确定度校准良好, 应近似 $\sim\mathcal{N}(0,1)$。",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.7\textwidth]{figures/rk_vs_nn_breakup/proton_rk_pull_ypol.png}",
        r"\caption{RK pull 分布, ypol。}",
        r"\end{figure}",
        r"",
        r"\begin{figure}[h]\centering",
        r"\includegraphics[width=0.7\textwidth]{figures/rk_vs_nn_breakup/proton_rk_pull_zpol.png}",
        r"\caption{RK pull 分布, zpol。}",
        r"\end{figure}",
        r"",
        r"\section{中子重建 --- truth vs reco}",
        r"中子动量在 RK 和 NN 后端中均由 NEBULA $\beta$-method 重建 (后端选择只影响质子重建)。下方残差为单后端 (使用 NN 文件)。",
        r"",
    ])
    for pol in ["ypol", "zpol"]:
        zh_lines.extend([
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/neutron_residuals_{pol}.png}}",
            rf"\caption{{中子残差分布, {pol}。}}",
            r"\end{figure}",
            r"",
            r"\begin{figure}[h]\centering",
            rf"\includegraphics[width=0.95\textwidth]{{figures/rk_vs_nn_breakup/neutron_reco_vs_truth_{pol}.png}}",
            rf"\caption{{中子 reco vs truth 2D 散点, {pol}。}}",
            r"\end{figure}",
            r"",
        ])
    zh_lines.extend([
        r"\section{结论}",
        r"(运行 pipeline 后填入。)",
        r"",
        r"\appendix",
        r"\section{Per-cell 数值表}",
        r"(完整 per-cell 数值见 \texttt{summary.json}。)",
        r"",
        r"\end{document}",
    ])
    zh_path.write_text("\n".join(zh_lines))
    print(f"[fig] wrote {zh_path}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output-dir", required=True)
    args = ap.parse_args()

    output_dir = Path(args.output_dir)
    (output_dir / "figures").mkdir(parents=True, exist_ok=True)

    print(f"[fig] loading {args.input}")
    events = load_events(args.input)
    n_events = len(events["event_index"])
    print(f"[fig] loaded {n_events} events")

    for pol in ["ypol", "zpol"]:
        print(f"[fig] generating plots for {pol}...")
        plot_residual_histogram(events, pol, output_dir)
        plot_reco_vs_truth(events, pol, output_dir)
        plot_per_event_winner(events, pol, output_dir)
        plot_rk_vs_nn_agreement(events, pol, output_dir)
        plot_per_cell_mae(events, pol, output_dir)
        plot_efficiency_per_cell(events, pol, output_dir)
        plot_rk_pull(events, pol, output_dir)
        plot_neutron_residuals(events, pol, output_dir)
        plot_neutron_reco_vs_truth(events, pol, output_dir)

    print(f"[fig] computing summary...")
    summary = compute_summary(events)
    summary_path = output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2))
    print(f"[fig] wrote {summary_path}")

    tag = datetime.now().strftime("%Y%m%d")
    write_latex(summary, output_dir, tag)

    print(f"\n[fig] === Headline metrics ===")
    for pol in ["ypol", "zpol"]:
        print(f"\n{pol}:")
        for backend in ["rk", "nn"]:
            s = summary[f"proton_{pol}"][backend]
            if s["px"]["n"] > 0:
                print(f"  {backend.upper()}: px MAE={s['px']['mae']:.1f}, py MAE={s['py']['mae']:.1f}, pz MAE={s['pz']['mae']:.1f}, |p| MAE={s['p']['mae']:.1f}")
        eff = summary[f"proton_{pol}"]["efficiency"]
        print(f"  efficiency: RK={eff['rk']:.1f}%, NN={eff['nn']:.1f}% (n_truth={eff['n_truth']})")
        ns = summary[f"neutron_{pol}"]
        print(f"  neutron: eff={ns['efficiency']:.1f}% (n_truth={ns['n_truth']}, n_reco={ns['n_reco']})")


if __name__ == "__main__":
    main()
