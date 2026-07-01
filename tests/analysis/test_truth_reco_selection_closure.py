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

from scripts.reconstruction.nn_target_momentum.compare_truth_reco_selection_rx import (  # noqa: E402
    add_reco_selection_columns,
    add_truth_reco_observable_columns,
    add_truth_selection_columns,
    compute_mode_ratio,
    compute_selection_migration_row,
    mode_values,
)


def _base_truth_reco_frame() -> pd.DataFrame:
    return pd.DataFrame(
        {
            "truth_pxp": [-80.0, 300.0, 20.0],
            "truth_pyp": [0.0, 0.0, 120.0],
            "truth_pxn": [-80.0, 300.0, 20.0],
            "truth_pyn": [10.0, 10.0, -120.0],
            "nn_pxp": [-80.0, 300.0, 20.0],
            "nn_pyp": [0.0, 0.0, 120.0],
            "reco_pxn": [-80.0, 300.0, 20.0],
            "reco_pyn": [10.0, 10.0, -120.0],
            "nn_status": [0, 0, 0],
            "n_reco_neutrons": [1, 1, 1],
        }
    )


def test_truth_cut_only_reads_truth_columns():
    df = _base_truth_reco_frame()
    changed_reco = df.copy()
    changed_reco[["nn_pxp", "nn_pyp", "reco_pxn", "reco_pyn"]] = [1.0, 2.0, 3.0, 4.0]

    base_mask = add_truth_selection_columns(df)["truth_tight"].tolist()
    reco_changed_mask = add_truth_selection_columns(changed_reco)["truth_tight"].tolist()

    changed_truth = df.copy()
    changed_truth.loc[0, "truth_pxp"] = 0.0
    changed_truth.loc[0, "truth_pxn"] = 0.0
    truth_changed_mask = add_truth_selection_columns(changed_truth)["truth_tight"].tolist()

    assert base_mask == [True, False, False]
    assert reco_changed_mask == base_mask
    assert truth_changed_mask != base_mask


def test_reco_cut_only_reads_reco_columns():
    df = _base_truth_reco_frame()
    changed_truth = df.copy()
    changed_truth[["truth_pxp", "truth_pyp", "truth_pxn", "truth_pyn"]] = [1.0, 2.0, 3.0, 4.0]

    base_mask = add_reco_selection_columns(df)["reco_tight"].tolist()
    truth_changed_mask = add_reco_selection_columns(changed_truth)["reco_tight"].tolist()

    changed_reco = df.copy()
    changed_reco.loc[0, "reco_pxn"] = 0.0
    changed_reco.loc[0, "nn_pxp"] = 0.0
    reco_changed_mask = add_reco_selection_columns(changed_reco)["reco_tight"].tolist()

    assert base_mask == [True, False, False]
    assert truth_changed_mask == base_mask
    assert reco_changed_mask != base_mask


def test_changing_reco_momentum_does_not_change_truth_mask():
    df = _base_truth_reco_frame()
    changed_reco = df.copy()
    changed_reco[["nn_pxp", "nn_pyp", "reco_pxn", "reco_pyn"]] = [-999.0, 999.0, np.nan, np.nan]

    base_mask = add_truth_selection_columns(df)["truth_tight"].tolist()
    changed_mask = add_truth_selection_columns(changed_reco)["truth_tight"].tolist()

    assert changed_mask == base_mask


def test_reco_px60_requires_valid_reconstructed_neutron():
    df = pd.DataFrame(
        {
            "nn_pxp": [1.0, 1.0, 1.0, 1.0, 1.0],
            "nn_pyp": [0.0, 0.0, 0.0, 0.0, 0.0],
            "reco_pxn": [59.9, 59.9, np.nan, 59.9, 60.0],
            "reco_pyn": [0.0, 0.0, 0.0, np.nan, 0.0],
            "nn_status": [0, 0, 0, 0, 0],
            "n_reco_neutrons": [1, 0, 1, 1, 1],
        }
    )

    out = add_reco_selection_columns(df, px_limit=60.0)

    assert out["reco_px60"].tolist() == [True, False, False, False, False]


