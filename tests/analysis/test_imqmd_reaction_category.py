#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.analysis.analyze_imqmd_reaction_categories import (  # noqa: E402
    count_reaction_types,
    parse_ypol_reactiontype_path,
    parse_zpol_dbreak_path,
    summarize_reaction_type_file,
    summarize_zpol_dbreak_file,
)


def test_parse_ypol_reactiontype_path_extracts_dataset_metadata():
    path = Path(
        "data/qmdrawdata/qmdrawdata/20260413ypol/d+Sn124E190/"
        "d+Sn124E190g065ypn-RP360/reactiontype.dat"
    )

    key = parse_ypol_reactiontype_path(path)

    assert key.target == "Sn124"
    assert key.energy_mev_u == 190
    assert key.gamma_label == "065"
    assert key.gamma_value == 0.65
    assert key.pol == "ypn"


def test_count_reaction_types_uses_fourth_column_labels(tmp_path):
    raw = tmp_path / "reactiontype.dat"
    raw.write_text(
        "1 12.1 196.0 dkeep\n"
        "2 8.2 120.0 break\n"
        "3 5.5 15.0 other\n"
        "4 3.7 162.0 spall\n"
        "5 8.6 158.0 break\n",
        encoding="utf-8",
    )

    counts = count_reaction_types(raw)

    assert counts == {"break": 2, "dkeep": 1, "other": 1, "spall": 1}


def test_summarize_reaction_type_file_reports_total_and_fractions(tmp_path):
    dataset = tmp_path / "d+Sn112E190" / "d+Sn112E190g080ynp-RP360"
    dataset.mkdir(parents=True)
    raw = dataset / "reactiontype.dat"
    raw.write_text(
        "1 1.0 10.0 break\n"
        "2 2.0 20.0 break\n"
        "3 3.0 30.0 dkeep\n"
        "4 4.0 40.0 spall\n",
        encoding="utf-8",
    )

    row = summarize_reaction_type_file(raw)

    assert row.total_events == 4
    assert row.break_count == 2
    assert row.dkeep_count == 1
    assert row.spall_count == 1
    assert row.other_count == 0
    assert row.break_fraction == 0.5
    assert row.dkeep_fraction == 0.25
    assert row.spall_fraction == 0.25


def test_summarize_zpol_dbreak_file_reports_header_probability(tmp_path):
    dataset = tmp_path / "d+Sn112E190g080znp"
    dataset.mkdir()
    raw = dataset / "dbreakb07.dat"
    raw.write_text(
        "znp d+112Sn   190.00MeV/u b= 7.000fm  10000events\n"
        "  No.  pxp(MeV/c)  pyp(MeV/c)  pzp(MeV/c)  pxn(MeV/c)  pyn(MeV/c)  pzn(MeV/c)\n"
        "  1176 15.0 61.2 456.8 -176.9 34.6 88.1\n"
        "  1657 -248.7 -5.0 276.3 -20.7 -41.2 90.2\n"
        "  1682 -54.8 141.7 389.6 -34.2 -84.8 307.7\n",
        encoding="utf-8",
    )

    key = parse_zpol_dbreak_path(raw)
    row = summarize_zpol_dbreak_file(raw)

    assert key.target == "Sn112"
    assert key.gamma_label == "080"
    assert key.pol == "znp"
    assert key.b_index == 7
    assert row.b_fm == 7.0
    assert row.header_events == 10000
    assert row.breakup_rows == 3
    assert row.breakup_probability == 0.0003
