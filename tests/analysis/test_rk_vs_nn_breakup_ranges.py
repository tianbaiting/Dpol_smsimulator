import numpy as np
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.analysis.rk_vs_nn_breakup.make_figures import (
    robust_axis_range,
    robust_upper_limit,
)


def test_robust_axis_range_ignores_sparse_outliers():
    central = np.linspace(-300.0, 300.0, 1001)
    values = np.concatenate([central, np.array([-4000.0, 5000.0])])

    assert robust_axis_range([values], quantiles=(1.0, 99.0), pad_fraction=0.0, step=50.0) == (-300.0, 300.0)


def test_robust_axis_range_applies_physical_floor_after_padding():
    central = np.linspace(50.0, 900.0, 1001)
    values = np.concatenate([central, np.array([5000.0])])

    assert robust_axis_range([values], quantiles=(0.0, 99.0), pad_fraction=0.05, step=100.0, lower_bound=0.0) == (0.0, 1000.0)


def test_robust_upper_limit_uses_central_tail_not_maximum():
    central = np.linspace(0.0, 280.0, 1001)
    values = np.concatenate([central, np.array([3500.0])])

    assert robust_upper_limit([values], high_quantile=99.0, pad_fraction=0.0, step=50.0) == 300.0


def test_robust_axis_range_can_be_symmetric_about_zero():
    values = np.linspace(-260.0, 210.0, 1001)

    assert robust_axis_range([values], quantiles=(0.0, 100.0), pad_fraction=0.0, step=50.0, symmetric_zero=True) == (-300.0, 300.0)
