"""NEBULA geometric acceptance: ray-cast from target (0,0,0) to NEBULA bars.

Pure Python; reads bar geometry from the two CSVs that the Geant4 sim also reads,
so this module is the single source of truth for the ε_geom layer of Part B.
"""
from __future__ import annotations
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
        # Strip trailing "!" (seen on last Veto line) then split on comma
        parts = [p.strip().rstrip("!") for p in line.split(",")]
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


def geom_acceptance(px: float, py: float, pz: float,
                    detectors_csv: Path, nebula_csv: Path,
                    bars_cache: dict | None = None) -> bool:
    """Return True iff a neutron leaving target (0,0,0) with momentum (px,py,pz)
    enters any NEBULA Neut bar.

    The ray direction is (px, py, pz); pz must be > 0 for a forward-going neutron.
    The magnitude of momentum does not matter — only the direction is used.

    Args:
        px, py, pz: Neutron momentum components (any consistent unit; MeV/c typical).
        detectors_csv: Path to NEBULA_Detectors_Dayone.csv.
        nebula_csv: Path to NEBULA_Dayone.csv.
        bars_cache: Optional dict to cache loaded bar lists between calls.
                    Key is (str(detectors_csv), str(nebula_csv)).

    Returns:
        True if the ray hits at least one Neut bar, False otherwise.
    """
    if pz <= 0:
        return False
    key = (str(detectors_csv), str(nebula_csv))
    if bars_cache is None or key not in bars_cache:
        bars = load_nebula_bars(detectors_csv, nebula_csv, neut_only=True)
        if bars_cache is not None:
            bars_cache[key] = bars
    else:
        bars = bars_cache[key]
    for b in bars:
        if ray_hits_bar(0.0, 0.0, 0.0, px, py, pz, b):
            return True
    return False
