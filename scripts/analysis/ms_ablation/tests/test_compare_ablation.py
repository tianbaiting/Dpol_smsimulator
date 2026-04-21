import sys
from pathlib import Path

import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compare_ablation import build_comparison


def _sample_summary(sigma_y_pdc2, truth_px=0.0, truth_py=0.0):
    return pd.DataFrame([{
        "truth_px": truth_px, "truth_py": truth_py, "N": 50,
        "sigma_x_pdc1_mm": 2.8, "sigma_y_pdc1_mm": 11.2,
        "sigma_x_pdc2_mm": 3.5, "sigma_y_pdc2_mm": sigma_y_pdc2,
        "sigma_theta_x_mrad": 3.5, "sigma_theta_y_mrad": 8.0,
    }])


def test_three_way_all_conditions_present():
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    assert len(out) == 3
    assert set(out["condition"]) == {"all_air", "beamline_vacuum", "all_vacuum"}


def test_three_way_delta_upstream_air():
    """delta_sigma_y_pdc2_mm_upstream_air = all_air - beamline_vacuum."""
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    row = out[out["condition"] == "all_air"].iloc[0]
    assert row["delta_sigma_y_pdc2_mm_upstream_air"] == pytest.approx(15.2 - 10.5)


def test_three_way_delta_downstream_air():
    """delta_sigma_y_pdc2_mm_downstream_air = beamline_vacuum - all_vacuum."""
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    row = out[out["condition"] == "beamline_vacuum"].iloc[0]
    assert row["delta_sigma_y_pdc2_mm_downstream_air"] == pytest.approx(10.5 - 1.0)


def test_three_way_delta_total_air():
    """delta_sigma_y_pdc2_mm_total_air = all_air - all_vacuum."""
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    row = out[out["condition"] == "all_air"].iloc[0]
    assert row["delta_sigma_y_pdc2_mm_total_air"] == pytest.approx(15.2 - 1.0)


def test_three_way_all_vacuum_has_no_delta():
    """all_vacuum is the reference; its delta columns must be NaN."""
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    row = out[out["condition"] == "all_vacuum"].iloc[0]
    assert pd.isna(row["delta_sigma_y_pdc2_mm_upstream_air"])
    assert pd.isna(row["delta_sigma_y_pdc2_mm_downstream_air"])
    assert pd.isna(row["delta_sigma_y_pdc2_mm_total_air"])


def test_three_way_multiple_truth_points():
    a1 = _sample_summary(15.2, truth_px=0.0,   truth_py=0.0)
    a2 = _sample_summary(12.3, truth_px=100.0, truth_py=0.0)
    all_air = pd.concat([a1, a2], ignore_index=True)

    m1 = _sample_summary(10.5, truth_px=0.0,   truth_py=0.0)
    m2 = _sample_summary(8.1,  truth_px=100.0, truth_py=0.0)
    mixed = pd.concat([m1, m2], ignore_index=True)

    v1 = _sample_summary(1.0, truth_px=0.0,   truth_py=0.0)
    v2 = _sample_summary(0.9, truth_px=100.0, truth_py=0.0)
    all_vac = pd.concat([v1, v2], ignore_index=True)

    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    assert len(out) == 6  # 2 truth × 3 conditions

    # tp (0,0) upstream-air delta on all_air row
    r = out[(out["truth_px"] == 0.0) & (out["condition"] == "all_air")].iloc[0]
    assert r["delta_sigma_y_pdc2_mm_upstream_air"] == pytest.approx(15.2 - 10.5)
    # tp (100,0) downstream-air delta on beamline_vacuum row
    r = out[(out["truth_px"] == 100.0) & (out["condition"] == "beamline_vacuum")].iloc[0]
    assert r["delta_sigma_y_pdc2_mm_downstream_air"] == pytest.approx(8.1 - 0.9)
