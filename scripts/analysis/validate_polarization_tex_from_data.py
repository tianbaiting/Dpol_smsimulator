#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
import re
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import ROOT


RAW_HEADER_RE = re.compile(
    r"b=\s*(?P<b>[0-9]+(?:\.[0-9]+)?)fm\s+(?P<events>[0-9]+)events"
)
GENERATOR_WRITE_RE = re.compile(
    r"Wrote\s+(?P<written>[0-9]+)\s+elastic events to\s+(?P<output>.+?)\s+"
    r"\(raw_lines=(?P<raw_lines>[0-9]+),\s+parsed=(?P<parsed>[0-9]+),\s+"
    r"selected=(?P<selected>[0-9]+),\s+cut_removed=(?P<cut_removed>[0-9]+),\s+"
    r"rotated=(?P<rotated>[0-9]+)\)"
)


@dataclass(frozen=True)
class FileAuditRow:
    pol_suffix: str
    target_dir: str
    b_index: int
    b_fm: float
    header_events: int
    raw_breakup_rows: int
    theoretical_preselect_events: int
    generator_selected_events: int
    generator_cut_removed_events: int
    generator_written_events: int
    g4input_entries: int
    inferred_cut_removed_events: int
    g4output_entries: int
    pdc_hits: int
    nebula_hits: int
    coincidence_hits: int
    pdc_efficiency: float
    nebula_efficiency: float
    coincidence_efficiency: float
    archive_entries: int
    archive_pdc_hits: int
    archive_nebula_hits: int
    archive_coincidence_hits: int
    archive_coincidence_efficiency: float
    fresh_minus_archive_coincidence_efficiency: float
    archive_available: bool
    raw_dat_path: str
    g4input_root_path: str
    g4output_root_path: str
    archive_root_path: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate polarization.tex breakup efficiency using existing ImQMD data and fresh Geant4 outputs."
    )
    parser.add_argument("--dataset-id", default="sn124_zpol_g060_bweighted")
    parser.add_argument("--raw-base", type=Path, required=True)
    parser.add_argument("--g4input-base", type=Path, required=True)
    parser.add_argument("--g4output-base", type=Path, required=True)
    parser.add_argument("--archive-base", type=Path, required=True)
    parser.add_argument("--generator-log", type=Path, required=True)
    parser.add_argument("--result-dir", type=Path, required=True)
    parser.add_argument("--geometry-macro", type=Path, required=True)
    parser.add_argument("--target-prefix", default="d+Sn124E190g060")
    parser.add_argument("--bmax-fm", type=float, default=10.0)
    parser.add_argument("--headline-bmax-fm", type=float, default=9.0)
    return parser.parse_args()


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    if not rows:
        raise ValueError(f"no rows for {path}")
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def root_open(path: Path) -> Any:
    root_file = ROOT.TFile.Open(str(path))
    if not root_file or root_file.IsZombie():
        raise RuntimeError(f"failed to open ROOT file: {path}")
    return root_file


def ensure_root_ready() -> None:
    ROOT.gROOT.SetBatch(True)
    load_status = ROOT.gSystem.Load("libsmdata.so")
    if load_status not in (0, 1):
        raise RuntimeError(f"failed to load libsmdata.so, status={load_status}")


def parse_raw_header(path: Path) -> tuple[float, int]:
    with path.open(encoding="utf-8", errors="replace") as handle:
        header = handle.readline().strip()
    match = RAW_HEADER_RE.search(header)
    if not match:
        raise RuntimeError(f"failed to parse raw header from {path}: {header}")
    return float(match.group("b")), int(match.group("events"))


def count_raw_breakup_rows(path: Path) -> int:
    count = 0
    with path.open(encoding="utf-8", errors="replace") as handle:
        next(handle, None)
        next(handle, None)
        for line in handle:
            if line.split():
                count += 1
    return count


def b_from_filename(path: Path) -> int:
    match = re.search(r"dbreakb([0-9]{2})", path.name)
    if not match:
        raise RuntimeError(f"failed to parse b index from {path}")
    return int(match.group(1))


def selected_events_by_b(raw_events: int, b_fm: float, bmax_fm: float) -> int:
    return int(math.floor(raw_events * b_fm / bmax_fm))


