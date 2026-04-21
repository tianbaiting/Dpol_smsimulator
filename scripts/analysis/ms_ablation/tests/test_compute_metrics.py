import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compute_metrics import compute_dispersion_summary


def _make_hits(truth_px, truth_py, xs1, ys1, xs2, ys2, z1=4000.0, z2=5000.0):
    return pd.DataFrame({
        "truth_px": [truth_px] * len(xs1),
        "truth_py": [truth_py] * len(xs1),
        "pdc1_x": xs1,
        "pdc1_y": ys1,
        "pdc2_x": xs2,
        "pdc2_y": ys2,
        "pdc1_z": [z1] * len(xs1),
        "pdc2_z": [z2] * len(xs1),
    })


def test_robust_half_width_matches_percentile_definition():
    xs = np.arange(100, dtype=float)
    hits = _make_hits(0, 0, xs, np.zeros(100), xs, np.zeros(100))
    out = compute_dispersion_summary(hits, [(0, 0)])
    assert len(out) == 1
    expected = (np.percentile(xs, 84) - np.percentile(xs, 16)) / 2
    assert out.iloc[0]["sigma_x_pdc1_mm"] == pytest.approx(expected, rel=1e-6)


def test_zero_dispersion():
    hits = _make_hits(0, 0, np.full(10, 5.0), np.full(10, -3.0),
                            np.full(10, 5.0), np.full(10, -3.0))
    out = compute_dispersion_summary(hits, [(0, 0)])
    row = out.iloc[0]
    assert row["sigma_x_pdc1_mm"] == pytest.approx(0.0)
    assert row["sigma_y_pdc1_mm"] == pytest.approx(0.0)
    assert row["sigma_theta_x_mrad"] == pytest.approx(0.0)
    assert row["sigma_theta_y_mrad"] == pytest.approx(0.0)


def test_theta_computation():
    xs1 = np.zeros(100)
    ys1 = np.zeros(100)
    xs2 = np.linspace(500, 600, 100)
    ys2 = np.zeros(100)
    hits = _make_hits(0, 0, xs1, ys1, xs2, ys2)
    out = compute_dispersion_summary(hits, [(0, 0)])
    expected_mrad = (np.percentile(xs2, 84) - np.percentile(xs2, 16)) / 2 / 1000.0 * 1000.0
    assert out.iloc[0]["sigma_theta_x_mrad"] == pytest.approx(expected_mrad, rel=1e-6)


def test_groups_by_truth_point():
    df1 = _make_hits(0, 0, np.arange(50), np.zeros(50), np.arange(50), np.zeros(50))
    df2 = _make_hits(100, 0, np.zeros(50), np.zeros(50), np.zeros(50), np.zeros(50))
    hits = pd.concat([df1, df2], ignore_index=True)
    out = compute_dispersion_summary(hits, [(0, 0), (100, 0)])
    assert len(out) == 2
    assert set(out["truth_px"]) == {0, 100}
    assert out.loc[out["truth_px"] == 100, "sigma_x_pdc1_mm"].iloc[0] == pytest.approx(0.0)


def test_missing_truth_point_yields_nan_row():
    hits = _make_hits(0, 0, np.arange(10), np.zeros(10), np.arange(10), np.zeros(10))
    out = compute_dispersion_summary(hits, [(0, 0), (100, 0)])
    assert len(out) == 2
    missing = out[out["truth_px"] == 100].iloc[0]
    assert missing["N"] == 0
    assert pd.isna(missing["sigma_x_pdc1_mm"])


def test_truth_float_tolerance():
    hits = _make_hits(0.0 + 1e-9, 0.0, np.arange(10), np.zeros(10),
                      np.arange(10), np.zeros(10))
    out = compute_dispersion_summary(hits, [(0, 0)])
    assert out.iloc[0]["N"] == 10
