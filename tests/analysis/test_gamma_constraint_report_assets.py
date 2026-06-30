#!/usr/bin/env python3
from __future__ import annotations

import math
import sys
from pathlib import Path

import pandas as pd


REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.reconstruction.nn_target_momentum.make_gamma_constraint_report_assets import (  # noqa: E402
    r_vs_gamma_table,
    separation_table,
)


def test_r_vs_gamma_table_keeps_sn112_and_sn124_separate():
    cell_passrates = pd.DataFrame(
        [
            {"pol": "ypol", "target": "Sn112E190", "gamma": "g050", "N_raw": 1000},
            {"pol": "ypol", "target": "Sn124E190", "gamma": "g050", "N_raw": 2000},
            {"pol": "zpol", "target": "Sn112E190", "gamma": "g050", "N_raw": 3000},
            {"pol": "zpol", "target": "Sn124E190", "gamma": "g050", "N_raw": 4000},
        ]
    )
    r_ladder_by_cell = pd.DataFrame(
        [
            {
                "pol": "ypol",
                "target": "Sn112E190",
                "gamma": "g050",
                "cut": "tight",
                "px_limit": 60.0,
                "stage": "reco_plane",
                "N": 100,
                "n_pos": 75,
                "n_neg": 25,
                "R": 3.0,
            },
            {
                "pol": "ypol",
                "target": "Sn124E190",
                "gamma": "g050",
                "cut": "tight",
                "px_limit": 60.0,
                "stage": "reco_plane",
                "N": 400,
                "n_pos": 320,
                "n_neg": 80,
                "R": 4.0,
            },
            {
                "pol": "zpol",
                "target": "Sn112E190",
                "gamma": "g050",
                "cut": "tight",
                "px_limit": 60.0,
                "stage": "reco_plane",
                "N": 300,
                "n_pos": 150,
                "n_neg": 150,
                "R": 1.0,
            },
            {
                "pol": "zpol",
                "target": "Sn124E190",
                "gamma": "g050",
                "cut": "tight",
                "px_limit": 60.0,
                "stage": "reco_plane",
                "N": 800,
                "n_pos": 200,
                "n_neg": 600,
                "R": 1.0 / 3.0,
            },
        ]
    )

    table = r_vs_gamma_table(
        {"cell_passrates": cell_passrates, "r_ladder_by_cell": r_ladder_by_cell},
        targets=("Sn112E190", "Sn124E190"),
    )

    assert set(table["target"]) == {"Sn112E190", "Sn124E190"}
    assert len(table) == 4

    sn112_ypol = table[(table["target"] == "Sn112E190") & (table["pol"] == "ypol")].iloc[0]
    sn124_ypol = table[(table["target"] == "Sn124E190") & (table["pol"] == "ypol")].iloc[0]
    assert math.isclose(sn112_ypol["useful_fraction"], 0.1)
    assert math.isclose(sn124_ypol["useful_fraction"], 0.2)
    assert sn112_ypol["expected_N_15mm_16h"] != sn124_ypol["expected_N_15mm_16h"]


def test_separation_table_is_target_resolved():
    r_table = pd.DataFrame(
        [
            {"target": "Sn112E190", "pol": "ypol", "gamma": "g050", "gamma_value": 0.5, "stage": "reco_plane", "R": 3.0, "expected_N_15mm_16h": 100.0},
            {"target": "Sn112E190", "pol": "ypol", "gamma": "g060", "gamma_value": 0.6, "stage": "reco_plane", "R": 2.0, "expected_N_15mm_16h": 100.0},
            {"target": "Sn124E190", "pol": "ypol", "gamma": "g050", "gamma_value": 0.5, "stage": "reco_plane", "R": 5.0, "expected_N_15mm_16h": 200.0},
            {"target": "Sn124E190", "pol": "ypol", "gamma": "g060", "gamma_value": 0.6, "stage": "reco_plane", "R": 4.0, "expected_N_15mm_16h": 200.0},
        ]
    )

    table = separation_table(r_table, targets=("Sn112E190", "Sn124E190"))

    assert table[["target", "pol", "left_gamma", "right_gamma"]].to_dict("records") == [
        {"target": "Sn112E190", "pol": "ypol", "left_gamma": "g050", "right_gamma": "g060"},
        {"target": "Sn124E190", "pol": "ypol", "left_gamma": "g050", "right_gamma": "g060"},
    ]
