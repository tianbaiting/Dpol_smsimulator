#!/usr/bin/env python3

import argparse
import math
import re
from dataclasses import dataclass
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon


FLOAT_PATTERN = re.compile(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?")


@dataclass(frozen=True)
class SolidShape:
    name: str
    color: tuple[float, float, float]
    points: list[tuple[float, float, float]]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render an X-Z top view from a Geant4 VRML/WRL file.")
    parser.add_argument("--input-wrl", type=Path, required=True)
    parser.add_argument("--output-png", type=Path, required=True)
    parser.add_argument("--dpi", type=int, default=220)
    parser.add_argument("--require-s021-counts", action="store_true")
    return parser.parse_args()


def parse_floats(line: str) -> list[float]:
    return [float(value) for value in FLOAT_PATTERN.findall(line)]


def read_wrl_solids(wrl_path: Path) -> list[SolidShape]:
    solids: list[SolidShape] = []
    current_name = ""
    current_color = (0.45, 0.45, 0.45)
    collecting_points = False
    points: list[tuple[float, float, float]] = []

    with wrl_path.open("r", encoding="utf-8", errors="replace") as stream:
        for line in stream:
            stripped = line.strip()
            if stripped.startswith("#----------"):
                current_name = stripped.removeprefix("#----------").strip()
                current_color = (0.45, 0.45, 0.45)
                collecting_points = False
                points = []
                continue
            if stripped.startswith("diffuseColor"):
                values = parse_floats(stripped)
                if len(values) >= 3:
                    current_color = (values[0], values[1], values[2])
                continue
            if stripped.startswith("point ["):
                collecting_points = True
                points = []
                continue
            if not collecting_points:
                continue
            if "]" in stripped:
                collecting_points = False
                if current_name.startswith("SOLID") and points:
                    solids.append(SolidShape(current_name, current_color, points))
                continue
            values = parse_floats(stripped)
            if len(values) >= 3:
                points.append((values[0], values[1], values[2]))
    return solids


def convex_hull(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    unique_points = sorted(set(points))
    if len(unique_points) <= 2:
        return unique_points

    def cross(origin: tuple[float, float], first: tuple[float, float], second: tuple[float, float]) -> float:
        return (first[0] - origin[0]) * (second[1] - origin[1]) - (first[1] - origin[1]) * (second[0] - origin[0])

    lower: list[tuple[float, float]] = []
    for point in unique_points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], point) <= 0.0:
            lower.pop()
        lower.append(point)

    upper: list[tuple[float, float]] = []
    for point in reversed(unique_points):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], point) <= 0.0:
            upper.pop()
        upper.append(point)

    return lower[:-1] + upper[:-1]


def clamp_color(color: tuple[float, float, float]) -> tuple[float, float, float]:
    return tuple(max(0.0, min(1.0, channel)) for channel in color)


def projected_points(shape: SolidShape) -> list[tuple[float, float]]:
    return [(point[2] / 1000.0, point[0] / 1000.0) for point in shape.points]


def center_z_mm(shape: SolidShape) -> float:
    return sum(point[2] for point in shape.points) / max(len(shape.points), 1)


def group_solids(solids: list[SolidShape]) -> dict[str, list[SolidShape]]:
    groups = {
        "magnet": [],
        "support": [],
        "pdc": [],
        "nebula_neutron": [],
        "nebula_veto": [],
        "plus_neutron": [],
        "plus_veto": [],
    }
    for shape in solids:
        name = shape.name
        z_mm = center_z_mm(shape)
        if "Neut:" in name:
            groups["plus_neutron" if z_mm < 9500.0 else "nebula_neutron"].append(shape)
        elif "Veto:" in name:
            groups["plus_veto" if z_mm < 9500.0 else "nebula_veto"].append(shape)
        elif "PDC" in name:
            groups["pdc"].append(shape)
        elif "yoke" in name:
            groups["magnet"].append(shape)
        elif any(token in name for token in ("vchamber", "Win", "vacuum")):
            groups["support"].append(shape)
    return groups


def add_shape(ax: plt.Axes, shape: SolidShape, face: str | tuple[float, float, float], edge: str | tuple[float, float, float], alpha: float, linewidth: float, zorder: int) -> None:
    hull = convex_hull(projected_points(shape))
    if len(hull) < 3:
        return
    ax.add_patch(
        Polygon(
            hull,
            closed=True,
            facecolor=face,
            edgecolor=edge,
            linewidth=linewidth,
            alpha=alpha,
            zorder=zorder,
        )
    )


def group_center(shapes: list[SolidShape]) -> tuple[float, float]:
    points = [point for shape in shapes for point in projected_points(shape)]
    if not points:
        return 0.0, 0.0
    return sum(point[0] for point in points) / len(points), sum(point[1] for point in points) / len(points)


