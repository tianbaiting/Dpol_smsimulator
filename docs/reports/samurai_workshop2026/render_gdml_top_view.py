#!/usr/bin/env python3

import argparse
import math
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Polygon


@dataclass(frozen=True)
class SolidBox:
    x_mm: float
    z_mm: float


@dataclass(frozen=True)
class PlacedBox:
    name: str
    volume: str
    x_mm: float
    z_mm: float
    rot_y_deg: float
    size: SolidBox


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Project a Geant4 GDML geometry into a SAMURAI top-view figure.")
    parser.add_argument("--input-gdml", type=Path, required=True)
    parser.add_argument("--output-png", type=Path, required=True)
    parser.add_argument("--target-x-mm", type=float, default=-12.488)
    parser.add_argument("--target-z-mm", type=float, default=-1069.458)
    parser.add_argument("--require-s021-counts", action="store_true")
    parser.add_argument("--dpi", type=int, default=220)
    return parser.parse_args()


def child(element: ET.Element, tag: str) -> ET.Element | None:
    return next((item for item in element if item.tag == tag), None)


def number(element: ET.Element | None, key: str, default: float = 0.0) -> float:
    if element is None:
        return default
    return float(element.attrib.get(key, default))


def load_gdml(path: Path) -> tuple[dict[str, ET.Element], dict[str, ET.Element], str]:
    root = ET.parse(path).getroot()
    solids_parent = root.find("solids")
    structure_parent = root.find("structure")
    setup_parent = root.find("setup")
    if solids_parent is None or structure_parent is None or setup_parent is None:
        raise RuntimeError(f"{path} is not a complete GDML document")

    solids = {solid.attrib["name"]: solid for solid in solids_parent}
    volumes = {volume.attrib["name"]: volume for volume in structure_parent.findall("volume")}
    world = child(setup_parent, "world")
    if world is None:
        raise RuntimeError(f"{path} has no world volume")
    return solids, volumes, world.attrib["ref"]


def solid_box(solid_name: str, solids: dict[str, ET.Element]) -> SolidBox | None:
    solid = solids.get(solid_name)
    if solid is None:
        return None
    if solid.tag == "box":
        return SolidBox(float(solid.attrib["x"]), float(solid.attrib["z"]))
    if solid.tag == "trap":
        x_values = [float(solid.attrib.get(key, "0")) for key in ("x1", "x2", "x3", "x4")]
        return SolidBox(max(x_values), float(solid.attrib["z"]))
    if solid.tag in {"subtraction", "union"}:
        first = child(solid, "first")
        if first is None:
            return None
        return solid_box(first.attrib["ref"], solids)
    return None


def placed_world_boxes(solids: dict[str, ET.Element], volumes: dict[str, ET.Element], world_name: str) -> list[PlacedBox]:
    world = volumes[world_name]
    placed: list[PlacedBox] = []
    for physvol in world.findall("physvol"):
        volumeref = child(physvol, "volumeref")
        if volumeref is None:
            continue
        volume_name = volumeref.attrib["ref"]
        volume = volumes.get(volume_name)
        if volume is None:
            continue
        solidref = child(volume, "solidref")
        if solidref is None:
            continue
        size = solid_box(solidref.attrib["ref"], solids)
        if size is None:
            continue
        position = child(physvol, "position")
        rotation = child(physvol, "rotation")
        placed.append(
            PlacedBox(
                name=physvol.attrib.get("name", ""),
                volume=volume_name,
                x_mm=number(position, "x"),
                z_mm=number(position, "z"),
                rot_y_deg=number(rotation, "y"),
                size=size,
            )
        )
    return placed


def corners(box: PlacedBox) -> list[tuple[float, float]]:
    half_x = 0.5 * box.size.x_mm
    half_z = 0.5 * box.size.z_mm
    angle = math.radians(box.rot_y_deg)
    cos_a = math.cos(angle)
    sin_a = math.sin(angle)
    local = [(-half_x, -half_z), (half_x, -half_z), (half_x, half_z), (-half_x, half_z)]
    points = []
    for x_local, z_local in local:
        x_world = box.x_mm + cos_a * x_local + sin_a * z_local
        z_world = box.z_mm - sin_a * x_local + cos_a * z_local
        points.append((z_world / 1000.0, x_world / 1000.0))
    return points


def add_box(ax: plt.Axes, box: PlacedBox, face: str, edge: str, alpha: float, linewidth: float, zorder: int) -> None:
    ax.add_patch(
        Polygon(
            corners(box),
            closed=True,
            facecolor=face,
            edgecolor=edge,
            linewidth=linewidth,
            alpha=alpha,
            zorder=zorder,
        )
    )


def split_detectors(placed: list[PlacedBox]) -> dict[str, list[PlacedBox]]:
    groups = {
        "nebula_neutron": [],
        "nebula_veto": [],
        "plus_neutron": [],
        "plus_veto": [],
        "pdc": [],
        "magnet": [],
        "windows": [],
    }
    for box in placed:
        if box.volume.startswith("NeutronDetector"):
            groups["plus_neutron" if box.z_mm < 9500.0 else "nebula_neutron"].append(box)
        elif box.volume.startswith("VetoDetector"):
            groups["plus_veto" if box.z_mm < 9500.0 else "nebula_veto"].append(box)
        elif box.volume.startswith("PDC"):
            groups["pdc"].append(box)
        elif box.volume.startswith("yoke_log"):
            groups["magnet"].append(box)
        elif box.volume.startswith(("NeutronWindow", "ExitWindow", "vacuum_")):
            groups["windows"].append(box)
    return groups