def parse_generator_log(path: Path) -> dict[str, dict[str, int]]:
    if not path.exists():
        return {}
    parsed: dict[str, dict[str, int]] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = GENERATOR_WRITE_RE.search(line)
        if not match:
            continue
        output_path = str(Path(match.group("output")).resolve())
        parsed[output_path] = {
            "written": int(match.group("written")),
            "raw_lines": int(match.group("raw_lines")),
            "parsed": int(match.group("parsed")),
            "selected": int(match.group("selected")),
            "cut_removed": int(match.group("cut_removed")),
            "rotated": int(match.group("rotated")),
        }
    return parsed


def root_tree_entries(path: Path) -> int:
    root_file = root_open(path)
    tree = root_file.Get("tree")
    if not tree:
        raise RuntimeError(f"failed to find tree in {path}")
    entries = int(tree.GetEntries())
    root_file.Close()
    return entries


def summarize_output_root(path: Path) -> dict[str, int]:
    root_file = root_open(path)
    tree = root_file.Get("tree")
    if not tree:
        raise RuntimeError(f"failed to find tree in {path}")

    has_frag = bool(tree.GetBranch("FragSimData"))
    has_nebula = bool(tree.GetBranch("NEBULAPla"))
    entries = int(tree.GetEntries())
    pdc_hits = 0
    nebula_hits = 0
    coincidence_hits = 0

    for index in range(entries):
        tree.GetEntry(index)
        frag_entries = 0
        nebula_entries = 0
        if has_frag:
            frag_entries = int(getattr(tree, "FragSimData").GetEntriesFast())
        if has_nebula:
            nebula_entries = int(getattr(tree, "NEBULAPla").GetEntriesFast())
        has_pdc = frag_entries > 0
        has_nebula_hit = nebula_entries > 0
        if has_pdc:
            pdc_hits += 1
        if has_nebula_hit:
            nebula_hits += 1
        if has_pdc and has_nebula_hit:
            coincidence_hits += 1

    root_file.Close()
    return {
        "entries": entries,
        "pdc_hits": pdc_hits,
        "nebula_hits": nebula_hits,
        "coincidence_hits": coincidence_hits,
    }


def efficiency(numerator: int, denominator: int) -> float:
    if denominator <= 0:
        return 0.0
    return numerator / denominator


