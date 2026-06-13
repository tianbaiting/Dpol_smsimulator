#!/usr/bin/env python3
"""Generate Geant4/VRML px-sign trajectory figures for the gamma report."""

from __future__ import annotations

import argparse
import math
import os
import re
import shutil
import subprocess
from pathlib import Path

import ROOT


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_OUT_DIR = REPO_ROOT / "docs/reports/gamma_constraint_20260611/figures/px_sign_geant4"
DEFAULT_SIM_BIN = REPO_ROOT / "build/bin/sim_deuteron"
DEFAULT_GEOM_MACRO = REPO_ROOT / "configs/simulation/geometry/3deg1.15Tnebulaplus.mac"
DEFAULT_SIM_WORKDIR = REPO_ROOT / "configs/simulation/macros/nebulaplus"

PX_VALUES = (60.0, -60.0, 100.0, -100.0)
PZ_MEV_C = 627.0
NEUTRON_MASS_MEV = 939.5654133
FLOAT_PATTERN = re.compile(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?")


def label_for_px(px_mev_c: float) -> str:
    sign = "plus" if px_mev_c > 0.0 else "minus"
    return f"neutron_px_{sign}{abs(int(px_mev_c))}"


def ensure_environment() -> None:
    os.environ.setdefault("SMSIMDIR", str(REPO_ROOT))
    lib_dir = REPO_ROOT / "build/lib"
    old_ld = os.environ.get("LD_LIBRARY_PATH", "")
    if str(lib_dir) not in old_ld.split(":"):
        os.environ["LD_LIBRARY_PATH"] = f"{old_ld}:{lib_dir}" if old_ld else str(lib_dir)


def write_tree_input(root_path: Path, px_mev_c: float) -> None:
    root_path.parent.mkdir(parents=True, exist_ok=True)
    out = ROOT.TFile(str(root_path), "RECREATE")
    if out.IsZombie():
        raise RuntimeError(f"cannot create ROOT input: {root_path}")

    beam_array = ROOT.std.vector("TBeamSimData")()
    ROOT.gBeamSimDataArray = beam_array
    tree = ROOT.TTree("tree", f"neutron px={px_mev_c:g} MeV/c sign illustration")
    tree.Branch("TBeamSimData", ROOT.gBeamSimDataArray)

    py_mev_c = 0.0
    energy_mev = math.sqrt(px_mev_c * px_mev_c + py_mev_c * py_mev_c + PZ_MEV_C * PZ_MEV_C + NEUTRON_MASS_MEV * NEUTRON_MASS_MEV)
    momentum = ROOT.TLorentzVector(px_mev_c, py_mev_c, PZ_MEV_C, energy_mev)
    position = ROOT.TVector3(0.0, 0.0, 0.0)
    beam = ROOT.TBeamSimData(0, 1, momentum, position)
    beam.fParticleName = "neutron"
    beam.fPrimaryParticleID = 0
    beam.fTime = 0.0
    beam.fIsAccepted = True
    beam_array.clear()
    beam_array.push_back(beam)
    tree.Fill()
    tree.Write()
    out.Close()
    ROOT.gBeamSimDataArray = ROOT.nullptr


def write_geant4_macro(macro_path: Path, input_root: Path, run_name: str, geom_macro: Path, g4output_dir: Path) -> None:
    macro_path.parent.mkdir(parents=True, exist_ok=True)
    g4output_dir.mkdir(parents=True, exist_ok=True)
    macro_path.write_text(
        "\n".join(
            [
                "/vis/open VRML2FILE",
                "/tracking/storeTrajectory 1",
                "/action/file/OverWrite y",
                f"/action/file/SaveDirectory {g4output_dir}",
                f"/action/file/RunName {run_name}",
                f"/control/execute {geom_macro}",
                "/samurai/geometry/Target/SetTarget true",
                "/samurai/geometry/Update",
                "/vis/drawVolume",
                "/vis/viewer/set/upVector 1 0 0",
                "/vis/viewer/set/viewpointThetaPhi 90. 90.",
                "/vis/scene/add/axes 0 0 0 6000 mm",
                "/vis/scene/add/trajectories smooth",
                "/vis/modeling/trajectories/create/drawByCharge",
                "/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true",
                "/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 3",
                "/vis/modeling/trajectories/drawByCharge-0/set 0 blue",
                "/vis/scene/endOfEventAction accumulate",
                "/vis/viewer/set/autoRefresh false",
                "/action/gun/Type Tree",
                f"/action/gun/tree/InputFileName {input_root}",
                "/action/gun/tree/TreeName tree",
                "/action/gun/tree/beamOn 1",
                "/vis/viewer/set/autoRefresh true",
                "/vis/viewer/refresh",
                "/vis/viewer/update",
                "/vis/viewer/flush",
                "exit",
                "",
            ]
        ),
        encoding="utf-8",
    )


def cleanup_geant4_scratch(sim_workdir: Path) -> None:
    for path in sim_workdir.glob("g4_*.wrl"):
        path.unlink(missing_ok=True)
    (sim_workdir / "detector_geometry.gdml").unlink(missing_ok=True)


def run_geant4(sim_bin: Path, macro_path: Path, wrl_path: Path, log_path: Path, sim_workdir: Path) -> None:
    cleanup_geant4_scratch(sim_workdir)
    wrl_path.parent.mkdir(parents=True, exist_ok=True)
    wrl_path.unlink(missing_ok=True)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["G4VRMLFILE_FILE_NAME"] = str(wrl_path)
    with log_path.open("w", encoding="utf-8") as log:
        completed = subprocess.run(
            [str(sim_bin), str(macro_path)],
            cwd=sim_workdir,
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=False,
        )
    if completed.returncode != 0:
        raise RuntimeError(f"Geant4 export failed for {macro_path}; see {log_path}")
    if not wrl_path.exists():
        candidates = sorted(sim_workdir.glob("g4_*.wrl"), key=lambda p: p.stat().st_mtime, reverse=True)
        if candidates:
            shutil.move(str(candidates[0]), str(wrl_path))
    cleanup_geant4_scratch(sim_workdir)
    if not wrl_path.exists():
        raise RuntimeError(f"missing WRL output: {wrl_path}; see {log_path}")


def parse_floats(line: str) -> list[float]:
    return [float(value) for value in FLOAT_PATTERN.findall(line)]


def read_wrl_shapes(wrl_path: Path) -> tuple[list[tuple[str, tuple[float, float, float], list[tuple[float, float, float]]]], list[tuple[tuple[float, float, float], list[tuple[float, float, float]]]]]:
    solids: list[tuple[str, tuple[float, float, float], list[tuple[float, float, float]]]] = []
    polylines: list[tuple[tuple[float, float, float], list[tuple[float, float, float]]]] = []
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
                if points:
                    if current_name.startswith("POLYLINE"):
                        polylines.append((current_color, points))
                    elif current_name.startswith("SOLID"):
                        solids.append((current_name, current_color, points))
                continue
            values = parse_floats(stripped)
            if len(values) >= 3:
                points.append((values[0], values[1], values[2]))
    return solids, polylines


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


def primary_trajectory(polylines: list[tuple[tuple[float, float, float], list[tuple[float, float, float]]]]) -> list[tuple[float, float, float]]:
    if not polylines:
        return []
    target_x = -12.488
    target_z = -1069.46

    def score(item: tuple[tuple[float, float, float], list[tuple[float, float, float]]]) -> tuple[float, float]:
        _, points = item
        start = points[0]
        distance = math.hypot(start[0] - target_x, start[2] - target_z)
        z_span = max(point[2] for point in points) - min(point[2] for point in points)
        return (distance, -z_span)

    return min(polylines, key=score)[1]


def convert_wrl_to_png(wrl_path: Path, png_path: Path, title: str) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Polygon

    solids, polylines = read_wrl_shapes(wrl_path)
    primary = primary_trajectory(polylines)

    fig, ax = plt.subplots(figsize=(10.5, 6.3), dpi=180)
    ax.set_facecolor("white")

    for name, color, points in solids:
        if "Axes" in name or len(points) < 3:
            continue
        projected = [(point[2], point[0]) for point in points]
        hull = convex_hull(projected)
        if len(hull) < 3:
            continue
        patch = Polygon(
            hull,
            closed=True,
            facecolor=clamp_color(color),
            edgecolor=clamp_color(color),
            linewidth=0.35,
            alpha=0.10,
        )
        ax.add_patch(patch)

    for color, points in polylines:
        if len(points) < 2:
            continue
        z_values = [point[2] for point in points]
        x_values = [point[0] for point in points]
        is_primary = points is primary
        ax.plot(
            z_values,
            x_values,
            color=clamp_color(color) if not is_primary else "#d62728",
            linewidth=3.0 if is_primary else 1.0,
            alpha=0.95 if is_primary else 0.30,
            solid_capstyle="round",
            zorder=8 if is_primary else 5,
        )

    ax.scatter([-1069.46], [-12.488], s=38, c="#111111", marker="o", zorder=10)
    ax.text(-1030.0, 50.0, "target", fontsize=8, color="#111111")
    ax.annotate("+lab Z", xy=(-100.0, -2350.0), xytext=(-900.0, -2350.0), arrowprops={"arrowstyle": "->", "lw": 1.2}, fontsize=9)
    ax.annotate("+lab X", xy=(-900.0, -1700.0), xytext=(-900.0, -2350.0), arrowprops={"arrowstyle": "->", "lw": 1.2}, fontsize=9)

    if primary:
        start = primary[0]
        end = max(primary, key=lambda point: point[2])
        ax.text(
            0.60,
            0.08,
            f"primary neutron: X_target start={start[0]:.1f} mm, X at last step={end[0]:.1f} mm",
            transform=ax.transAxes,
            fontsize=8,
            color="#d62728",
            ha="left",
            va="bottom",
        )

    ax.set_title(title, fontsize=12, pad=10)
    ax.set_xlabel("lab Z downstream [mm]")
    ax.set_ylabel("lab X [mm]")
    ax.set_xlim(-1400.0, 9300.0)
    ax.set_ylim(-2600.0, 1400.0)
    ax.set_aspect("equal", adjustable="box")
    ax.grid(True, color="#d0d0d0", linewidth=0.45, alpha=0.55)
    ax.text(
        0.02,
        0.98,
        "X-Z projection of Geant4 VRML output; target-frame input py=0, pz=627 MeV/c; lab direction uses -3 deg Y rotation",
        transform=ax.transAxes,
        fontsize=8,
        va="top",
        ha="left",
        color="#222222",
    )
    fig.tight_layout()
    png_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(png_path)
    plt.close(fig)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--sim-bin", type=Path, default=DEFAULT_SIM_BIN)
    parser.add_argument("--geom-macro", type=Path, default=DEFAULT_GEOM_MACRO)
    parser.add_argument("--sim-workdir", type=Path, default=DEFAULT_SIM_WORKDIR)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    ensure_environment()
    if not args.sim_bin.exists():
        raise FileNotFoundError(f"missing sim_deuteron binary: {args.sim_bin}")
    if not args.geom_macro.exists():
        raise FileNotFoundError(f"missing geometry macro: {args.geom_macro}")

    input_dir = args.out_dir / "inputs"
    macro_dir = args.out_dir / "macros"
    wrl_dir = args.out_dir / "wrl"
    log_dir = args.out_dir / "logs"
    png_dir = args.out_dir

    for px in PX_VALUES:
        label = label_for_px(px)
        root_path = input_dir / f"{label}.root"
        macro_path = macro_dir / f"{label}.mac"
        wrl_path = wrl_dir / f"{label}.wrl"
        log_path = log_dir / f"{label}.log"
        png_path = png_dir / f"{label}.png"
        write_tree_input(root_path, px)
        write_geant4_macro(macro_path, root_path, label, args.geom_macro, log_dir / "g4output")
        run_geant4(args.sim_bin, macro_path, wrl_path, log_path, args.sim_workdir)
        title = f"neutron target-frame px = {px:+.0f} MeV/c"
        convert_wrl_to_png(wrl_path, png_path, title)

    print(f"px-sign Geant4 figures saved to {args.out_dir}")


if __name__ == "__main__":
    main()
