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

from scripts.reconstruction.nn_target_momentum.plot_nebulaplus_reco import (  # noqa: E402
    add_observable_columns,
    build_pxn_sign_efficiency_table,
    build_reco_plane_dpx,
    build_variant_dpx,
    compute_cut_masks,
    compute_pxn_sign_efficiency,
    fiducial_px60_mask,
    fiducial_px100_mask,
    ratio_with_bootstrap,
    reco_pxn_px60_mask,
    reco_pxn_px100_mask,
    rotate_to_reaction_plane,
)


def test_rotate_to_reaction_plane_aligns_truth_sum_to_positive_x():
    rot_pxp, rot_pyp, rot_pxn, rot_pyn, phi = rotate_to_reaction_plane(
        np.array([0.0]),
        np.array([1.0]),
        np.array([0.0]),
        np.array([1.0]),
    )

    assert math.isclose(phi[0], math.pi / 2.0, rel_tol=0.0, abs_tol=1e-12)
    assert math.isclose(rot_pxp[0] + rot_pxn[0], 2.0, rel_tol=0.0, abs_tol=1e-12)
    assert math.isclose(rot_pyp[0] + rot_pyn[0], 0.0, rel_tol=0.0, abs_tol=1e-12)


def test_ypol_cut_masks_match_report_thresholds():
    df = pd.DataFrame(
        {
            "truth_pxp": [-80.0, 300.0, 20.0],
            "truth_pyp": [0.0, 0.0, 120.0],
            "truth_pxn": [-80.0, 300.0, 20.0],
            "truth_pyn": [10.0, 10.0, -120.0],
            "truth_pzp": [620.0, 620.0, 620.0],
            "truth_pzn": [100.0, 100.0, 100.0],
        }
    )

    masks = compute_cut_masks(df, "ypol")

    assert masks["loose"].tolist() == [True, True, False]
    assert masks["mid"].tolist() == [True, False, False]
    assert masks["tight"].tolist() == [True, False, False]


def test_zpol_cut_masks_match_report_thresholds():
    df = pd.DataFrame(
        {
            "truth_pxp": [-80.0, 300.0, 20.0],
            "truth_pyp": [0.0, 0.0, 0.0],
            "truth_pxn": [-80.0, 300.0, 20.0],
            "truth_pyn": [10.0, 10.0, 0.0],
            "truth_pzp": [620.0, 620.0, 500.0],
            "truth_pzn": [540.0, 540.0, 500.0],
        }
    )

    masks = compute_cut_masks(df, "zpol")

    assert masks["loose"].tolist() == [True, True, False]
    assert masks["mid"].tolist() == [True, False, False]
    assert masks["tight"].tolist() == [True, False, False]


def test_fiducial_px100_mask_uses_strict_boundary():
    df = pd.DataFrame({"truth_pxn": [-100.0, -99.9, 0.0, 99.9, 100.0]})

    mask = fiducial_px100_mask(df)

    assert mask.tolist() == [False, True, True, True, False]


def test_fiducial_px60_mask_uses_strict_boundary():
    df = pd.DataFrame({"truth_pxn": [-60.0, -59.9, 0.0, 59.9, 60.0]})

    mask = fiducial_px60_mask(df)

    assert mask.tolist() == [False, True, True, True, False]


def test_reco_pxn_px100_mask_requires_reconstructed_neutron():
    df = pd.DataFrame(
        {
            "reco_pxn": [-100.0, -99.9, 0.0, 99.9, 100.0, 50.0],
            "n_reco_neutrons": [1, 1, 1, 1, 1, 0],
        }
    )

    mask = reco_pxn_px100_mask(df)

    assert mask.tolist() == [False, True, True, True, False, False]


def test_reco_pxn_px60_mask_requires_reconstructed_neutron():
    df = pd.DataFrame(
        {
            "reco_pxn": [-60.0, -59.9, 0.0, 59.9, 60.0, 50.0],
            "reco_pyn": [0.0, 0.0, 0.0, 0.0, 0.0, np.nan],
            "n_reco_neutrons": [1, 1, 1, 1, 1, 1],
        }
    )

    mask = reco_pxn_px60_mask(df)

    assert mask.tolist() == [False, True, True, True, False, False]


def test_add_observable_columns_builds_reco_reaction_plane():
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

    out = add_observable_columns(df)

    assert math.isclose(out["phi_reco_event"].iloc[0], math.pi / 2.0, rel_tol=0.0, abs_tol=1e-12)
    assert math.isclose(out["reco_plane_nn_pxp"].iloc[0], 2.0, rel_tol=0.0, abs_tol=1e-12)
    assert math.isclose(out["reco_plane_reco_pxn"].iloc[0], 3.0, rel_tol=0.0, abs_tol=1e-12)
    assert math.isclose(out["reco_plane_nn_pyp"].iloc[0], 0.0, rel_tol=0.0, abs_tol=1e-12)
    assert math.isclose(out["reco_plane_reco_pyn"].iloc[0], 0.0, rel_tol=0.0, abs_tol=1e-12)


