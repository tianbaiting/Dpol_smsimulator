"""Tests for geom_acceptance: NEBULA geometric acceptance ray-cast."""
import math
from pathlib import Path
import pytest

from geom_acceptance import (
    load_nebula_bars,
    ray_hits_bar,
    geom_acceptance,
    NEBULA_FRONT_Z_MM,
)

REPO = Path(__file__).resolve().parents[3]
DETECTORS_CSV = REPO / "configs/simulation/geometry/NEBULA_Detectors_Dayone.csv"
NEBULA_CSV    = REPO / "configs/simulation/geometry/NEBULA_Dayone.csv"


def test_load_nebula_bars_counts():
    bars = load_nebula_bars(DETECTORS_CSV, NEBULA_CSV, neut_only=True)
    # 60 bars per layer × 2 layers = 120 Neut bars
    assert len(bars) == 120
    # bar half-size should be (60, 900, 60) mm
    b = bars[0]
    assert b.half_x == 60.0
    assert b.half_y == 900.0
    assert b.half_z == 60.0


def test_load_includes_global_position_offset():
    bars = load_nebula_bars(DETECTORS_CSV, NEBULA_CSV, neut_only=True)
    # Global NEBULA position from NEBULA_Dayone.csv is z=7249.72.
    # Layer-1, SubLayer-1 bars have local z=0, so global z should be ~7249.72.
    # (Layer-1 also has SubLayer-2 at local z=130, i.e. global z=7379.72.)
    sublayer1 = [b for b in bars if abs(b.center_z - 7249.72) < 1.0]
    assert len(sublayer1) == 30


def test_central_pencil_hits_bar():
    # px=0,py=0,pz=627: straight-ahead neutron from target (0,0,0).
    # Must hit a NEBULA bar (one of the central 60).
    assert geom_acceptance(0.0, 0.0, 627.0, DETECTORS_CSV, NEBULA_CSV) is True


def test_far_off_axis_misses():
    # px=1000 MeV/c, pz=627, py=0 -> x at NEBULA_z ~= 7250*1000/627 ~= 11.6 m.
    # NEBULA half-width along X is ~1962 mm; this clearly misses.
    assert geom_acceptance(1000.0, 0.0, 627.0, DETECTORS_CSV, NEBULA_CSV) is False


def test_just_inside_x_edge_hits():
    # NEBULA covers roughly x in [-1962, +1962] mm. Pick px such that x at z=7250 is +1500 mm.
    # px/pz = 1500/7250 -> px = 627 * 1500 / 7250 ~= 129.8 MeV/c.
    assert geom_acceptance(129.8, 0.0, 627.0, DETECTORS_CSV, NEBULA_CSV) is True


def test_just_outside_y_edge_misses():
    # NEBULA bar half-height is 900 mm. py such that y at z=7250 = 1500 mm.
    # py/pz = 1500/7250 -> py = 129.8 MeV/c. y=1500 > 900 -> miss.
    assert geom_acceptance(0.0, 129.8, 627.0, DETECTORS_CSV, NEBULA_CSV) is False


def test_negative_pz_returns_false():
    # Backward-going neutron should not hit (NEBULA is downstream of target).
    assert geom_acceptance(0.0, 0.0, -627.0, DETECTORS_CSV, NEBULA_CSV) is False


def test_ray_hits_bar_local_aabb():
    # Direct AABB test against a bar at x=0, y=0, z=7250, half=(60,900,60).
    from geom_acceptance import Bar
    b = Bar(center_x=0.0, center_y=0.0, center_z=7250.0,
            half_x=60.0, half_y=900.0, half_z=60.0)
    # Ray from origin along +z must enter through front face (z = 7190).
    assert ray_hits_bar(0.0, 0.0, 0.0, 0.0, 0.0, 1.0, b)
    # Ray with positive py direction so y at z=7190 is 100 mm -> within ±900.
    dy = 100.0 / 7190.0
    n = math.sqrt(1 + dy*dy)
    assert ray_hits_bar(0.0, 0.0, 0.0, 0.0, dy/n, 1.0/n, b)
