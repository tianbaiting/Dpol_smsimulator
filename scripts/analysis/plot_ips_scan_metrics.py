#!/usr/bin/env python3

import argparse
import csv
import os
import tempfile
from pathlib import Path

# [EN] Force a writable Matplotlib cache location so batch plotting works on shared machines. / [CN] 强制使用可写的 Matplotlib 缓存目录，避免共享机器上的批处理绘图失败。
os.environ.setdefault("MPLCONFIGDIR", tempfile.mkdtemp(prefix="mplconfig_"))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot IPS scan elastic leakage and small-b acceptance versus offset."
    )
    parser.add_argument(
        "--input-csv",
        type=Path,
        required=True,
        help="Path to ips_scan_summary.csv",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="Directory to store generated figures",
    )
    parser.add_argument(
        "--prefix",
        type=str,
        default="ips_scan",
        help="Output file prefix",
    )
    parser.add_argument(
        "--leakage-limit",
        type=float,
        default=0.005,
        help="Elastic leakage feasibility threshold",
    )
    parser.add_argument(
        "--title",
        type=str,
        default="IPS scan metrics",
        help="Figure title prefix",
    )
    return parser.parse_args()


def load_scan_rows(csv_path: Path) -> list[dict]:
    rows: list[dict] = []
    with csv_path.open("r", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            if row.get("phase") != "scan":
                continue
            rows.append(
                {
                    "stage": row["stage"],
                    "rank": int(row["rank"]),
                    "offset_mm": float(row["offset_mm"]),
                    "feasible": int(row["feasible"]) == 1,
                    "elastic_leakage": float(row["elastic_leakage"]),
                    "smallb_selected_rate": float(row["smallb_selected_rate"]),
                    "smallb_selected_total": int(row["smallb_selected_total"]),
                    "smallb_selected_ips": int(row["smallb_selected_ips"]),
                    "elastic_selected_total": int(row["elastic_selected_total"]),
                    "elastic_selected_ips": int(row["elastic_selected_ips"]),
                }
            )
    rows.sort(key=lambda item: item["offset_mm"])
    if not rows:
        raise RuntimeError(f"No scan rows found in {csv_path}")
    return rows


def stage_marker(stage: str) -> str:
    if stage == "coarse":
        return "s"
    if stage == "refine":
        return "o"
    if stage == "coarse+refine":
        return "D"
    return "^"


def plot_metrics(rows: list[dict], output_dir: Path, prefix: str, leakage_limit: float, title: str) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)

    offsets = [row["offset_mm"] for row in rows]
    leakages = [row["elastic_leakage"] for row in rows]
    acceptance = [row["smallb_selected_rate"] for row in rows]
    best_row = min(rows, key=lambda item: item["rank"])

    fig, axes = plt.subplots(2, 1, figsize=(10.5, 8.0), sharex=True)
    leakage_ax, acceptance_ax = axes

    leakage_ax.plot(offsets, leakages, color="#c0392b", linewidth=1.8, alpha=0.8)
    leakage_ax.axhline(
        leakage_limit,
        color="#7f8c8d",
        linestyle="--",
        linewidth=1.3,
        label=f"leakage limit = {leakage_limit:.3f}",
    )
    for row in rows:
        leakage_ax.scatter(
            row["offset_mm"],
            row["elastic_leakage"],
            color="#1f77b4" if row["feasible"] else "#d62728",
            marker=stage_marker(row["stage"]),
            s=54,
            zorder=3,
        )
    leakage_ax.annotate(
        f"best = {best_row['offset_mm']:.0f} mm\n{best_row['elastic_leakage']:.4f}",
        xy=(best_row["offset_mm"], best_row["elastic_leakage"]),
        xytext=(16, 18),
        textcoords="offset points",
        arrowprops={"arrowstyle": "->", "lw": 1.0},
        fontsize=10,
    )
    leakage_ax.set_ylabel("Elastic leakage")
    leakage_ax.set_title(f"{title}: leakage and small-b acceptance vs offset")
    leakage_ax.grid(True, alpha=0.25)
    leakage_ax.legend(loc="upper left")

    acceptance_ax.plot(offsets, acceptance, color="#2c3e50", linewidth=1.8, alpha=0.8)
    for row in rows:
        acceptance_ax.scatter(
            row["offset_mm"],
            row["smallb_selected_rate"],
            color="#1f77b4" if row["feasible"] else "#d62728",
            marker=stage_marker(row["stage"]),
            s=54,
            zorder=3,
        )
    acceptance_ax.annotate(
        f"best = {best_row['smallb_selected_rate']:.4f}",
        xy=(best_row["offset_mm"], best_row["smallb_selected_rate"]),
        xytext=(16, -24),
        textcoords="offset points",
        arrowprops={"arrowstyle": "->", "lw": 1.0},
        fontsize=10,
    )
    acceptance_ax.set_xlabel("IPS axis offset (mm)")
    acceptance_ax.set_ylabel("Small-b IPS acceptance")
    acceptance_ax.grid(True, alpha=0.25)

    fig.tight_layout()
    metrics_png = output_dir / f"{prefix}_metrics_vs_offset.png"
    metrics_pdf = output_dir / f"{prefix}_metrics_vs_offset.pdf"
    fig.savefig(metrics_png, dpi=220, bbox_inches="tight")
    fig.savefig(metrics_pdf, bbox_inches="tight")
    plt.close(fig)

    tradeoff_fig, tradeoff_ax = plt.subplots(figsize=(8.8, 6.6))
    for row in rows:
        tradeoff_ax.scatter(
            row["elastic_leakage"],
            row["smallb_selected_rate"],
            color="#1f77b4" if row["feasible"] else "#d62728",
            marker=stage_marker(row["stage"]),
            s=70,
            zorder=3,
        )
        if row["rank"] <= 5:
            tradeoff_ax.annotate(
                f"{row['offset_mm']:.0f} mm",
                (row["elastic_leakage"], row["smallb_selected_rate"]),
                textcoords="offset points",
                xytext=(8, 4),
                fontsize=9,
            )
    tradeoff_ax.axvline(
        leakage_limit,
        color="#7f8c8d",
        linestyle="--",
        linewidth=1.3,
        label=f"leakage limit = {leakage_limit:.3f}",
    )
    tradeoff_ax.set_xlabel("Elastic leakage")
    tradeoff_ax.set_ylabel("Small-b IPS acceptance")
    tradeoff_ax.set_title(f"{title}: trade-off curve")
    tradeoff_ax.grid(True, alpha=0.25)
    tradeoff_ax.legend(loc="lower left")
    tradeoff_fig.tight_layout()
    tradeoff_png = output_dir / f"{prefix}_tradeoff.png"
    tradeoff_pdf = output_dir / f"{prefix}_tradeoff.pdf"
    tradeoff_fig.savefig(tradeoff_png, dpi=220, bbox_inches="tight")
    tradeoff_fig.savefig(tradeoff_pdf, bbox_inches="tight")
    plt.close(tradeoff_fig)


def main() -> int:
    args = parse_args()
    rows = load_scan_rows(args.input_csv)
    plot_metrics(rows, args.output_dir, args.prefix, args.leakage_limit, args.title)
    print(f"plotted {len(rows)} scan candidates from {args.input_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