def center(boxes: list[PlacedBox]) -> tuple[float, float]:
    if not boxes:
        return 0.0, 0.0
    return sum(box.z_mm for box in boxes) / (1000.0 * len(boxes)), sum(box.x_mm for box in boxes) / (1000.0 * len(boxes))


def detector_hull(boxes: list[PlacedBox]) -> tuple[float, float, float, float]:
    z_values = []
    x_values = []
    for box in boxes:
        for z_m, x_m in corners(box):
            z_values.append(z_m)
            x_values.append(x_m)
    return min(z_values), max(z_values), min(x_values), max(x_values)


def label_group(ax: plt.Axes, boxes: list[PlacedBox], text: str, color: str, x_offset_m: float) -> None:
    if not boxes:
        return
    z_min, z_max, x_min, x_max = detector_hull(boxes)
    z_mid = 0.5 * (z_min + z_max)
    ax.text(
        z_mid,
        x_max + x_offset_m,
        text,
        color=color,
        fontsize=14,
        ha="center",
        va="bottom",
        weight="bold",
        zorder=8,
    )


def draw_scene(groups: dict[str, list[PlacedBox]], target_x_mm: float, target_z_mm: float, output_png: Path, dpi: int) -> None:
    fig, ax = plt.subplots(figsize=(8.2, 5.8), dpi=dpi)
    fig.patch.set_facecolor("white")
    ax.set_facecolor("white")

    for box in groups["magnet"][:1]:
        add_box(ax, box, face="#d9c35f", edge="#77651e", alpha=0.55, linewidth=1.2, zorder=1)
    for box in groups["windows"]:
        add_box(ax, box, face="#d7dbe2", edge="#8d96a3", alpha=0.55, linewidth=0.6, zorder=2)
    for box in groups["pdc"]:
        add_box(ax, box, face="#4868b7", edge="#1d2f6f", alpha=0.82, linewidth=0.8, zorder=4)

    for box in groups["plus_veto"]:
        add_box(ax, box, face="#f1a25d", edge="#b26019", alpha=0.90, linewidth=0.25, zorder=5)
    for box in groups["plus_neutron"]:
        add_box(ax, box, face="#69b9a5", edge="#277463", alpha=0.92, linewidth=0.22, zorder=6)
    for box in groups["nebula_veto"]:
        add_box(ax, box, face="#e87465", edge="#9b3329", alpha=0.88, linewidth=0.25, zorder=5)
    for box in groups["nebula_neutron"]:
        add_box(ax, box, face="#4d8cc9", edge="#1f5a94", alpha=0.92, linewidth=0.22, zorder=6)

    target_z_m = target_z_mm / 1000.0
    target_x_m = target_x_mm / 1000.0
    ax.add_patch(Circle((target_z_m, target_x_m), 0.08, facecolor="#cf2b2b", edgecolor="#891515", linewidth=0.8, zorder=9))
    ax.plot([target_z_m - 1.15, target_z_m], [target_x_m, target_x_m], color="#cf2b2b", linewidth=1.2, zorder=8)
    ax.annotate("", xy=(target_z_m, target_x_m), xytext=(target_z_m - 0.85, target_x_m), arrowprops={"arrowstyle": "->", "color": "#cf2b2b", "lw": 1.2})

    label_group(ax, groups["plus_neutron"], "NEBULA Plus", "#237060", 0.22)
    label_group(ax, groups["nebula_neutron"], "NEBULA", "#1f5a94", 0.22)
    if groups["pdc"]:
        pdc_z, pdc_x = center(groups["pdc"])
        ax.text(pdc_z, pdc_x - 0.60, "PDC1/PDC2", color="#1d2f6f", fontsize=12, ha="center", va="top", weight="bold", zorder=8)
    ax.text(0.18, 2.35, "SAMURAI", color="#5f4f18", fontsize=13, ha="center", va="center", weight="bold", zorder=8)
    ax.text(target_z_m + 0.12, target_x_m - 0.15, "Target", color="#b51e1e", fontsize=11, ha="left", va="top", zorder=8)

    ax.annotate("z", xy=(10.8, -5.05), xytext=(9.7, -5.05), arrowprops={"arrowstyle": "->", "color": "#333333", "lw": 1.0}, color="#333333", fontsize=10, ha="center", va="center")
    ax.annotate("x", xy=(10.8, -5.05), xytext=(10.8, -4.25), arrowprops={"arrowstyle": "->", "color": "#333333", "lw": 1.0}, color="#333333", fontsize=10, ha="center", va="center")

    ax.set_xlim(-1.8, 11.35)
    ax.set_ylim(-5.35, 4.05)
    ax.set_aspect("equal", adjustable="box")
    ax.axis("off")
    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, bbox_inches="tight", pad_inches=0.03)
    plt.close(fig)


def check_counts(groups: dict[str, list[PlacedBox]], require_s021_counts: bool) -> None:
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


def main() -> int:
    args = parse_args()
    solids, volumes, world = load_gdml(args.input_gdml)
    groups = split_detectors(placed_world_boxes(solids, volumes, world))
    check_counts(groups, args.require_s021_counts)
    draw_scene(groups, args.target_x_mm, args.target_z_mm, args.output_png, args.dpi)
    print(f"wrote {args.output_png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
