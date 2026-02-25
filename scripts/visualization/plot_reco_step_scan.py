#!/usr/bin/env python3
# [EN] Usage: python3 scripts/visualization/plot_reco_step_scan.py --csv results/reco_step_scan/B115T_3deg/reco_step_scan.csv --out results/reco_step_scan/B115T_3deg/plots / [CN] 用法: python3 scripts/visualization/plot_reco_step_scan.py --csv results/reco_step_scan/B115T_3deg/reco_step_scan.csv --out results/reco_step_scan/B115T_3deg/plots

import argparse
import csv
import os
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load_rows(path: str):
    rows = []
    with open(path, "r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            row["step_size"] = float(row["step_size"])
            row["mean_d_pdc"] = float(row["mean_d_pdc"])
            row["rms_d_pdc"] = float(row["rms_d_pdc"])
            row["mean_d_target"] = float(row["mean_d_target"])
            row["success_rate"] = float(row["success_rate"])
            row["count"] = int(row["count"])
            rows.append(row)
    return rows


def plot_metric(rows, out_path, metric, ylabel):
    grouped = defaultdict(list)
    for row in rows:
        if row["count"] <= 0:
            continue
        grouped[row["method"]].append(row)

    fig, ax = plt.subplots(figsize=(10, 6))
    colors = {"minuit": "#1f77b4", "threepoint": "#d62728"}
    markers = {"minuit": "o", "threepoint": "s"}

    for method, items in grouped.items():
        items = sorted(items, key=lambda x: x["step_size"])
        x = [r["step_size"] for r in items]
        y = [r[metric] for r in items]
        ax.plot(
            x,
            y,
            marker=markers.get(method, "o"),
            color=colors.get(method, "black"),
            linewidth=2,
            label=method,
        )

    ax.set_xlabel("Runge-Kutta Step Size [mm]")
    ax.set_ylabel(ylabel)
    ax.set_title(f"{ylabel} vs Step Size")
    ax.grid(True, linestyle="--", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", required=True, help="CSV from compare_reco_step_sizes.C")
    parser.add_argument("--out", default="results/reco_step_scan/B115T_3deg/plots", help="Output directory")
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    rows = load_rows(args.csv)
    if not rows:
        raise SystemExit("No rows in CSV")

    plot_metric(rows, os.path.join(args.out, "mean_d_pdc_vs_step.png"), "mean_d_pdc", "Mean Distance to PDC Deposits [mm]")
    plot_metric(rows, os.path.join(args.out, "mean_d_target_vs_step.png"), "mean_d_target", "Mean Distance to Target [mm]")
    plot_metric(rows, os.path.join(args.out, "success_rate_vs_step.png"), "success_rate", "Reconstruction Success Rate")

    print(f"Saved plots to {args.out}")


if __name__ == "__main__":
    main()
