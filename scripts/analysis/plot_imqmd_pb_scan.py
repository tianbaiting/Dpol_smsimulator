#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import math
import re
from dataclasses import asdict, dataclass
from pathlib import Path
from statistics import median
from typing import Any

import ROOT


DATASET_RE = re.compile(
    r"^d\+(?P<target>[A-Za-z0-9]+)E(?P<energy>[0-9]+)g(?P<gamma>[0-9]{3})(?P<pol>znp|zpn)$"
)
RAW_HEADER_RE = re.compile(
    r"b=\s*(?P<b>[0-9]+(?:\.[0-9]+)?)fm\s+(?P<events>[0-9]+)events"
)


@dataclass(frozen=True)
class BPoint:
    b_index: int
    b_center_fm: float
    header_events: int
    raw_breakup_rows: int
    breakup_probability: float
    shell_area_fm2: float
    shell_cross_section_mb: float
    cumulative_cross_section_mb: float


@dataclass(frozen=True)
class DatasetSummary:
    dataset_id: str
    target: str
    energy_mev_u: int
    gamma_label: str
    gamma_value: float
    pol: str
    b_file_count: int
    b_min_fm: float
    b_max_fm: float
    header_events_total: int
    raw_breakup_rows_total: int
    integrated_breakup_cross_section_mb: float
    peak_breakup_probability: float
    peak_breakup_probability_b_fm: float
    profile_csv_path: str
    plot_png_path: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    if not rows:
        raise ValueError(f"no rows for {path}")
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def parse_dataset_dir(path: Path) -> dict[str, Any]:
    match = DATASET_RE.match(path.name)
    if not match:
        raise RuntimeError(f"unsupported dataset directory name: {path.name}")
    gamma_label = match.group("gamma")
    return {
        "dataset_id": path.name,
        "target": match.group("target"),
        "energy_mev_u": int(match.group("energy")),
        "gamma_label": gamma_label,
        "gamma_value": int(gamma_label) / 100.0,
        "pol": match.group("pol"),
    }


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


def b_index_from_name(path: Path) -> int:
    match = re.search(r"dbreakb([0-9]{2})", path.name)
    if not match:
        raise RuntimeError(f"failed to parse b index from {path.name}")
    return int(match.group(1))


def build_shell_edges(b_values: list[float]) -> list[tuple[float, float]]:
    if not b_values:
        return []
    if len(b_values) == 1:
        width = 1.0
        center = b_values[0]
        return [(max(0.0, center - 0.5 * width), center + 0.5 * width)]

    diffs = [b_values[idx + 1] - b_values[idx] for idx in range(len(b_values) - 1)]
    default_width = median(diffs)
    edges: list[tuple[float, float]] = []
    for idx, center in enumerate(b_values):
        if idx == 0:
            low = max(0.0, center - 0.5 * diffs[0])
        else:
            low = 0.5 * (b_values[idx - 1] + center)
        if idx == len(b_values) - 1:
            high = center + 0.5 * diffs[-1]
        else:
            high = 0.5 * (center + b_values[idx + 1])
        if not (high > low):
            high = low + default_width
        edges.append((low, high))
    return edges


def collect_dataset_profile(dataset_dir: Path) -> tuple[dict[str, Any], list[BPoint]]:
    metadata = parse_dataset_dir(dataset_dir)
    raw_files = sorted(dataset_dir.glob("dbreakb*.dat"))
    if not raw_files:
        raise RuntimeError(f"no dbreakb files found in {dataset_dir}")

    raw_rows: list[dict[str, Any]] = []
    for raw_file in raw_files:
        b_center_fm, header_events = parse_raw_header(raw_file)
        raw_breakup_rows = count_raw_breakup_rows(raw_file)
        raw_rows.append(
            {
                "b_index": b_index_from_name(raw_file),
                "b_center_fm": b_center_fm,
                "header_events": header_events,
                "raw_breakup_rows": raw_breakup_rows,
            }
        )
    raw_rows.sort(key=lambda row: row["b_center_fm"])

    shell_edges = build_shell_edges([float(row["b_center_fm"]) for row in raw_rows])
    cumulative_cross_section_mb = 0.0
    points: list[BPoint] = []
    for row, (edge_low, edge_high) in zip(raw_rows, shell_edges):
        shell_area_fm2 = math.pi * (edge_high * edge_high - edge_low * edge_low)
        breakup_probability = (
            float(row["raw_breakup_rows"]) / float(row["header_events"])
            if row["header_events"] > 0 else 0.0
        )
        shell_cross_section_mb = shell_area_fm2 * breakup_probability * 10.0
        cumulative_cross_section_mb += shell_cross_section_mb
        points.append(
            BPoint(
                b_index=int(row["b_index"]),
                b_center_fm=float(row["b_center_fm"]),
                header_events=int(row["header_events"]),
                raw_breakup_rows=int(row["raw_breakup_rows"]),
                breakup_probability=breakup_probability,
                shell_area_fm2=shell_area_fm2,
                shell_cross_section_mb=shell_cross_section_mb,
                cumulative_cross_section_mb=cumulative_cross_section_mb,
            )
        )
    return metadata, points


