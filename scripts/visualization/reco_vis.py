#!/usr/bin/env python3
# [EN] Usage: python3 scripts/visualization/reco_vis.py --input /path/to/output.root --event 0 --method threepoint --out results/reco_vis / [CN] 用法: python3 scripts/visualization/reco_vis.py --input /path/to/output.root --event 0 --method threepoint --out results/reco_vis

import argparse
import csv
import os
import subprocess
import sys
from dataclasses import dataclass
from typing import Dict, List, Tuple

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ALLOWED_METHODS = ("grid", "gd", "minuit", "threepoint")


@dataclass
class RecoData:
    method: str
    momentum: Tuple[float, float, float]
    final_distance: float
    target: Tuple[float, float, float]
    pdc_hits: List[Tuple[float, float, float]]
    track_points: List[Tuple[float, float, float]]


def run_dump_macro(args: argparse.Namespace, method: str, out_csv: str) -> int:
    # [EN] Run ROOT macro to dump reconstruction points into CSV. / [CN] 运行ROOT宏导出重建点到CSV。
    sms = os.environ.get("SMSIMDIR", os.getcwd())
    geom = args.geom
    if not os.path.isabs(geom):
        geom = os.path.join(sms, geom)

    fx, fy, fz = [float(x) for x in args.fixed_momentum.split(",")]

    cmd = (
        f"root -l -q '{sms}/scripts/visualization/dump_reco_track.C(\"{args.input}\","
        f" {args.event}, \"{method}\", \"{geom}\", \"{out_csv}\","
        f" {args.pmin}, {args.pmax}, {args.pinit}, {args.lr}, {args.tol}, {args.max_iter},"
        f" {args.pdc_sigma}, {args.target_sigma}, {fx}, {fy}, {fz})'"
    )
    proc = subprocess.run(cmd, shell=True, check=False)
    return proc.returncode


def parse_methods(raw: str) -> List[str]:
    # [EN] Support one method, comma list, or all methods in a single run. / [CN] 支持单个方法、逗号列表或一次运行全部方法。
    token = raw.strip().lower()
    if token == "all":
        return list(ALLOWED_METHODS)

    methods = []
    seen = set()
    for part in token.split(","):
        m = part.strip().lower()
        if not m:
            continue
        if m not in ALLOWED_METHODS:
            raise ValueError(f"Unsupported method: {m}. Allowed: {', '.join(ALLOWED_METHODS)} or all")
        if m not in seen:
            methods.append(m)
            seen.add(m)
    if not methods:
        raise ValueError("No valid method parsed from --method")
    return methods


def parse_csv(path: str) -> RecoData:
    # [EN] Parse CSV output from ROOT macro. / [CN] 解析ROOT宏输出的CSV。
    method = ""
    momentum = (0.0, 0.0, 0.0)
    final_distance = 0.0
    target = (0.0, 0.0, 0.0)
    pdc_hits: List[Tuple[float, float, float]] = []
    track_points: List[Tuple[float, float, float]] = []

    with open(path, "r", newline="") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith("#"):
                if line.startswith("# method="):
                    method = line.split("=", 1)[1]
                elif line.startswith("# momentum="):
                    parts = line.split("=", 1)[1].split(",")
                    momentum = (float(parts[0]), float(parts[1]), float(parts[2]))
                elif line.startswith("# final_distance="):
                    final_distance = float(line.split("=", 1)[1])
                elif line.startswith("# target="):
                    parts = line.split("=", 1)[1].split(",")
                    target = (float(parts[0]), float(parts[1]), float(parts[2]))
                continue

            if line.startswith("kind,"):
                continue

            row = next(csv.reader([line]))
            kind = row[0]
            x, y, z = float(row[1]), float(row[2]), float(row[3])
            if kind == "pdc":
                pdc_hits.append((x, y, z))
            elif kind == "track":
                track_points.append((x, y, z))

    return RecoData(method, momentum, final_distance, target, pdc_hits, track_points)