def test_reco_event_plane_uses_reconstructed_proton_and_neutron_sums():
    df = pd.DataFrame(
        {
            "truth_pxp": [1.0],
            "truth_pyp": [0.0],
            "truth_pxn": [1.0],
            "truth_pyn": [0.0],
            "nn_pxp": [0.0],
            "nn_pyp": [2.0],
            "reco_pxn": [0.0],
            "reco_pyn": [3.0],
        }
    )

    out = add_truth_reco_observable_columns(df)

    assert math.isclose(out["phi_reco"].iloc[0], math.pi / 2.0, rel_tol=0.0, abs_tol=1e-12)
    assert math.isclose(out["reco_rot_pxp"].iloc[0], 2.0, rel_tol=0.0, abs_tol=1e-12)
    assert math.isclose(out["reco_rot_pxn"].iloc[0], 3.0, rel_tol=0.0, abs_tol=1e-12)
    assert math.isclose(out["reco_dpx"].iloc[0], -1.0, rel_tol=0.0, abs_tol=1e-12)


def test_four_modes_use_correct_selection_and_observable_combinations():
    df = pd.DataFrame(
        {
            "truth_tight": [True, True, False, False],
            "reco_tight": [True, False, True, False],
            "truth_px60": [True, False, True, False],
            "reco_px60": [False, True, True, False],
            "truth_dpx": [1.0, -1.0, 2.0, -2.0],
            "reco_dpx": [-1.0, 3.0, -3.0, 4.0],
        }
    )

    assert mode_values(df, "truth_cut_truth_obs", "tight").tolist() == [1.0]
    assert mode_values(df, "truth_cut_reco_obs", "tight").tolist() == [3.0]
    assert mode_values(df, "reco_cut_truth_obs", "tight").tolist() == [1.0, 2.0]
    assert mode_values(df, "reco_cut_reco_obs", "tight").tolist() == [-3.0]


def test_selection_efficiency_purity_and_jaccard_match_toy_counts():
    df = pd.DataFrame(
        {
            "truth_tight": [True, True, False, False, True],
            "reco_tight": [True, False, True, False, False],
            "truth_px60": [True, True, True, True, True],
            "reco_px60": [True, True, True, True, True],
            "truth_loose": [True, True, True, False, True],
            "truth_mid": [True, True, False, False, True],
            "reco_loose": [True, True, True, False, False],
            "reco_mid": [True, False, True, False, False],
        }
    )

    row = compute_selection_migration_row(df, target="Sn124E190", pol="ypol", gamma="g050")

    assert row["N_parent"] == 5
    assert row["N_truth_selected"] == 3
    assert row["N_reco_selected"] == 2
    assert row["N_both"] == 1
    assert row["N_truth_only"] == 2
    assert row["N_reco_only"] == 1
    assert row["selection_efficiency"] == 1.0 / 3.0
    assert row["selection_purity"] == 1.0 / 2.0
    assert row["jaccard_overlap"] == 1.0 / 4.0
    assert row["truth_loss_fraction"] == 2.0 / 3.0
    assert row["reco_contamination_fraction"] == 1.0 / 2.0


def test_truth_cut_reco_obs_ratio_reproduces_fixed_sample_reco_plane_result():
    df = pd.DataFrame(
        {
            "truth_tight": [True, True, True, False, True],
            "reco_tight": [False, False, False, False, False],
            "truth_px60": [True, True, True, True, True],
            "reco_px60": [True, True, True, True, False],
            "truth_dpx": [-100.0, -100.0, -100.0, -100.0, -100.0],
            "reco_dpx": [10.0, 20.0, -30.0, 40.0, -50.0],
        }
    )

    row = compute_mode_ratio(
        df,
        mode="truth_cut_reco_obs",
        cut="tight",
        n_boot=0,
        target="Sn124E190",
        pol="ypol",
        gamma="g050",
    )

    assert row["selection_observable_mode"] == "truth_cut_reco_obs"
    assert row["N"] == 3
    assert row["n_pos"] == 2
    assert row["n_neg"] == 1
    assert row["R"] == 2.0