def make_dataset_summary(
    metadata: dict[str, Any],
    points: list[BPoint],
    profile_csv_path: Path,
    plot_png_path: Path,
) -> DatasetSummary:
    peak_point = max(points, key=lambda point: point.breakup_probability)
    return DatasetSummary(
        dataset_id=str(metadata["dataset_id"]),
        target=str(metadata["target"]),
        energy_mev_u=int(metadata["energy_mev_u"]),
        gamma_label=str(metadata["gamma_label"]),
        gamma_value=float(metadata["gamma_value"]),
        pol=str(metadata["pol"]),
        b_file_count=len(points),
        b_min_fm=min(point.b_center_fm for point in points),
        b_max_fm=max(point.b_center_fm for point in points),
        header_events_total=sum(point.header_events for point in points),
        raw_breakup_rows_total=sum(point.raw_breakup_rows for point in points),
        integrated_breakup_cross_section_mb=points[-1].cumulative_cross_section_mb,
        peak_breakup_probability=peak_point.breakup_probability,
        peak_breakup_probability_b_fm=peak_point.b_center_fm,
        profile_csv_path=str(profile_csv_path.resolve()),
        plot_png_path=str(plot_png_path.resolve()),
    )


def style_graph(graph: Any, color: int, marker: int) -> None:
    graph.SetLineColor(color)
    graph.SetMarkerColor(color)
    graph.SetMarkerStyle(marker)
    graph.SetMarkerSize(1.0)
    graph.SetLineWidth(2)


def plot_dataset(path: Path, metadata: dict[str, Any], points: list[BPoint]) -> None:
    ROOT.gROOT.SetBatch(True)
    ROOT.gStyle.SetOptStat(0)

    b_values = [point.b_center_fm for point in points]
    p_values = [point.breakup_probability for point in points]
    shell_values = [point.shell_cross_section_mb for point in points]
    cum_values = [point.cumulative_cross_section_mb for point in points]

    canvas = ROOT.TCanvas(
        f"c_{metadata['dataset_id']}",
        str(metadata["dataset_id"]),
        900,
        900,
    )
    canvas.Divide(1, 2)

    graph_p = ROOT.TGraph(len(points))
    graph_shell = ROOT.TGraph(len(points))
    graph_cum = ROOT.TGraph(len(points))
    for idx, point in enumerate(points):
        graph_p.SetPoint(idx, point.b_center_fm, point.breakup_probability)
        graph_shell.SetPoint(idx, point.b_center_fm, point.shell_cross_section_mb)
        graph_cum.SetPoint(idx, point.b_center_fm, point.cumulative_cross_section_mb)

    style_graph(graph_p, ROOT.kBlue + 1, 20)
    style_graph(graph_shell, ROOT.kOrange + 7, 24)
    style_graph(graph_cum, ROOT.kRed + 1, 21)

    title = (
        f"d + {metadata['target']}, E = {metadata['energy_mev_u']} MeV/u, "
        f"gamma = {metadata['gamma_value']:.2f}, pol = {metadata['pol']}"
    )

    pad1 = canvas.cd(1)
    pad1.SetGrid()
    frame1 = pad1.DrawFrame(
        min(b_values) - 0.5,
        0.0,
        max(b_values) + 0.5,
        max(max(p_values) * 1.15, 0.05),
    )
    frame1.SetTitle(f"{title};impact parameter b [fm];P(b)")
    graph_p.Draw("LP SAME")

    legend1 = ROOT.TLegend(0.14, 0.76, 0.52, 0.88)
    legend1.SetBorderSize(0)
    legend1.AddEntry(graph_p, "P(b) = N_breakup / N_incident", "lp")
    legend1.Draw()

    pad2 = canvas.cd(2)
    pad2.SetGrid()
    frame2 = pad2.DrawFrame(
        min(b_values) - 0.5,
        0.0,
        max(b_values) + 0.5,
        max(max(cum_values, default=0.0), max(shell_values, default=0.0)) * 1.15,
    )
    frame2.SetTitle(
        "Shell contribution and cumulative breakup cross section;"
        "impact parameter b [fm];cross section [mb]"
    )
    graph_shell.Draw("LP SAME")
    graph_cum.Draw("LP SAME")

    legend2 = ROOT.TLegend(0.14, 0.72, 0.72, 0.88)
    legend2.SetBorderSize(0)
    legend2.AddEntry(graph_shell, "shell d#sigma_{bu}", "lp")
    legend2.AddEntry(graph_cum, "cumulative #sigma_{bu}", "lp")
    legend2.Draw()

    canvas.SaveAs(str(path))