def plot_plane(data: RecoData, plane: str, out_path: str) -> None:
    # [EN] Plot 2D projection for hits and track. / [CN] 绘制击中点与轨迹的二维投影。
    fig, ax = plt.subplots(figsize=(10, 7))

    def proj(points: List[Tuple[float, float, float]]):
        if plane == "xz":
            return [p[0] for p in points], [p[2] for p in points]
        if plane == "xy":
            return [p[0] for p in points], [p[1] for p in points]
        return [p[1] for p in points], [p[2] for p in points]

    if data.pdc_hits:
        xh, yh = proj(data.pdc_hits)
        ax.scatter(xh, yh, s=10, color="black", label="PDC hits")

    if data.track_points:
        xt, yt = proj(data.track_points)
        ax.plot(xt, yt, color="red", linewidth=2, label="Reconstructed track")

    tx, ty = proj([data.target])
    ax.scatter(tx, ty, s=60, color="blue", marker="*", label="Target")

    if plane == "xz":
        ax.set_xlabel("X [mm]")
        ax.set_ylabel("Z [mm]")
    elif plane == "xy":
        ax.set_xlabel("X [mm]")
        ax.set_ylabel("Y [mm]")
    else:
        ax.set_xlabel("Y [mm]")
        ax.set_ylabel("Z [mm]")

    ax.set_title(f"Method: {data.method} | p=({data.momentum[0]:.1f}, {data.momentum[1]:.1f}, {data.momentum[2]:.1f}) MeV/c | d={data.final_distance:.2f} mm")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def plot_compare_plane(data_map: Dict[str, RecoData], plane: str, out_path: str) -> None:
    # [EN] Overlay multiple reconstruction methods on one plane for direct comparison. / [CN] 在同一平面叠加多种重建方法以便直接比较。
    fig, ax = plt.subplots(figsize=(10, 7))

    def proj(points: List[Tuple[float, float, float]]):
        if plane == "xz":
            return [p[0] for p in points], [p[2] for p in points]
        if plane == "xy":
            return [p[0] for p in points], [p[1] for p in points]
        return [p[1] for p in points], [p[2] for p in points]

    first = next(iter(data_map.values()))
    if first.pdc_hits:
        xh, yh = proj(first.pdc_hits)
        ax.scatter(xh, yh, s=10, color="black", label="PDC hits")

    colors = {
        "grid": "#1f77b4",
        "gd": "#ff7f0e",
        "minuit": "#2ca02c",
        "threepoint": "#d62728",
    }

    for method in ALLOWED_METHODS:
        if method not in data_map:
            continue
        data = data_map[method]
        if data.track_points:
            xt, yt = proj(data.track_points)
            if len(data.track_points) > 1:
                ax.plot(xt, yt, color=colors.get(method, "gray"), linewidth=2, label=f"{method} (d={data.final_distance:.2f} mm)")
            else:
                ax.scatter(xt, yt, color=colors.get(method, "gray"), s=40, marker="x", label=f"{method} (single point)")

    tx, ty = proj([first.target])
    ax.scatter(tx, ty, s=60, color="blue", marker="*", label="Target")

    if plane == "xz":
        ax.set_xlabel("X [mm]")
        ax.set_ylabel("Z [mm]")
    elif plane == "xy":
        ax.set_xlabel("X [mm]")
        ax.set_ylabel("Y [mm]")
    else:
        ax.set_xlabel("Y [mm]")
        ax.set_ylabel("Z [mm]")

    ax.set_title("Reconstructed Track Comparison")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, help="Input ROOT file (g4 output)")
    parser.add_argument("--event", type=int, default=0)
    parser.add_argument("--geom", default="configs/simulation/geometry/5deg_1.2T.mac")
    parser.add_argument("--method", default="threepoint", help="Reconstruction method: grid|gd|minuit|threepoint|all|comma-list")
    parser.add_argument("--out", default="results/reco_vis")
    parser.add_argument("--pmin", type=float, default=50.0)
    parser.add_argument("--pmax", type=float, default=5000.0)
    parser.add_argument("--pinit", type=float, default=1000.0)
    parser.add_argument("--lr", type=float, default=0.05)
    parser.add_argument("--tol", type=float, default=1.0)
    parser.add_argument("--max-iter", type=int, default=200)
    parser.add_argument("--pdc-sigma", type=float, default=0.5)
    parser.add_argument("--target-sigma", type=float, default=5.0)
    parser.add_argument("--fixed-momentum", default="0,0,627")
    args = parser.parse_args()

    methods = parse_methods(args.method)
    os.makedirs(args.out, exist_ok=True)
    data_map: Dict[str, RecoData] = {}
    produced_csv = []

    for method in methods:
        out_csv = os.path.join(args.out, f"reco_event{args.event}_{method}.csv")
        rc = run_dump_macro(args, method, out_csv)
        if not os.path.exists(out_csv):
            print(f"[WARN] No CSV output for method={method}, root_rc={rc}")
            continue

        data = parse_csv(out_csv)
        if not data.method:
            data.method = method
        data_map[method] = data
        produced_csv.append(out_csv)

        plot_plane(data, "xz", os.path.join(args.out, f"reco_{method}_xz_event{args.event}.png"))
        plot_plane(data, "xy", os.path.join(args.out, f"reco_{method}_xy_event{args.event}.png"))

    if not data_map:
        print("No CSV output generated. Check input event or PDC hits.")
        return 1

    if len(data_map) > 1:
        compare_xz = os.path.join(args.out, f"reco_compare_xz_event{args.event}.png")
        compare_xy = os.path.join(args.out, f"reco_compare_xy_event{args.event}.png")
        plot_compare_plane(data_map, "xz", compare_xz)
        plot_compare_plane(data_map, "xy", compare_xy)
        print(f"Compare XZ: {compare_xz}")
        print(f"Compare XY: {compare_xy}")

    for method in methods:
        if method not in data_map:
            continue
        data = data_map[method]
        print(f"Method: {data.method}")
        print(f"Best momentum: ({data.momentum[0]:.3f}, {data.momentum[1]:.3f}, {data.momentum[2]:.3f}) MeV/c")
        print(f"Final distance: {data.final_distance:.3f} mm")

    for csv_path in produced_csv:
        print(f"CSV: {csv_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
