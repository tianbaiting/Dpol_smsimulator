#!/usr/bin/env python3
# [EN] Usage: python3 scripts/visualization/reco_vis.py --input /path/to/output.root --event 0 --method threepoint --out results/reco_vis / [CN] 用法: python3 scripts/visualization/reco_vis.py --input /path/to/output.root --event 0 --method threepoint --out results/reco_vis

import argparse
import csv
import os
import subprocess
import sys
from dataclasses import dataclass
from typing import List, Tuple

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


@dataclass
class RecoData:
    method: str
    momentum: Tuple[float, float, float]
    final_distance: float
    target: Tuple[float, float, float]
    pdc_hits: List[Tuple[float, float, float]]
    track_points: List[Tuple[float, float, float]]


def run_dump_macro(args: argparse.Namespace, out_csv: str) -> None:
    # [EN] Run ROOT macro to dump reconstruction points into CSV. / [CN] 运行ROOT宏导出重建点到CSV。
    sms = os.environ.get("SMSIMDIR", os.getcwd())
    geom = args.geom
    if not os.path.isabs(geom):
        geom = os.path.join(sms, geom)

    fx, fy, fz = [float(x) for x in args.fixed_momentum.split(",")]

    cmd = (
        f"root -l -q '{sms}/scripts/visualization/dump_reco_track.C(\"{args.input}\","
        f" {args.event}, \"{args.method}\", \"{geom}\", \"{out_csv}\","
        f" {args.pmin}, {args.pmax}, {args.pinit}, {args.lr}, {args.tol}, {args.max_iter},"
        f" {args.pdc_sigma}, {args.target_sigma}, {fx}, {fy}, {fz})'"
    )
    subprocess.run(cmd, shell=True, check=False)


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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, help="Input ROOT file (g4 output)")
    parser.add_argument("--event", type=int, default=0)
    parser.add_argument("--geom", default="configs/simulation/geometry/5deg_1.2T.mac")
    parser.add_argument("--method", default="threepoint", choices=["grid", "gd", "minuit", "threepoint"])
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

    os.makedirs(args.out, exist_ok=True)
    out_csv = os.path.join(args.out, f"reco_event{args.event}_{args.method}.csv")

    run_dump_macro(args, out_csv)
    if not os.path.exists(out_csv):
        print("No CSV output generated. Check if PDC hits exist in the selected events.")
        return 1
    data = parse_csv(out_csv)

    plot_plane(data, "xz", os.path.join(args.out, f"reco_{args.method}_xz_event{args.event}.png"))
    plot_plane(data, "xy", os.path.join(args.out, f"reco_{args.method}_xy_event{args.event}.png"))

    print(f"Method: {data.method}")
    print(f"Best momentum: ({data.momentum[0]:.3f}, {data.momentum[1]:.3f}, {data.momentum[2]:.3f}) MeV/c")
    print(f"Final distance: {data.final_distance:.3f} mm")
    print(f"CSV: {out_csv}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
