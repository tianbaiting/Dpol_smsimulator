"""NEBULA geometric acceptance: ray-cast from target position to NEBULA bars.

Pure Python; reads bar geometry from the two CSVs that the Geant4 sim also reads,
so this module is the single source of truth for the ε_geom layer of Part B.

Coordinate conventions
----------------------
- Lab frame: standard Geant4/SAMURAI frame.
- Target frame: rotated by `target_angle_deg` about the Y axis relative to lab.
  The rotation matrix R_y(θ) transforms target-frame momentum to lab-frame via:
      px_lab =  cos(θ) * px_tgt + sin(θ) * pz_tgt
      py_lab =  py_tgt
      pz_lab = -sin(θ) * px_tgt + cos(θ) * pz_tgt
  (target's z-axis tilts toward +X in lab, toward the proton-arm side.)
- Default mac values: target_pos = (-12.488, 0.009, -1069.458) mm, θ = 3°.
"""
from __future__ import annotations
import math
from dataclasses import dataclass
from pathlib import Path
from typing import List


NEBULA_FRONT_Z_MM = 7189.72  # approximate; bars are at z=7249.72 with half_z=60


@dataclass(frozen=True)
class Bar:
    center_x: float
    center_y: float
    center_z: float
    half_x: float
    half_y: float
    half_z: float


def _parse_nebula_global(nebula_csv: Path) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    """Return (global_position_xyz, neut_size_xyz) in mm."""
    pos = (0.0, 0.0, 0.0)
    neut_size = (120.0, 1800.0, 120.0)
    for line in nebula_csv.read_text().splitlines():
        s = line.strip()
        if not s or s.startswith("//"):
            continue
        parts = [p.strip().rstrip(",") for p in s.split(",")]
        key = parts[0]
        if key == "Position":
            pos = (float(parts[1]), float(parts[2]), float(parts[3]))
        elif key == "NeutSize":
            neut_size = (float(parts[1]), float(parts[2]), float(parts[3]))
    return pos, neut_size


def load_nebula_bars(detectors_csv: Path, nebula_csv: Path, neut_only: bool = True) -> List[Bar]:
    """Load all NEBULA bars from the two CSV config files.

    Args:
        detectors_csv: Path to NEBULA_Detectors_Dayone.csv (bar local positions).
        nebula_csv: Path to NEBULA_Dayone.csv (global position + bar sizes).
        neut_only: If True, skip Veto bars and return only Neut bars.

    Returns:
        List of Bar dataclasses with global (center_x, center_y, center_z) and half-sizes.
    """
    pos, neut_size = _parse_nebula_global(nebula_csv)
    # half-sizes: NeutSize gives full dimensions
    half = (neut_size[0] * 0.5, neut_size[1] * 0.5, neut_size[2] * 0.5)
    bars: List[Bar] = []
    for i, line in enumerate(detectors_csv.read_text().splitlines()):
        if i == 0 or not line.strip():
            continue
        # Strip leading/trailing "!" (seen as "!0" prefix on last Veto line) then split on comma
        parts = [p.strip().strip("!") for p in line.split(",")]
        if len(parts) < 7:
            continue
        det_type = parts[1].strip()
        if neut_only and det_type != "Neut":
            continue
        bx = float(parts[4])
        by = float(parts[5])
        bz = float(parts[6])
        bars.append(Bar(
            center_x=bx + pos[0],
            center_y=by + pos[1],
            center_z=bz + pos[2],
            half_x=half[0],
            half_y=half[1],
            half_z=half[2],
        ))
    return bars


def ray_hits_bar(ox: float, oy: float, oz: float,
                 dx: float, dy: float, dz: float,
                 bar: Bar) -> bool:
    """Slab-method AABB ray intersection.

    Checks whether the ray from origin (ox, oy, oz) with direction (dx, dy, dz)
    intersects the axis-aligned bounding box of *bar*. Direction need not be
    unit-length; only the ratio of components matters.

    Returns True if the ray intersects the bar at t >= 0 (i.e., in front of origin).
    """
    t_min = -float("inf")
    t_max = float("inf")
    for o, d, c, h in (
        (ox, dx, bar.center_x, bar.half_x),
        (oy, dy, bar.center_y, bar.half_y),
        (oz, dz, bar.center_z, bar.half_z),
    ):
        if abs(d) < 1e-12:
            # Ray is parallel to slab; check if origin is inside
            if o < c - h or o > c + h:
                return False
            continue
        t1 = (c - h - o) / d
        t2 = (c + h - o) / d
        if t1 > t2:
            t1, t2 = t2, t1
        t_min = max(t_min, t1)
        t_max = min(t_max, t2)
        if t_min > t_max:
            return False
    # Intersection must be at non-negative t (in front of origin)
    return t_max >= max(t_min, 0.0)


def geom_acceptance(px_tgt: float, py_tgt: float, pz_tgt: float,
                    detectors_csv: Path, nebula_csv: Path,
                    target_pos: tuple[float, float, float] = (0.0, 0.0, 0.0),
                    target_angle_deg: float = 0.0,
                    bars_cache: dict | None = None) -> bool:
    """Return True iff a neutron emitted from target_pos (lab mm) with
    target-frame momentum (px_tgt, py_tgt, pz_tgt) enters any NEBULA Neut bar.

    The target frame is rotated by `target_angle_deg` about the Y axis from lab:
        px_lab =  cos(θ) * px_tgt + sin(θ) * pz_tgt
        py_lab =  py_tgt
        pz_lab = -sin(θ) * px_tgt + cos(θ) * pz_tgt
    pz_tgt must be > 0 for a forward-going neutron (in target frame).

    Backward compatibility: calling geom_acceptance(px, py, pz, det_csv, neb_csv)
    without the new keyword args defaults to target_pos=(0,0,0) and angle=0°,
    which is equivalent to the old lab-frame behaviour.

    Args:
        px_tgt, py_tgt, pz_tgt: Neutron momentum components in target frame
                                 (any consistent unit; MeV/c typical).
        detectors_csv: Path to NEBULA_Detectors_Dayone.csv.
        nebula_csv: Path to NEBULA_Dayone.csv.
        target_pos: Ray origin in lab-frame mm (default: (0,0,0)).
        target_angle_deg: Rotation of target frame about Y axis [degrees]
                          (default: 0.0, i.e. target frame == lab frame).
        bars_cache: Optional dict to cache loaded bar lists between calls.
                    Key is (str(detectors_csv), str(nebula_csv)).

    Returns:
        True if the ray hits at least one Neut bar, False otherwise.
    """
    if pz_tgt <= 0:
        return False

    # [EN] Rotate target-frame momentum to lab-frame via R_y(target_angle_deg).
    # [CN] 将靶系动量通过 R_y(target_angle_deg) 旋转到实验室系。
    if target_angle_deg != 0.0:
        theta = math.radians(target_angle_deg)
        ct, st = math.cos(theta), math.sin(theta)
        px_lab =  ct * px_tgt + st * pz_tgt
        py_lab =  py_tgt
        pz_lab = -st * px_tgt + ct * pz_tgt
    else:
        px_lab, py_lab, pz_lab = px_tgt, py_tgt, pz_tgt

    key = (str(detectors_csv), str(nebula_csv))
    if bars_cache is None or key not in bars_cache:
        bars = load_nebula_bars(detectors_csv, nebula_csv, neut_only=True)
        if bars_cache is not None:
            bars_cache[key] = bars
    else:
        bars = bars_cache[key]

    ox, oy, oz = target_pos
    for b in bars:
        if ray_hits_bar(ox, oy, oz, px_lab, py_lab, pz_lab, b):
            return True
    return False