def group_bounds(shapes: list[SolidShape]) -> tuple[float, float, float, float]:
    points = [point for shape in shapes for point in projected_points(shape)]
    if not points:
        return 0.0, 0.0, 0.0, 0.0
    z_values = [point[0] for point in points]
    x_values = [point[1] for point in points]
    return min(z_values), max(z_values), min(x_values), max(x_values)


def label_group(ax: plt.Axes, shapes: list[SolidShape], text: str, color: str, x_offset_m: float) -> None:
    if not shapes:
        return
    z_min, z_max, _, x_max = group_bounds(shapes)
    ax.text(
        0.5 * (z_min + z_max),
        x_max + x_offset_m,
        text,
        color=color,
        fontsize=15,
        ha="center",
        va="bottom",
        weight="bold",
        zorder=10,
    )


def check_counts(groups: dict[str, list[SolidShape]], require_s021_counts: bool) -> None:
    counts = {
        "NEBULA neutron": len(groups["nebula_neutron"]),
        "NEBULA veto": len(groups["nebula_veto"]),
        "NEBULA Plus neutron": len(groups["plus_neutron"]),
        "NEBULA Plus veto": len(groups["plus_veto"]),
        "PDC": len(groups["pdc"]),
    }
    for key, value in counts.items():
        print(f"{key}: {value}")
    if not require_s021_counts:
        return
    expected = {
        "NEBULA neutron": 120,
        "NEBULA veto": 24,
        "NEBULA Plus neutron": 90,
        "NEBULA Plus veto": 20,
        "PDC": 2,
    }
    mismatches = [f"{key} expected {expected[key]} got {counts[key]}" for key in expected if counts[key] != expected[key]]
    if mismatches:
        raise RuntimeError("; ".join(mismatches))


def render(groups: dict[str, list[SolidShape]], output_png: Path, dpi: int) -> None:
    fig, ax = plt.subplots(figsize=(8.2, 5.4), dpi=dpi)
    fig.patch.set_facecolor("white")
    ax.set_facecolor("white")

    for shape in groups["magnet"][:1]:
        add_shape(ax, shape, face="#d5c465", edge="#7c6c24", alpha=0.45, linewidth=1.0, zorder=1)
    for shape in groups["support"]:
        add_shape(ax, shape, face=clamp_color(shape.color), edge="#9aa3ad", alpha=0.16, linewidth=0.45, zorder=2)
    for shape in groups["pdc"]:
        add_shape(ax, shape, face="#4868b7", edge="#1d2f6f", alpha=0.82, linewidth=0.60, zorder=4)

    for shape in groups["plus_veto"]:
        add_shape(ax, shape, face="#f1a25d", edge="#b26019", alpha=0.78, linewidth=0.18, zorder=5)
    for shape in groups["plus_neutron"]:
        add_shape(ax, shape, face="#69b9a5", edge="#277463", alpha=0.86, linewidth=0.18, zorder=6)
    for shape in groups["nebula_veto"]:
        add_shape(ax, shape, face="#e87465", edge="#9b3329", alpha=0.76, linewidth=0.18, zorder=5)
    for shape in groups["nebula_neutron"]:
        add_shape(ax, shape, face="#4d8cc9", edge="#1f5a94", alpha=0.86, linewidth=0.18, zorder=6)

    label_group(ax, groups["plus_neutron"], "NEBULA Plus", "#237060", 0.18)
    label_group(ax, groups["nebula_neutron"], "NEBULA", "#1f5a94", 0.18)
    if groups["pdc"]:
        pdc_z, pdc_x = group_center(groups["pdc"])
        ax.text(pdc_z, pdc_x - 0.62, "PDC1/PDC2", color="#1d2f6f", fontsize=12, ha="center", va="top", weight="bold", zorder=10)
    ax.text(0.15, 2.45, "SAMURAI", color="#5f4f18", fontsize=13, ha="center", va="center", weight="bold", zorder=10)

    ax.annotate("z", xy=(10.8, -5.0), xytext=(9.7, -5.0), arrowprops={"arrowstyle": "->", "color": "#333333", "lw": 1.0}, color="#333333", fontsize=10, ha="center", va="center")
    ax.annotate("x", xy=(10.8, -5.0), xytext=(10.8, -4.2), arrowprops={"arrowstyle": "->", "color": "#333333", "lw": 1.0}, color="#333333", fontsize=10, ha="center", va="center")

    ax.set_xlim(-3.35, 11.25)
    ax.set_ylim(-5.35, 4.05)
    ax.set_aspect("equal", adjustable="box")
    ax.axis("off")
    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, bbox_inches="tight", pad_inches=0.03)
    plt.close(fig)


def main() -> int:
    args = parse_args()
    solids = read_wrl_solids(args.input_wrl)
    groups = group_solids(solids)
    check_counts(groups, args.require_s021_counts)
    render(groups, args.output_png, args.dpi)
    print(f"wrote {args.output_png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