def plot_group_overlay(path: Path, target: str, energy_mev_u: int, summaries: list[DatasetSummary],
                       profiles: dict[str, list[BPoint]]) -> None:
    ROOT.gROOT.SetBatch(True)
    ROOT.gStyle.SetOptStat(0)

    gamma_values = sorted({summary.gamma_value for summary in summaries})
    colors = [
        ROOT.kBlue + 1,
        ROOT.kRed + 1,
        ROOT.kGreen + 2,
        ROOT.kMagenta + 1,
        ROOT.kOrange + 7,
        ROOT.kCyan + 2,
        ROOT.kBlack,
    ]
    pols = ["znp", "zpn"]

    canvas = ROOT.TCanvas(f"c_{target}_{energy_mev_u}", f"{target}_{energy_mev_u}", 1100, 900)
    canvas.Divide(1, 2)
    kept_graphs: list[Any] = []
    kept_legends: list[Any] = []

    for pad_index, pol in enumerate(pols, start=1):
        pad = canvas.cd(pad_index)
        pad.SetGrid()
        pol_summaries = [summary for summary in summaries if summary.pol == pol]
        if not pol_summaries:
            continue
        b_values = [point.b_center_fm for summary in pol_summaries for point in profiles[summary.dataset_id]]
        p_values = [point.breakup_probability for summary in pol_summaries for point in profiles[summary.dataset_id]]
        frame = pad.DrawFrame(
            min(b_values) - 0.5,
            0.0,
            max(b_values) + 0.5,
            max(max(p_values) * 1.15, 0.05),
        )
        frame.SetTitle(
            f"d + {target}, E = {energy_mev_u} MeV/u, pol = {pol};"
            "impact parameter b [fm];P(b)"
        )
        legend = ROOT.TLegend(0.12, 0.60, 0.40, 0.88)
        legend.SetBorderSize(0)
        for idx, gamma_value in enumerate(gamma_values):
            matching = [summary for summary in pol_summaries if math.isclose(summary.gamma_value, gamma_value)]
            if not matching:
                continue
            summary = matching[0]
            graph = ROOT.TGraph(len(profiles[summary.dataset_id]))
            for point_index, point in enumerate(profiles[summary.dataset_id]):
                graph.SetPoint(point_index, point.b_center_fm, point.breakup_probability)
            style_graph(graph, colors[idx % len(colors)], 20 + (idx % 5))
            graph.Draw("LP SAME")
            legend.AddEntry(graph, f"gamma = {gamma_value:.2f}", "lp")
            kept_graphs.append(graph)
        legend.Draw()
        kept_legends.append(legend)

    canvas.SaveAs(str(path))


def main() -> None:
    root = repo_root()
    raw_base = root / "data" / "qmdrawdata" / "qmdrawdata" / "z_pol" / "b_discrete"
    results_dir = root / "results" / "imqmd_pb_scan"
    profiles_dir = results_dir / "profiles"
    plots_dir = results_dir / "plots"
    overlays_dir = results_dir / "overlays"
    results_dir.mkdir(parents=True, exist_ok=True)
    profiles_dir.mkdir(parents=True, exist_ok=True)
    plots_dir.mkdir(parents=True, exist_ok=True)
    overlays_dir.mkdir(parents=True, exist_ok=True)

    dataset_dirs = sorted(path for path in raw_base.iterdir() if path.is_dir())
    if not dataset_dirs:
        raise RuntimeError(f"no dataset directories found under {raw_base}")

    dataset_summaries: list[DatasetSummary] = []
    profiles_map: dict[str, list[BPoint]] = {}
    for dataset_dir in dataset_dirs:
        metadata, points = collect_dataset_profile(dataset_dir)
        profile_csv_path = profiles_dir / f"{metadata['dataset_id']}.csv"
        profile_json_path = profiles_dir / f"{metadata['dataset_id']}.json"
        plot_png_path = plots_dir / f"{metadata['dataset_id']}.png"
        profile_rows = [asdict(point) for point in points]
        write_csv(profile_csv_path, profile_rows)
        write_json(profile_json_path, profile_rows)
        plot_dataset(plot_png_path, metadata, points)
        summary = make_dataset_summary(metadata, points, profile_csv_path, plot_png_path)
        dataset_summaries.append(summary)
        profiles_map[summary.dataset_id] = points

    group_map: dict[tuple[str, int], list[DatasetSummary]] = {}
    for summary in dataset_summaries:
        group_map.setdefault((summary.target, summary.energy_mev_u), []).append(summary)
    overlay_rows: list[dict[str, Any]] = []
    for (target, energy_mev_u), summaries in sorted(group_map.items()):
        overlay_path = overlays_dir / f"d_{target}_E{energy_mev_u}_pb_by_gamma.png"
        plot_group_overlay(overlay_path, target, energy_mev_u, summaries, profiles_map)
        overlay_rows.append(
            {
                "target": target,
                "energy_mev_u": energy_mev_u,
                "overlay_png_path": str(overlay_path.resolve()),
                "dataset_count": len(summaries),
                "gammas": ",".join(
                    f"{gamma_value:.2f}"
                    for gamma_value in sorted({summary.gamma_value for summary in summaries})
                ),
            }
        )

    summary_rows = [asdict(summary) for summary in dataset_summaries]
    write_csv(results_dir / "dataset_summary.csv", summary_rows)
    write_json(results_dir / "dataset_summary.json", summary_rows)
    write_csv(results_dir / "overlay_summary.csv", overlay_rows)
    write_json(results_dir / "overlay_summary.json", overlay_rows)


if __name__ == "__main__":
    main()
