#!/usr/bin/env python3
"""NN vs RK comparison plots and JSON metrics for the target-frame retrain.

Reads per-event CSVs emitted by ``evaluate_target_momentum_reco`` (one for NN,
one for RK) over the SAME test sim, computes per-coord residual statistics on
the matched-by-event-index intersection, and writes figures + a metrics JSON.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

COORDS = ("px", "py", "pz")


def half_width(x: np.ndarray) -> float:
    if x.size < 5:
        return float("nan")
    p84, p16 = np.percentile(x, [84.135, 15.865])
    return 0.5 * (p84 - p16)


def stats(arr: np.ndarray) -> dict:
    arr = arr[np.isfinite(arr)]
    if arr.size == 0:
        return {"n": 0}
    return {
        "n": int(arr.size),
        "mean": float(np.mean(arr)),
        "median": float(np.median(arr)),
        "rmse": float(np.sqrt(np.mean(arr ** 2))),
        "half_width_68": float(half_width(arr)),
        "p95_abs": float(np.percentile(np.abs(arr), 95)),
    }


def load(path: Path, tag: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    df = df[df["matched"] == 1].copy()
    df.rename(columns={c: f"{tag}_{c}" for c in ["reco_px", "reco_py", "reco_pz", "dpx", "dpy", "dpz"]}, inplace=True)
    return df.set_index("event_index")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--nn-events", type=Path, required=True)
    ap.add_argument("--rk-events", type=Path, required=True)
    ap.add_argument("--out-dir", type=Path, required=True)
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)

    nn = load(args.nn_events, "nn")
    rk = load(args.rk_events, "rk")

    common = nn.index.intersection(rk.index)
    nn_only = len(nn.index.difference(rk.index))
    rk_only = len(rk.index.difference(nn.index))

    df = nn.loc[common].join(rk[["rk_reco_px", "rk_reco_py", "rk_reco_pz", "rk_dpx", "rk_dpy", "rk_dpz"]].loc[common])
    print(f"[compare] nn rows={len(nn)}  rk rows={len(rk)}  matched={len(df)}  nn-only={nn_only}  rk-only={rk_only}")

    metrics: dict = {
        "counts": {
            "nn_matched": int(len(nn)),
            "rk_matched": int(len(rk)),
            "common": int(len(df)),
            "nn_only": int(nn_only),
            "rk_only": int(rk_only),
        },
        "residuals": {"nn": {}, "rk": {}},
    }
    d_cols = {"px": "dpx", "py": "dpy", "pz": "dpz"}
    for coord in COORDS:
        metrics["residuals"]["nn"][coord] = stats(df[f"nn_{d_cols[coord]}"].to_numpy())
        metrics["residuals"]["rk"][coord] = stats(df[f"rk_{d_cols[coord]}"].to_numpy())

    # |p| residual
    truth_p = np.linalg.norm(df[["truth_px", "truth_py", "truth_pz"]].to_numpy(), axis=1)
    nn_p = np.linalg.norm(df[["nn_reco_px", "nn_reco_py", "nn_reco_pz"]].to_numpy(), axis=1)
    rk_p = np.linalg.norm(df[["rk_reco_px", "rk_reco_py", "rk_reco_pz"]].to_numpy(), axis=1)
    metrics["residuals"]["nn"]["pmag"] = stats(nn_p - truth_p)
    metrics["residuals"]["rk"]["pmag"] = stats(rk_p - truth_p)

    (args.out_dir / "metrics.json").write_text(json.dumps(metrics, indent=2))
    print(f"[compare] metrics -> {args.out_dir / 'metrics.json'}")

    # ---- figure 1: truth vs reco 2D, 3 coords x 2 backends ----
    fig, axes = plt.subplots(2, 3, figsize=(13, 8), sharex="col", sharey="col")
    truth_ranges = {
        "px": (-200, 200),
        "py": (-200, 200),
        "pz": (450, 750),
    }
    for j, coord in enumerate(COORDS):
        tr = truth_ranges[coord]
        for i, tag in enumerate(("nn", "rk")):
            ax = axes[i, j]
            t = df[f"truth_{coord}"].to_numpy()
            r = df[f"{tag}_reco_{coord}"].to_numpy()
            ax.hist2d(t, r, bins=100, range=[tr, tr], cmap="viridis")
            lo, hi = tr
            ax.plot([lo, hi], [lo, hi], "r-", lw=0.8, alpha=0.6)
            ax.set_xlim(tr); ax.set_ylim(tr)
            ax.set_title(f"{tag.upper()}  {coord}")
            if i == 1:
                ax.set_xlabel(f"truth {coord} [MeV/c]")
            if j == 0:
                ax.set_ylabel(f"reco {coord} [MeV/c]")
    fig.suptitle("truth vs reco (target frame)  NN (top)  RK (bottom)")
    fig.tight_layout()
    fig.savefig(args.out_dir / "reco_vs_truth_2d.png", dpi=130)
    plt.close(fig)

    # ---- figure 2: residual histograms, overlay NN vs RK ----
    fig, axes = plt.subplots(1, 4, figsize=(16, 4))
    res_ranges = {"px": (-40, 40), "py": (-20, 20), "pz": (-60, 60), "pmag": (-60, 60)}
    arrays = {
        "px":  (df["nn_dpx"].to_numpy(), df["rk_dpx"].to_numpy()),
        "py":  (df["nn_dpy"].to_numpy(), df["rk_dpy"].to_numpy()),
        "pz":  (df["nn_dpz"].to_numpy(), df["rk_dpz"].to_numpy()),
        "pmag": (nn_p - truth_p, rk_p - truth_p),
    }
    for ax, (coord, (nn_arr, rk_arr)) in zip(axes, arrays.items()):
        rng = res_ranges[coord]
        ax.hist(nn_arr, bins=80, range=rng, histtype="step", color="C0", label="NN")
        ax.hist(rk_arr, bins=80, range=rng, histtype="step", color="C3", label="RK")
        ax.set_xlabel(f"reco - truth  {coord} [MeV/c]")
        ax.set_yscale("log")
        ax.legend(fontsize=9, loc="upper right")
    fig.suptitle("per-coord residual (matched events, target frame)")
    fig.tight_layout()
    fig.savefig(args.out_dir / "residuals_overlay.png", dpi=130)
    plt.close(fig)

    # ---- figure 3: dpx vs truth_px scatter density (the saturation diagnostic) ----
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.5), sharex=True, sharey=True)
    for ax, tag in zip(axes, ("nn", "rk")):
        ax.hist2d(df["truth_px"].to_numpy(), df[f"{tag}_dpx"].to_numpy(),
                  bins=80, range=[[-200, 200], [-60, 60]], cmap="viridis")
        ax.axhline(0, color="r", lw=0.6, alpha=0.6)
        ax.set_xlabel("truth px [MeV/c]")
        ax.set_title(f"{tag.upper()}: dpx vs truth_px")
    axes[0].set_ylabel("reco - truth  px [MeV/c]")
    fig.tight_layout()
    fig.savefig(args.out_dir / "dpx_vs_truth_px.png", dpi=130)
    plt.close(fig)

    print(f"[compare] figures -> {args.out_dir}")


if __name__ == "__main__":
    main()