def glob_single_output(base_dir: Path, stem: str) -> Path:
    matches = sorted(base_dir.glob(f"{stem}*.root"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one ROOT output for {stem} under {base_dir}, got {len(matches)}")
    return matches[0]


def build_aggregate_row(rows: list[FileAuditRow], scope_id: str, b_limit: int | None) -> dict[str, Any]:
    if b_limit is None:
        scoped = rows
    else:
        scoped = [row for row in rows if row.b_index <= b_limit]
    if not scoped:
        raise RuntimeError(f"no rows available for aggregate scope {scope_id}")

    header_events = sum(row.header_events for row in scoped)
    raw_breakup_rows = sum(row.raw_breakup_rows for row in scoped)
    theoretical_preselect_events = sum(row.theoretical_preselect_events for row in scoped)
    generator_selected_events = sum(row.generator_selected_events for row in scoped)
    generator_cut_removed_events = sum(row.generator_cut_removed_events for row in scoped)
    generator_written_events = sum(row.generator_written_events for row in scoped)
    g4input_entries = sum(row.g4input_entries for row in scoped)
    g4output_entries = sum(row.g4output_entries for row in scoped)
    pdc_hits = sum(row.pdc_hits for row in scoped)
    nebula_hits = sum(row.nebula_hits for row in scoped)
    coincidence_hits = sum(row.coincidence_hits for row in scoped)
    archive_entries = sum(row.archive_entries for row in scoped)
    archive_pdc_hits = sum(row.archive_pdc_hits for row in scoped)
    archive_nebula_hits = sum(row.archive_nebula_hits for row in scoped)
    archive_coincidence_hits = sum(row.archive_coincidence_hits for row in scoped)

    return {
        "scope_id": scope_id,
        "file_count": len(scoped),
        "header_events_total": header_events,
        "raw_breakup_rows_total": raw_breakup_rows,
        "raw_breakup_probability": efficiency(raw_breakup_rows, header_events),
        "theoretical_preselect_events_total": theoretical_preselect_events,
        "generator_selected_events_total": generator_selected_events,
        "generator_cut_removed_events_total": generator_cut_removed_events,
        "generator_written_events_total": generator_written_events,
        "g4input_entries_total": g4input_entries,
        "g4output_entries_total": g4output_entries,
        "pdc_hits_total": pdc_hits,
        "nebula_hits_total": nebula_hits,
        "coincidence_hits_total": coincidence_hits,
        "pdc_efficiency": efficiency(pdc_hits, g4output_entries),
        "nebula_efficiency": efficiency(nebula_hits, g4output_entries),
        "coincidence_efficiency": efficiency(coincidence_hits, g4output_entries),
        "archive_entries_total": archive_entries,
        "archive_pdc_hits_total": archive_pdc_hits,
        "archive_nebula_hits_total": archive_nebula_hits,
        "archive_coincidence_hits_total": archive_coincidence_hits,
        "archive_coincidence_efficiency": efficiency(archive_coincidence_hits, archive_entries),
        "fresh_minus_archive_coincidence_efficiency": (
            efficiency(coincidence_hits, g4output_entries)
            - efficiency(archive_coincidence_hits, archive_entries)
        ),
    }


def build_shell_cross_section_estimates(
    rows: list[FileAuditRow],
    scope_id: str,
    b_min: int,
    b_max: int,
    delta_b_fm: float = 1.0,
) -> dict[str, Any]:
    scoped = [row for row in rows if b_min <= row.b_index <= b_max]
    if not scoped:
        raise RuntimeError(f"no rows available for shell cross section scope {scope_id}")

    by_b: dict[int, dict[str, float]] = {}
    for row in scoped:
        bucket = by_b.setdefault(
            row.b_index,
            {
                "b_fm": row.b_fm,
                "header_events": 0.0,
                "raw_breakup_rows": 0.0,
                "coincidence_hits": 0.0,
                "g4output_entries": 0.0,
            },
        )
        bucket["header_events"] += row.header_events
        bucket["raw_breakup_rows"] += row.raw_breakup_rows
        bucket["coincidence_hits"] += row.coincidence_hits
        bucket["g4output_entries"] += row.g4output_entries

    per_b_rows: list[dict[str, Any]] = []
    sigma_breakup_fm2 = 0.0
    sigma_detected_fm2 = 0.0
    sigma_selected_input_fm2 = 0.0
    total_header_events = 0.0
    total_raw_breakup_rows = 0.0
    total_coincidence_hits = 0.0

    for b_index in sorted(by_b):
        bucket = by_b[b_index]
        shell_area_fm2 = 2.0 * math.pi * bucket["b_fm"] * delta_b_fm
        breakup_probability = efficiency(int(bucket["raw_breakup_rows"]), int(bucket["header_events"]))
        coincidence_probability = efficiency(int(bucket["coincidence_hits"]), int(bucket["header_events"]))
        selected_input_probability = efficiency(int(bucket["g4output_entries"]), int(bucket["header_events"]))
        sigma_breakup_fm2 += shell_area_fm2 * breakup_probability
        sigma_detected_fm2 += shell_area_fm2 * coincidence_probability
        sigma_selected_input_fm2 += shell_area_fm2 * selected_input_probability
        total_header_events += bucket["header_events"]
        total_raw_breakup_rows += bucket["raw_breakup_rows"]
        total_coincidence_hits += bucket["coincidence_hits"]
        per_b_rows.append(
            {
                "b_index": b_index,
                "b_fm": bucket["b_fm"],
                "shell_area_fm2": shell_area_fm2,
                "header_events": int(bucket["header_events"]),
                "raw_breakup_rows": int(bucket["raw_breakup_rows"]),
                "raw_breakup_probability": breakup_probability,
                "g4output_entries": int(bucket["g4output_entries"]),
                "selected_input_probability": selected_input_probability,
                "coincidence_hits": int(bucket["coincidence_hits"]),
                "coincidence_probability_vs_incident": coincidence_probability,
                "breakup_cross_section_contribution_mb": shell_area_fm2 * breakup_probability * 10.0,
                "selected_input_cross_section_contribution_mb": shell_area_fm2 * selected_input_probability * 10.0,
                "detected_coincidence_cross_section_contribution_mb": shell_area_fm2 * coincidence_probability * 10.0,
            }
        )

    aggregate_breakup_probability = efficiency(int(total_raw_breakup_rows), int(total_header_events))
    naive_area_fm2 = math.pi * (float(b_max) ** 2) * aggregate_breakup_probability

    return {
        "scope_id": scope_id,
        "b_min_center_fm": b_min,
        "b_max_center_fm": b_max,
        "delta_b_fm": delta_b_fm,
        "method_recommended": "sigma = sum(2*pi*b_i*delta_b*P_i), P_i = N_breakup_i / N_incident_i",
        "method_naive_for_comparison": "sigma = pi*b_max^2*(sum N_breakup_i / sum N_incident_i)",
        "header_events_total": int(total_header_events),
        "raw_breakup_rows_total": int(total_raw_breakup_rows),
        "aggregate_breakup_probability": aggregate_breakup_probability,
        "breakup_cross_section_shell_midpoint_fm2": sigma_breakup_fm2,
        "breakup_cross_section_shell_midpoint_mb": sigma_breakup_fm2 * 10.0,
        "selected_input_cross_section_shell_midpoint_mb": sigma_selected_input_fm2 * 10.0,
        "detected_coincidence_cross_section_shell_midpoint_mb": sigma_detected_fm2 * 10.0,
        "breakup_cross_section_naive_area_mb": naive_area_fm2 * 10.0,
        "naive_area_reference_bmax_fm": float(b_max),
        "shell_vs_naive_ratio": (
            (sigma_breakup_fm2 / naive_area_fm2)
            if naive_area_fm2 > 0.0 else 0.0
        ),
        "per_b_rows": per_b_rows,
    }


def build_pb_profile_rows(
    rows: list[FileAuditRow],
    b_min: int,
    b_max: int,
    delta_b_fm: float = 1.0,
    convergence_start_b: int = 5,
) -> list[dict[str, Any]]:
    scoped = [row for row in rows if b_min <= row.b_index <= b_max]
    if not scoped:
        raise RuntimeError(f"no rows available for P(b) profile in [{b_min}, {b_max}]")

    by_b: dict[int, dict[str, float]] = {}
    for row in scoped:
        bucket = by_b.setdefault(
            row.b_index,
            {
                "b_fm": row.b_fm,
                "header_events": 0.0,
                "raw_breakup_rows": 0.0,
                "g4output_entries": 0.0,
                "coincidence_hits": 0.0,
            },
        )
        bucket["header_events"] += row.header_events
        bucket["raw_breakup_rows"] += row.raw_breakup_rows
        bucket["g4output_entries"] += row.g4output_entries
        bucket["coincidence_hits"] += row.coincidence_hits

    cumulative_sigma_all_mb = 0.0
    cumulative_sigma_conv_mb = 0.0
    profile_rows: list[dict[str, Any]] = []
    for b_index in sorted(by_b):
        bucket = by_b[b_index]
        shell_area_fm2 = 2.0 * math.pi * bucket["b_fm"] * delta_b_fm
        breakup_probability = efficiency(int(bucket["raw_breakup_rows"]), int(bucket["header_events"]))
        selected_probability = efficiency(int(bucket["g4output_entries"]), int(bucket["header_events"]))
        coincidence_probability = efficiency(int(bucket["coincidence_hits"]), int(bucket["header_events"]))
        shell_breakup_mb = shell_area_fm2 * breakup_probability * 10.0
        shell_selected_mb = shell_area_fm2 * selected_probability * 10.0
        shell_detected_mb = shell_area_fm2 * coincidence_probability * 10.0
        cumulative_sigma_all_mb += shell_breakup_mb
        if b_index >= convergence_start_b:
            cumulative_sigma_conv_mb += shell_breakup_mb
        profile_rows.append(
            {
                "b_index": b_index,
                "b_center_fm": bucket["b_fm"],
                "delta_b_fm": delta_b_fm,
                "header_events": int(bucket["header_events"]),
                "raw_breakup_rows": int(bucket["raw_breakup_rows"]),
                "breakup_probability": breakup_probability,
                "g4output_entries": int(bucket["g4output_entries"]),
                "selected_input_probability": selected_probability,
                "coincidence_hits": int(bucket["coincidence_hits"]),
                "coincidence_probability_vs_incident": coincidence_probability,
                "shell_area_fm2": shell_area_fm2,
                "shell_breakup_cross_section_mb": shell_breakup_mb,
                "shell_selected_input_cross_section_mb": shell_selected_mb,
                "shell_detected_coincidence_cross_section_mb": shell_detected_mb,
                "cumulative_breakup_cross_section_mb_from_bmin": cumulative_sigma_all_mb,
                "cumulative_breakup_cross_section_mb_from_bconv": cumulative_sigma_conv_mb,
            }
        )
    return profile_rows


def write_pb_plot(path: Path, profile_rows: list[dict[str, Any]], convergence_start_b: int) -> None:
    ROOT.gROOT.SetBatch(True)
    ROOT.gStyle.SetOptStat(0)

    canvas = ROOT.TCanvas("c_pb_sigma", "ImQMD breakup probability and cumulative sigma", 900, 900)
    canvas.Divide(1, 2)

    b_values = [float(row["b_center_fm"]) for row in profile_rows]
    p_values = [float(row["breakup_probability"]) for row in profile_rows]
    cum_all_values = [float(row["cumulative_breakup_cross_section_mb_from_bmin"]) for row in profile_rows]
    cum_conv_values = [float(row["cumulative_breakup_cross_section_mb_from_bconv"]) for row in profile_rows]
    shell_values = [float(row["shell_breakup_cross_section_mb"]) for row in profile_rows]

    max_p = max(p_values) if p_values else 1.0
    max_sigma = max(max(cum_all_values, default=0.0), max(shell_values, default=0.0)) * 1.15
    max_sigma = max(max_sigma, 1.0)

    graph_p = ROOT.TGraph(len(profile_rows))
    graph_shell = ROOT.TGraph(len(profile_rows))
    graph_cum_all = ROOT.TGraph(len(profile_rows))
    graph_cum_conv = ROOT.TGraph(len(profile_rows))
    for idx, row in enumerate(profile_rows):
        b = float(row["b_center_fm"])
        graph_p.SetPoint(idx, b, float(row["breakup_probability"]))
        graph_shell.SetPoint(idx, b, float(row["shell_breakup_cross_section_mb"]))
        graph_cum_all.SetPoint(idx, b, float(row["cumulative_breakup_cross_section_mb_from_bmin"]))
        graph_cum_conv.SetPoint(idx, b, float(row["cumulative_breakup_cross_section_mb_from_bconv"]))

    graph_p.SetLineColor(ROOT.kBlue + 1)
    graph_p.SetMarkerColor(ROOT.kBlue + 1)
    graph_p.SetMarkerStyle(20)
    graph_p.SetMarkerSize(1.1)
    graph_p.SetLineWidth(2)

    graph_shell.SetLineColor(ROOT.kOrange + 7)
    graph_shell.SetMarkerColor(ROOT.kOrange + 7)
    graph_shell.SetMarkerStyle(24)
    graph_shell.SetMarkerSize(1.0)
    graph_shell.SetLineWidth(2)

    graph_cum_all.SetLineColor(ROOT.kRed + 1)
    graph_cum_all.SetMarkerColor(ROOT.kRed + 1)
    graph_cum_all.SetMarkerStyle(21)
    graph_cum_all.SetMarkerSize(1.0)
    graph_cum_all.SetLineWidth(2)

    graph_cum_conv.SetLineColor(ROOT.kGreen + 2)
    graph_cum_conv.SetMarkerColor(ROOT.kGreen + 2)
    graph_cum_conv.SetMarkerStyle(22)
    graph_cum_conv.SetMarkerSize(1.0)
    graph_cum_conv.SetLineWidth(2)

    pad1 = canvas.cd(1)
    pad1.SetGrid()
    frame1 = pad1.DrawFrame(min(b_values) - 0.5, 0.0, max(b_values) + 0.5, max_p)
    frame1.SetTitle("ImQMD elastic-breakup probability profile;impact parameter b [fm];P(b)")
    graph_p.Draw("LP SAME")

    line_conv = ROOT.TLine(float(convergence_start_b), 0.0, float(convergence_start_b), max_p)
    line_conv.SetLineStyle(2)
    line_conv.SetLineColor(ROOT.kGray + 2)
    line_conv.Draw("SAME")

    legend1 = ROOT.TLegend(0.14, 0.73, 0.52, 0.88)
    legend1.SetBorderSize(0)
    legend1.AddEntry(graph_p, "P(b) = N_breakup(b) / N_incident(b)", "lp")
    legend1.AddEntry(line_conv, f"reference start b = {convergence_start_b} fm", "l")
    legend1.Draw()

    pad2 = canvas.cd(2)
    pad2.SetGrid()
    frame2 = pad2.DrawFrame(min(b_values) - 0.5, 0.0, max(b_values) + 0.5, max_sigma)
    frame2.SetTitle("Shell contribution and cumulative breakup cross section;impact parameter b [fm];cross section [mb]")
    graph_shell.Draw("LP SAME")
    graph_cum_all.Draw("LP SAME")
    graph_cum_conv.Draw("LP SAME")

    line_conv2 = ROOT.TLine(float(convergence_start_b), 0.0, float(convergence_start_b), max_sigma)
    line_conv2.SetLineStyle(2)
    line_conv2.SetLineColor(ROOT.kGray + 2)
    line_conv2.Draw("SAME")

    legend2 = ROOT.TLegend(0.14, 0.62, 0.72, 0.88)
    legend2.SetBorderSize(0)
    legend2.AddEntry(graph_shell, "shell d#sigma_{bu} #approx 2#pib#Deltab P(b)", "lp")
    legend2.AddEntry(graph_cum_all, "cumulative #sigma_{bu} from smallest b", "lp")
    legend2.AddEntry(graph_cum_conv, f"cumulative #sigma_{{bu}} from b #geq {convergence_start_b} fm", "lp")
    legend2.Draw()

    canvas.SaveAs(str(path))


def main() -> None:
    ensure_root_ready()
    args = parse_args()
    args.result_dir.mkdir(parents=True, exist_ok=True)

    generator_log = parse_generator_log(args.generator_log)
    target_dirs = sorted(args.raw_base.glob(f"{args.target_prefix}z*"))
    if not target_dirs:
        raise RuntimeError(f"no raw target directories found under {args.raw_base} for {args.target_prefix}")

    rows: list[FileAuditRow] = []
    for target_dir in target_dirs:
        pol_suffix = target_dir.name[-3:]
        g4input_dir = args.g4input_base / target_dir.name
        g4output_dir = args.g4output_base / target_dir.name
        archive_dir = args.archive_base / target_dir.name
        for raw_dat in sorted(target_dir.glob("dbreakb*.dat")):
            b_index = b_from_filename(raw_dat)
            b_fm, header_events = parse_raw_header(raw_dat)
            raw_breakup_rows = count_raw_breakup_rows(raw_dat)
            theoretical_preselect = selected_events_by_b(header_events, b_fm, args.bmax_fm)

            g4input_root = g4input_dir / raw_dat.with_suffix(".root").name
            if not g4input_root.exists():
                raise RuntimeError(f"missing g4input ROOT file: {g4input_root}")
            g4input_entries = root_tree_entries(g4input_root)

            g4output_root = glob_single_output(g4output_dir, raw_dat.stem)
            fresh_stats = summarize_output_root(g4output_root)

            archive_root_path = ""
            archive_entries = 0
            archive_pdc_hits = 0
            archive_nebula_hits = 0
            archive_coincidence_hits = 0
            archive_eff = 0.0
            archive_available = False
            if archive_dir.exists():
                archive_matches = sorted(archive_dir.glob(f"{raw_dat.stem}*.root"))
                if len(archive_matches) == 1:
                    archive_root = archive_matches[0]
                    archive_stats = summarize_output_root(archive_root)
                    archive_root_path = str(archive_root.resolve())
                    archive_entries = archive_stats["entries"]
                    archive_pdc_hits = archive_stats["pdc_hits"]
                    archive_nebula_hits = archive_stats["nebula_hits"]
                    archive_coincidence_hits = archive_stats["coincidence_hits"]
                    archive_eff = efficiency(archive_coincidence_hits, archive_entries)
                    archive_available = True

            generator_info = generator_log.get(str(g4input_root.resolve()), {})
            generator_selected = int(generator_info.get("selected", theoretical_preselect))
            generator_cut_removed = int(
                generator_info.get("cut_removed", max(0, theoretical_preselect - g4input_entries))
            )
            generator_written = int(generator_info.get("written", g4input_entries))

            rows.append(
                FileAuditRow(
                    pol_suffix=pol_suffix,
                    target_dir=target_dir.name,
                    b_index=b_index,
                    b_fm=b_fm,
                    header_events=header_events,
                    raw_breakup_rows=raw_breakup_rows,
                    theoretical_preselect_events=theoretical_preselect,
                    generator_selected_events=generator_selected,
                    generator_cut_removed_events=generator_cut_removed,
                    generator_written_events=generator_written,
                    g4input_entries=g4input_entries,
                    inferred_cut_removed_events=max(0, theoretical_preselect - g4input_entries),
                    g4output_entries=fresh_stats["entries"],
                    pdc_hits=fresh_stats["pdc_hits"],
                    nebula_hits=fresh_stats["nebula_hits"],
                    coincidence_hits=fresh_stats["coincidence_hits"],
                    pdc_efficiency=efficiency(fresh_stats["pdc_hits"], fresh_stats["entries"]),
                    nebula_efficiency=efficiency(fresh_stats["nebula_hits"], fresh_stats["entries"]),
                    coincidence_efficiency=efficiency(
                        fresh_stats["coincidence_hits"], fresh_stats["entries"]
                    ),
                    archive_entries=archive_entries,
                    archive_pdc_hits=archive_pdc_hits,
                    archive_nebula_hits=archive_nebula_hits,
                    archive_coincidence_hits=archive_coincidence_hits,
                    archive_coincidence_efficiency=archive_eff,
                    fresh_minus_archive_coincidence_efficiency=(
                        efficiency(fresh_stats["coincidence_hits"], fresh_stats["entries"]) - archive_eff
                    ),
                    archive_available=archive_available,
                    raw_dat_path=str(raw_dat.resolve()),
                    g4input_root_path=str(g4input_root.resolve()),
                    g4output_root_path=str(g4output_root.resolve()),
                    archive_root_path=archive_root_path,
                )
            )

    aggregate_summary_rows = [
        build_aggregate_row(rows, "all_b", None),
        build_aggregate_row(rows, "b_le_9", int(args.headline_bmax_fm)),
    ]
    shell_cross_section_rows = [
        build_shell_cross_section_estimates(rows, "b1_to_b9_midpoint_shell", 1, int(args.headline_bmax_fm)),
        build_shell_cross_section_estimates(rows, "b1_to_b10_midpoint_shell", 1, int(args.bmax_fm)),
        build_shell_cross_section_estimates(rows, "b5_to_b9_midpoint_shell", 5, int(args.headline_bmax_fm)),
        build_shell_cross_section_estimates(rows, "b5_to_b10_midpoint_shell", 5, int(args.bmax_fm)),
    ]
    pb_profile_rows = build_pb_profile_rows(rows, 1, int(args.bmax_fm), 1.0, convergence_start_b=5)
    pb_plot_path = args.result_dir / "imqmd_breakup_probability_and_cumulative_sigma.png"

    generator_audit_rows = [
        {
            "pol_suffix": row.pol_suffix,
            "b_index": row.b_index,
            "b_fm": row.b_fm,
            "header_events": row.header_events,
            "raw_breakup_rows": row.raw_breakup_rows,
            "raw_breakup_probability": row.raw_breakup_rows / row.header_events,
            "theoretical_preselect_events": row.theoretical_preselect_events,
            "generator_selected_events": row.generator_selected_events,
            "generator_cut_removed_events": row.generator_cut_removed_events,
            "generator_written_events": row.generator_written_events,
            "g4input_entries": row.g4input_entries,
            "inferred_cut_removed_events": row.inferred_cut_removed_events,
            "raw_dat_path": row.raw_dat_path,
            "g4input_root_path": row.g4input_root_path,
        }
        for row in rows
    ]
    g4_validation_rows = [
        {
            "pol_suffix": row.pol_suffix,
            "b_index": row.b_index,
            "b_fm": row.b_fm,
            "g4output_entries": row.g4output_entries,
            "pdc_hits": row.pdc_hits,
            "nebula_hits": row.nebula_hits,
            "coincidence_hits": row.coincidence_hits,
            "pdc_efficiency": row.pdc_efficiency,
            "nebula_efficiency": row.nebula_efficiency,
            "coincidence_efficiency": row.coincidence_efficiency,
            "g4output_root_path": row.g4output_root_path,
        }
        for row in rows
    ]
    compare_rows = [
        {
            "pol_suffix": row.pol_suffix,
            "b_index": row.b_index,
            "b_fm": row.b_fm,
            "fresh_entries": row.g4output_entries,
            "fresh_coincidence_hits": row.coincidence_hits,
            "fresh_coincidence_efficiency": row.coincidence_efficiency,
            "archive_available": row.archive_available,
            "archive_entries": row.archive_entries,
            "archive_coincidence_hits": row.archive_coincidence_hits,
            "archive_coincidence_efficiency": row.archive_coincidence_efficiency,
            "fresh_minus_archive_coincidence_efficiency": row.fresh_minus_archive_coincidence_efficiency,
            "fresh_root_path": row.g4output_root_path,
            "archive_root_path": row.archive_root_path,
        }
        for row in rows
    ]

    summary_payload = {
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "dataset_id": args.dataset_id,
        "geometry_macro": str(args.geometry_macro.resolve()),
        "b_weighting_rule": "keep_first_floor(N_raw * b / bmax)_events_per_b_file_before_unphysical_cut",
        "bmax_fm": args.bmax_fm,
        "headline_bmax_fm": args.headline_bmax_fm,
        "sigma_validation_status": "manuscript_anchor_only",
        "notes": [
            "The local dataset can independently validate the Geant4 coincidence efficiency eta_det.",
            "Because each raw dbreak header carries '10000events' and the stored event numbers are sparse, the local dataset also supports an absolute shell-by-shell breakup cross section estimate when that header is interpreted as the incident-event count for the corresponding b file.",
            "Z-pol elastic input uses the b/bmax prefix selection rule implemented by GenInputRoot_qmdrawdata.",
            "header_events_total comes from the '10000events' raw-file headers; generator_selected_events_total is the number of stored breakup rows actually available before the unphysical cut.",
            "The recommended cross section estimator for b_discrete data is the shell sum sigma = sum(2*pi*b_i*delta_b*P_i), not the collapsed area formula pi*b_max^2*(N/N0) unless the original sampling law is known to be area-uniform.",
        ],
        "paths": {
            "raw_base": str(args.raw_base.resolve()),
            "g4input_base": str(args.g4input_base.resolve()),
            "g4output_base": str(args.g4output_base.resolve()),
            "archive_base": str(args.archive_base.resolve()),
            "generator_log": str(args.generator_log.resolve()),
            "result_dir": str(args.result_dir.resolve()),
            "pb_profile_csv": str((args.result_dir / "imqmd_pb_profile.csv").resolve()),
            "pb_plot_png": str(pb_plot_path.resolve()),
        },
        "aggregate": {row["scope_id"]: row for row in aggregate_summary_rows},
        "cross_section_estimates": {row["scope_id"]: row for row in shell_cross_section_rows},
        "pb_profile": pb_profile_rows,
        "per_file": [asdict(row) for row in rows],
    }

    write_csv(args.result_dir / "generator_selection_audit.csv", generator_audit_rows)
    write_json(args.result_dir / "generator_selection_audit.json", generator_audit_rows)
    write_csv(args.result_dir / "g4_validation_counts.csv", g4_validation_rows)
    write_json(args.result_dir / "g4_validation_counts.json", g4_validation_rows)
    write_csv(args.result_dir / "fresh_vs_archive_compare.csv", compare_rows)
    write_json(args.result_dir / "fresh_vs_archive_compare.json", compare_rows)
    write_csv(args.result_dir / "imqmd_pb_profile.csv", pb_profile_rows)
    write_json(args.result_dir / "imqmd_pb_profile.json", pb_profile_rows)
    write_csv(
        args.result_dir / "imqmd_cross_section_estimates.csv",
        [
            {
                "scope_id": row["scope_id"],
                "b_min_center_fm": row["b_min_center_fm"],
                "b_max_center_fm": row["b_max_center_fm"],
                "delta_b_fm": row["delta_b_fm"],
                "header_events_total": row["header_events_total"],
                "raw_breakup_rows_total": row["raw_breakup_rows_total"],
                "aggregate_breakup_probability": row["aggregate_breakup_probability"],
                "breakup_cross_section_shell_midpoint_mb": row["breakup_cross_section_shell_midpoint_mb"],
                "breakup_cross_section_naive_area_mb": row["breakup_cross_section_naive_area_mb"],
                "shell_vs_naive_ratio": row["shell_vs_naive_ratio"],
                "selected_input_cross_section_shell_midpoint_mb": row["selected_input_cross_section_shell_midpoint_mb"],
                "detected_coincidence_cross_section_shell_midpoint_mb": row["detected_coincidence_cross_section_shell_midpoint_mb"],
            }
            for row in shell_cross_section_rows
        ],
    )
    write_json(args.result_dir / "imqmd_cross_section_estimates.json", shell_cross_section_rows)
    write_pb_plot(pb_plot_path, pb_profile_rows, convergence_start_b=5)
    write_json(args.result_dir / "validation_summary.json", summary_payload)


if __name__ == "__main__":
    main()
