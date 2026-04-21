import sys
from pathlib import Path

import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compare_ablation import build_comparison


def _sample_summary(sigma_y_pdc2):
    return pd.DataFrame([{
        "truth_px": 0.0, "truth_py": 0.0, "N": 50,
        "sigma_x_pdc1_mm": 2.8, "sigma_y_pdc1_mm": 11.2,
        "sigma_x_pdc2_mm": 3.5, "sigma_y_pdc2_mm": sigma_y_pdc2,
        "sigma_theta_x_mrad": 3.5, "sigma_theta_y_mrad": 8.0,
    }])


def test_comparison_has_both_conditions():
    baseline = _sample_summary(15.2)
    noair    = _sample_summary(12.0)
    out = build_comparison(baseline, noair)
    assert len(out) == 2
    assert set(out["condition"]) == {"baseline", "no_air"}


def test_comparison_delta_column_present():
    baseline = _sample_summary(15.2)
    noair    = _sample_summary(12.0)
    out = build_comparison(baseline, noair)
    baseline_row = out[out["condition"] == "baseline"].iloc[0]
    assert baseline_row["delta_sigma_y_pdc2_mm"] == pytest.approx(15.2 - 12.0)


def test_comparison_multiple_truth_points():
    b1 = _sample_summary(15.2).assign(truth_px=0.0,   truth_py=0.0)
    b2 = _sample_summary(12.3).assign(truth_px=100.0, truth_py=0.0)
    baseline = pd.concat([b1, b2], ignore_index=True)

    n1 = _sample_summary(11.0).assign(truth_px=0.0,   truth_py=0.0)
    n2 = _sample_summary(9.0).assign(truth_px=100.0,  truth_py=0.0)
    noair = pd.concat([n1, n2], ignore_index=True)

    out = build_comparison(baseline, noair)
    assert len(out) == 4  # 2 truth × 2 conditions
    d1 = out[(out["truth_px"] == 0.0) & (out["condition"] == "baseline")].iloc[0]
    d2 = out[(out["truth_px"] == 100.0) & (out["condition"] == "baseline")].iloc[0]
    assert d1["delta_sigma_y_pdc2_mm"] == pytest.approx(15.2 - 11.0)
    assert d2["delta_sigma_y_pdc2_mm"] == pytest.approx(12.3 - 9.0)


def test_noair_row_has_no_delta():
    baseline = _sample_summary(15.2)
    noair    = _sample_summary(12.0)
    out = build_comparison(baseline, noair)
    noair_row = out[out["condition"] == "no_air"].iloc[0]
    assert pd.isna(noair_row["delta_sigma_y_pdc2_mm"])
