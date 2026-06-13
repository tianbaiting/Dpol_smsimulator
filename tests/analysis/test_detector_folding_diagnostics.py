#!/usr/bin/env python3
from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
import pandas as pd


REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.reconstruction.nn_target_momentum.plot_detector_folding_diagnostics import (  # noqa: E402
    compute_efficiency_1d,
    compute_efficiency_2d,
    compute_r_ladder,
    compute_response_matrix,
    compute_sign_migration,
    event_masks,
)


def make_frame():
    return pd.DataFrame(
        {
            "truth_pxn": [-50.0, -10.0, 10.0, 50.0, 80.0],
            "truth_rot_pxp": [-80.0, 10.0, 80.0, 100.0, 120.0],
            "truth_rot_pxn": [0.0, 20.0, 20.0, 40.0, 20.0],
            "reco_plane_nn_pxp": [-70.0, 20.0, 90.0, 80.0, 130.0],
            "reco_plane_reco_pxn": [0.0, 30.0, 20.0, 40.0, 30.0],
            "reco_pxn": [-40.0, -5.0, 5.0, 70.0, np.nan],
            "reco_pyn": [1.0, 1.0, 1.0, 1.0, np.nan],
            "n_reco_neutrons": [1, 1, 1, 1, 0],
        }
    )


def test_event_masks_apply_truth_and_reco_fiducials_separately():
    masks = event_masks(make_frame(), px_limit=60.0)

    assert masks["truth_fiducial"].tolist() == [True, True, True, True, False]
    assert masks["hit"].tolist() == [True, True, True, True, False]
    assert masks["reco_fiducial"].tolist() == [True, True, True, False, False]


def test_compute_efficiency_1d_counts_denominator_and_numerator():
    df = make_frame()
    masks = event_masks(df, px_limit=60.0)

    out = compute_efficiency_1d(
        df,
        x_col="truth_pxn",
        denominator_mask=masks["truth_fiducial"],
        numerator_mask=masks["reco_fiducial"],
        bins=np.array([-60.0, 0.0, 60.0]),
    )

    assert out["den"].tolist() == [2, 2]
    assert out["num"].tolist() == [2, 1]
    assert out["eff"].tolist() == [1.0, 0.5]
    assert out["err"].iloc[0] == 0.0
    assert math.isclose(out["err"].iloc[1], math.sqrt(0.5 * 0.5 / 2.0))


def test_compute_efficiency_2d_returns_binwise_efficiency():
    df = make_frame()
    masks = event_masks(df, px_limit=60.0)

    den, num, eff = compute_efficiency_2d(
        df,
        x_col="truth_pxn",
        y_col="truth_dpx",
        denominator_mask=masks["truth_fiducial"],
        numerator_mask=masks["reco_fiducial"],
        x_bins=np.array([-60.0, 0.0, 60.0]),
        y_bins=np.array([-100.0, 0.0, 100.0]),
    )

    assert den.shape == (2, 2)
    assert num.shape == (2, 2)
    assert eff.shape == (2, 2)
    assert den.sum() == 4
    assert num.sum() == 3
    assert eff[0, 0] == 1.0
    assert eff[1, 1] == 0.5


def test_compute_response_matrix_column_normalizes_truth_bins():
    df = make_frame()
    masks = event_masks(df, px_limit=60.0)

    counts, prob = compute_response_matrix(
        df,
        truth_col="truth_dpx",
        reco_col="reco_plane_dpx",
        mask=masks["reco_fiducial"],
        truth_bins=np.array([-100.0, 0.0, 100.0]),
        reco_bins=np.array([-100.0, 0.0, 100.0]),
    )

    assert counts.tolist() == [[2.0, 0.0], [0.0, 1.0]]
    assert prob.tolist() == [[1.0, 0.0], [0.0, 1.0]]


def test_compute_sign_migration_reports_probabilities_given_truth_sign():
    df = make_frame()
    masks = event_masks(df, px_limit=60.0)

    table = compute_sign_migration(df, masks["reco_fiducial"])

    neg = table[table["truth_sign"] == "negative"].iloc[0]
    pos = table[table["truth_sign"] == "positive"].iloc[0]
    assert neg["N_reco_negative"] == 2
    assert neg["N_reco_positive"] == 0
    assert neg["P_reco_negative"] == 1.0
    assert pos["N_reco_negative"] == 0
    assert pos["N_reco_positive"] == 1
    assert pos["P_reco_positive"] == 1.0


def test_compute_r_ladder_separates_truth_hit_and_reco_stages():
    df = make_frame()
    masks = event_masks(df, px_limit=60.0)

    table = compute_r_ladder(df, masks)

    truth = table[table["stage"] == "truth"].iloc[0]
    reco = table[table["stage"] == "reco_plane"].iloc[0]
    assert truth["n_neg"] == 2
    assert truth["n_pos"] == 2
    assert truth["R"] == 1.0
    assert reco["n_neg"] == 2
    assert reco["n_pos"] == 1
    assert reco["R"] == 0.5