def test_ratio_with_bootstrap_counts_positive_over_negative():
    result = ratio_with_bootstrap(np.array([-2.0, -1.0, 0.0, 3.0, 4.0]), n_boot=0)

    assert result.r == 1.0
    assert result.n_pos == 2
    assert result.n_neg == 2
    assert result.n_used == 4
    assert math.isnan(result.r_lo)
    assert math.isnan(result.r_hi)


def test_ratio_with_bootstrap_returns_nan_for_empty_sign_sample():
    result = ratio_with_bootstrap(np.array([0.0, np.nan]), n_boot=0)

    assert math.isnan(result.r)
    assert result.n_pos == 0
    assert result.n_neg == 0
    assert result.n_used == 0


def test_build_variant_dpx_uses_truth_neutron_fallback_for_mixed_only():
    df = pd.DataFrame(
        {
            "truth_rot_pxp": [5.0, 5.0],
            "truth_rot_pxn": [1.0, 1.0],
            "nn_rot_pxp": [8.0, 8.0],
            "reco_rot_pxn": [2.0, 20.0],
            "n_reco_neutrons": [1, 0],
        }
    )

    variants = build_variant_dpx(df)

    assert variants["truth"].tolist() == [4.0, 4.0]
    assert variants["mixed"].tolist() == [6.0, 7.0]
    assert variants["full"].tolist() == [6.0]


def test_build_reco_plane_dpx_uses_separate_truth_and_reco_masks():
    df = pd.DataFrame(
        {
            "truth_rot_pxp": [10.0, 20.0],
            "truth_rot_pxn": [1.0, 2.0],
            "reco_plane_nn_pxp": [100.0, 200.0],
            "reco_plane_reco_pxn": [10.0, 20.0],
        },
        index=[10, 20],
    )
    truth_mask = pd.Series([True, False], index=df.index, dtype=bool)
    reco_mask = pd.Series([False, True], index=df.index, dtype=bool)

    variants = build_reco_plane_dpx(df, truth_mask, reco_mask)

    assert variants["truth"].tolist() == [9.0]
    assert variants["full_reco"].tolist() == [180.0]


def test_compute_pxn_sign_efficiency_reports_positive_negative_splits():
    df = pd.DataFrame(
        {
            "truth_pxn": [-70.0, -20.0, -10.0, 20.0, 30.0, 70.0],
            "reco_pxn": [1.0, 1.0, np.nan, 1.0, np.nan, 1.0],
            "n_reco_neutrons": [1, 1, 1, 1, 0, 1],
        }
    )
    mask = fiducial_px100_mask(df)

    row = compute_pxn_sign_efficiency(df, mask, pol="ypol", cut="loose", fiducial="px100")

    assert row["pol"] == "ypol"
    assert row["cut"] == "loose"
    assert row["fiducial"] == "px100"
    assert row["N"] == 6
    assert row["N_pos"] == 3
    assert row["N_neg"] == 3
    assert row["N_hit_pos"] == 2
    assert row["N_hit_neg"] == 2
    assert row["eps_pos"] == 2.0 / 3.0
    assert row["eps_neg"] == 2.0 / 3.0
    assert row["rel_eff_diff"] == 0.0


def test_build_pxn_sign_efficiency_table_aligns_group_local_masks():
    df = pd.DataFrame(
        {
            "target": pd.Categorical(["Sn112", "Sn112", "Sn124", "Sn124"]),
            "gamma": pd.Categorical(["g050", "g050", "g050", "g050"]),
            "loose": [True, True, True, True],
            "mid": [True, True, False, False],
            "tight": [False, False, True, True],
            "truth_pxn": [-10.0, 10.0, -20.0, 20.0],
            "reco_pxn": [1.0, np.nan, np.nan, 1.0],
            "n_reco_neutrons": [1, 0, 0, 1],
        },
        index=[10, 20, 30, 40],
    )
    fiducial_masks = {
        "baseline": pd.Series(True, index=df.index, dtype=bool),
        "px100": fiducial_px100_mask(df),
    }

    table = build_pxn_sign_efficiency_table(df, "ypol", fiducial_masks)

    sn112_loose = table[
        (table["target"] == "Sn112")
        & (table["gamma"] == "g050")
        & (table["cut"] == "loose")
        & (table["fiducial"] == "px100")
    ].iloc[0]
    assert sn112_loose["N_pos"] == 1
    assert sn112_loose["N_neg"] == 1
    assert sn112_loose["N_hit_pos"] == 0
    assert sn112_loose["N_hit_neg"] == 1
