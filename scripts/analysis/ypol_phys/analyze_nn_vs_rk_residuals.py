#!/usr/bin/env python3
"""Compute and plot RK vs NN proton-momentum residuals on ypol elastic_allair.

[EN] Reads per-cell CSVs produced by extract_proton_rk_nn.C, pools all 16 cells,
restricts to truth_has_proton==1 events with valid RK and NN reco (status==0,
non-zero p), then writes residual histograms and a JSON summary table.
"""
import argparse, json, os
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

COMP = ["px", "py", "pz"]


def half_width(x):
    if len(x) < 5:
        return float("nan")
    p84, p16 = np.percentile(x, [84.135, 15.865])
    return 0.5 * (p84 - p16)


def stats(arr):
    arr = arr[np.isfinite(arr)]
    if len(arr) == 0:
        return {"n": 0}
    return {
        "n": int(len(arr)),
        "median": float(np.median(arr)),
        "mean": float(np.mean(arr)),
        "rmse": float(np.sqrt(np.mean(arr ** 2))),
        "half_width": float(half_width(arr)),
        "p95_abs": float(np.percentile(np.abs(arr), 95)),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv-dir", required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    csv_paths = sorted(Path(args.csv_dir).glob("*.csv"))
    print(f"[analyze] reading {len(csv_paths)} CSVs from {args.csv_dir}")
    dfs = [pd.read_csv(p) for p in csv_paths]
    df = pd.concat(dfs, ignore_index=True)
    print(f"[analyze] total rows = {len(df)}")

    n_truth = int(df["truth_has_proton"].sum())
    df_t = df[df["truth_has_proton"] == 1].copy()
    rk_ok = (df_t["rk_status"] == 0) & ((df_t[["rk_pxp", "rk_pyp", "rk_pzp"]].abs().sum(axis=1)) > 0)
    nn_ok = (df_t["nn_status"] == 0) & ((df_t[["nn_pxp", "nn_pyp", "nn_pzp"]].abs().sum(axis=1)) > 0)
    df_t["rk_ok"] = rk_ok
    df_t["nn_ok"] = nn_ok

    print(f"[analyze] truth_has_proton={n_truth}, rk_ok={int(rk_ok.sum())}, nn_ok={int(nn_ok.sum())}")
    print(f"[analyze] both_ok={int((rk_ok & nn_ok).sum())}")

    # restrict to rows where BOTH have a valid reco -> apples-to-apples comparison
    df_e = df_t[rk_ok & nn_ok].copy()

    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    # residuals
    res = {}
    for method in ("rk", "nn"):
        res[method] = {}
        for c in COMP:
            res[method][c] = (df_e[f"{method}_p{c[1]}p"] - df_e[f"truth_p{c[1]}p"]).values
        # |p| residual
        truth_p = np.linalg.norm(df_e[["truth_pxp", "truth_pyp", "truth_pzp"]].values, axis=1)
        reco_p = np.linalg.norm(df_e[[f"{method}_pxp", f"{method}_pyp", f"{method}_pzp"]].values, axis=1)
        res[method]["pmag"] = reco_p - truth_p

    summary = {
        "n_total": int(len(df)),
        "n_truth_has_proton": n_truth,
        "n_rk_ok": int(rk_ok.sum()),
        "n_nn_ok": int(nn_ok.sum()),
        "n_both_ok": int(len(df_e)),
        "rk": {k: stats(v) for k, v in res["rk"].items()},
        "nn": {k: stats(v) for k, v in res["nn"].items()},
    }
    with (out / "summary.json").open("w") as f:
        json.dump(summary, f, indent=2)

    # 2x4 grid: rows = method (RK, NN), cols = px/py/pz/|p|
    # Wider range + log y so RK heavy tails (px RMSE 17, pz RMSE 46) become visible.
    fig, axes = plt.subplots(2, 4, figsize=(16, 7), sharex="col")
    method_titles = {"rk": "RK", "nn": "NN (formal_B115T3deg)"}
    col_titles = [r"$\Delta p_x$", r"$\Delta p_y$", r"$\Delta p_z$", r"$\Delta |\vec p|$"]
    keys = ["px", "py", "pz", "pmag"]
    # Wide ranges + log y so the catastrophic RK failures are visible:
    #  RK pz tail extends to +2535, |p| tail to +3750; 1097 events have |Δpz|>300
    #  vs 26 for NN. Range ±500 catches the bulk of the RK tail population while
    #  keeping per-bin width <10 MeV/c so the central peak stays resolved.
    ranges = {"px": (-500, 500), "py": (-30, 30), "pz": (-500, 500), "pmag": (-500, 500)}
    for r, m in enumerate(("rk", "nn")):
        for c, k in enumerate(keys):
            ax = axes[r, c]
            data = res[m][k]
            lo, hi = ranges[k]
            ax.hist(data, bins=120, range=(lo, hi), histtype="step", color=("C0" if m == "rk" else "C3"), linewidth=1.5)
            med = np.median(data)
            hw = half_width(data)
            ax.axvline(med, ls="--", color="k", lw=0.8)
            ax.set_title(f"{method_titles[m]}: {col_titles[c]}\nN={len(data)}, med={med:+.2f}, hw={hw:.2f}", fontsize=10)
            ax.set_xlim(lo, hi)
            ax.set_yscale("log")
            ax.set_ylim(bottom=0.5)
            if r == 1:
                ax.set_xlabel("reco - truth (MeV/c)")
            if c == 0:
                ax.set_ylabel("events (log)")
            ax.grid(alpha=0.3, which="both")
    fig.suptitle("Proton momentum residuals on ypol_new_20260413_elastic_allair (16 cells, 160k events, both reco OK)", fontsize=11)
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    fig.savefig(out / "residuals_grid.png", dpi=130)
    plt.close(fig)

    # overlay 1x4: RK vs NN per component, log y, wide range — shows tail divergence
    fig2, axes2 = plt.subplots(1, 4, figsize=(16, 4))
    for c, k in enumerate(keys):
        ax = axes2[c]
        lo, hi = ranges[k]
        ax.hist(res["rk"][k], bins=120, range=(lo, hi), histtype="step", color="C0", lw=1.6, label="RK")
        ax.hist(res["nn"][k], bins=120, range=(lo, hi), histtype="step", color="C3", lw=1.6, label="NN")
        ax.set_xlabel("reco - truth (MeV/c)")
        ax.set_title(col_titles[c])
        ax.set_xlim(lo, hi)
        ax.set_yscale("log")
        ax.set_ylim(bottom=0.5)
        ax.grid(alpha=0.3, which="both")
        if c == 0:
            ax.legend()
            ax.set_ylabel("events (log)")
    fig2.suptitle("RK vs NN residual overlay (proton, 16 cells pooled, log y reveals RK tails)", fontsize=11)
    fig2.tight_layout(rect=[0, 0, 1, 0.94])
    fig2.savefig(out / "residuals_overlay.png", dpi=130)
    plt.close(fig2)

    # 2D scatter: reco vs truth for each component, both methods.
    # Hardcoded per-column ranges so RK outliers don't compress the useful diagonal.
    # log color scale for the 2D density (RK has long thin spike + few outliers).
    from matplotlib.colors import LogNorm
    fig3, axes3 = plt.subplots(2, 3, figsize=(13, 8))
    truth_cols = ["truth_pxp", "truth_pyp", "truth_pzp"]
    reco_cols_rk = ["rk_pxp", "rk_pyp", "rk_pzp"]
    reco_cols_nn = ["nn_pxp", "nn_pyp", "nn_pzp"]
    # px/py: ±200 / ±100; pz: 400-800 (truth pz median ~627).
    axis_ranges = {"px": (-200, 200), "py": (-100, 100), "pz": (400, 800)}
    for r, (label, rcols, color) in enumerate([("RK", reco_cols_rk, "C0"), ("NN", reco_cols_nn, "C3")]):
        for c, comp in enumerate(COMP):
            ax = axes3[r, c]
            x = df_e[truth_cols[c]].values
            y = df_e[rcols[c]].values
            mask = np.isfinite(x) & np.isfinite(y)
            xlo, xhi = axis_ranges[comp]
            ylo, yhi = axis_ranges[comp]
            ax.hist2d(x[mask], y[mask], bins=120, range=[[xlo, xhi], [ylo, yhi]],
                      cmap="viridis", norm=LogNorm())
            ax.plot([xlo, xhi], [xlo, xhi], 'r--', lw=1)
            ax.set_xlim(xlo, xhi); ax.set_ylim(ylo, yhi)
            ax.set_xlabel(f"truth {comp} (MeV/c)")
            ax.set_ylabel(f"{label} reco {comp} (MeV/c)")
            ax.set_title(f"{label}: {comp}")
    fig3.suptitle("reco vs truth 2D, proton (16 cells pooled, fixed axes; RK outliers clipped)", fontsize=11)
    fig3.tight_layout(rect=[0, 0, 1, 0.95])
    fig3.savefig(out / "reco_vs_truth_2d.png", dpi=130)
    plt.close(fig3)

    # per-cell summary for table
    cell_rows = []
    for tag, sub in df_e.groupby("tag"):
        for method in ("rk", "nn"):
            row = {"tag": tag, "method": method, "n": len(sub)}
            for c in COMP:
                d = (sub[f"{method}_p{c[1]}p"] - sub[f"truth_p{c[1]}p"]).values
                row[f"{c}_med"] = float(np.median(d))
                row[f"{c}_hw"] = half_width(d)
            cell_rows.append(row)
    pd.DataFrame(cell_rows).to_csv(out / "per_cell_summary.csv", index=False)

    # human-readable summary
    lines = []
    lines.append(f"# Proton residual summary (ypol elastic_allair, 16 cells)")
    lines.append(f"total events read: {summary['n_total']}")
    lines.append(f"truth_has_proton:  {n_truth}")
    lines.append(f"RK ok / NN ok / both ok: {summary['n_rk_ok']} / {summary['n_nn_ok']} / {summary['n_both_ok']}")
    lines.append("")
    lines.append("RK residuals (apples-to-apples on both-ok subset):")
    for k in keys:
        s = summary["rk"][k]
        lines.append(f"  {k:5s}: med={s['median']:+.3f}  hw={s['half_width']:.3f}  rmse={s['rmse']:.3f}  p95|.|={s['p95_abs']:.3f}")
    lines.append("")
    lines.append("NN residuals (same subset):")
    for k in keys:
        s = summary["nn"][k]
        lines.append(f"  {k:5s}: med={s['median']:+.3f}  hw={s['half_width']:.3f}  rmse={s['rmse']:.3f}  p95|.|={s['p95_abs']:.3f}")
    txt = "\n".join(lines)
    print(txt)
    (out / "summary.txt").write_text(txt + "\n")
    print(f"[analyze] wrote -> {out}")


if __name__ == "__main__":
    main()
