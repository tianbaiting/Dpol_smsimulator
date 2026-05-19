#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import math
import os
import re
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


REACTION_TYPES = ("dkeep", "break", "spall", "other")
YPOL_RE = re.compile(
    r"^d\+(?P<target>Sn112|Sn124)E(?P<energy>[0-9]+)g(?P<gamma>[0-9]{3})(?P<pol>ynp|ypn)-RP360$"
)
ZPOL_RE = re.compile(
    r"^d\+(?P<target>Sn112|Sn124)E(?P<energy>[0-9]+)g(?P<gamma>[0-9]{3})(?P<pol>znp|zpn)$"
)
DBREAK_RE = re.compile(r"^dbreakb(?P<b_index>[0-9]{2})\.dat$")
RAW_HEADER_RE = re.compile(
    r"b=\s*(?P<b>[0-9]+(?:\.[0-9]+)?)fm\s+(?P<events>[0-9]+)events"
)


@dataclass(frozen=True)
class YpolKey:
    target: str
    energy_mev_u: int
    gamma_label: str
    gamma_value: float
    pol: str
    dataset_id: str


@dataclass(frozen=True)
class ReactionTypeRow:
    target: str
    energy_mev_u: int
    gamma_label: str
    gamma_value: float
    pol: str
    dataset_id: str
    total_events: int
    dkeep_count: int
    break_count: int
    spall_count: int
    other_count: int
    dkeep_fraction: float
    break_fraction: float
    spall_fraction: float
    other_fraction: float
    source_path: str


@dataclass(frozen=True)
class ZpolKey:
    target: str
    energy_mev_u: int
    gamma_label: str
    gamma_value: float
    pol: str
    b_index: int
    dataset_id: str


@dataclass(frozen=True)
class ZpolBreakupRow:
    target: str
    energy_mev_u: int
    gamma_label: str
    gamma_value: float
    pol: str
    b_index: int
    b_fm: float
    header_events: int
    breakup_rows: int
    breakup_probability: float
    source_path: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(
        description="Analyze ImQMD ypol reaction categories and zpol b-discrete breakup probabilities."
    )
    parser.add_argument(
        "--qmd-root",
        type=Path,
        default=root / "data" / "qmdrawdata" / "qmdrawdata",
    )
    parser.add_argument(
        "--result-dir",
        type=Path,
        default=root / "results" / "imqmd_reaction_category",
    )
    parser.add_argument(
        "--report-dir",
        type=Path,
        default=root / "docs" / "reports" / "imqmd_reaction_category",
    )
    parser.add_argument("--energy", type=int, default=190)
    parser.add_argument("--targets", nargs="+", default=["Sn112", "Sn124"])
    parser.add_argument(
        "--ypol-gammas",
        nargs="+",
        default=["050", "055", "060", "065", "070", "075", "080", "085"],
    )
    parser.add_argument(
        "--zpol-gammas",
        nargs="+",
        default=["050", "060", "070", "080", "090", "100"],
    )
    return parser.parse_args()


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        raise RuntimeError(f"no rows for {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def parse_ypol_reactiontype_path(path: Path) -> YpolKey:
    dataset_id = path.parent.name
    match = YPOL_RE.match(dataset_id)
    if not match:
        raise RuntimeError(f"unsupported ypol reactiontype path: {path}")
    gamma_label = match.group("gamma")
    return YpolKey(
        target=match.group("target"),
        energy_mev_u=int(match.group("energy")),
        gamma_label=gamma_label,
        gamma_value=int(gamma_label) / 100.0,
        pol=match.group("pol"),
        dataset_id=dataset_id,
    )


def count_reaction_types(path: Path) -> dict[str, int]:
    counts: Counter[str] = Counter()
    with path.open(encoding="utf-8", errors="replace") as handle:
        for line_no, line in enumerate(handle, start=1):
            parts = line.split()
            if not parts:
                continue
            if len(parts) < 4:
                raise RuntimeError(f"bad reactiontype row at {path}:{line_no}: {line.rstrip()}")
            reaction_type = parts[3]
            if reaction_type not in REACTION_TYPES:
                raise RuntimeError(f"unknown reaction type {reaction_type!r} at {path}:{line_no}")
            counts[reaction_type] += 1
    return {reaction_type: counts[reaction_type] for reaction_type in sorted(counts)}


def fraction(count: int, total: int) -> float:
    if total <= 0:
        return 0.0
    return count / total


def summarize_reaction_type_file(path: Path) -> ReactionTypeRow:
    key = parse_ypol_reactiontype_path(path)
    counts = count_reaction_types(path)
    total = sum(counts.values())
    return ReactionTypeRow(
        target=key.target,
        energy_mev_u=key.energy_mev_u,
        gamma_label=key.gamma_label,
        gamma_value=key.gamma_value,
        pol=key.pol,
        dataset_id=key.dataset_id,
        total_events=total,
        dkeep_count=counts.get("dkeep", 0),
        break_count=counts.get("break", 0),
        spall_count=counts.get("spall", 0),
        other_count=counts.get("other", 0),
        dkeep_fraction=fraction(counts.get("dkeep", 0), total),
        break_fraction=fraction(counts.get("break", 0), total),
        spall_fraction=fraction(counts.get("spall", 0), total),
        other_fraction=fraction(counts.get("other", 0), total),
        source_path=str(path),
    )


def parse_zpol_dbreak_path(path: Path) -> ZpolKey:
    dataset_id = path.parent.name
    dataset_match = ZPOL_RE.match(dataset_id)
    file_match = DBREAK_RE.match(path.name)
    if not dataset_match or not file_match:
        raise RuntimeError(f"unsupported zpol dbreak path: {path}")
    gamma_label = dataset_match.group("gamma")
    return ZpolKey(
        target=dataset_match.group("target"),
        energy_mev_u=int(dataset_match.group("energy")),
        gamma_label=gamma_label,
        gamma_value=int(gamma_label) / 100.0,
        pol=dataset_match.group("pol"),
        b_index=int(file_match.group("b_index")),
        dataset_id=dataset_id,
    )


def parse_dbreak_header(path: Path) -> tuple[float, int]:
    with path.open(encoding="utf-8", errors="replace") as handle:
        header = handle.readline().strip()
    match = RAW_HEADER_RE.search(header)
    if not match:
        raise RuntimeError(f"failed to parse dbreak header from {path}: {header}")
    return float(match.group("b")), int(match.group("events"))


def count_dbreak_rows(path: Path) -> int:
    count = 0
    with path.open(encoding="utf-8", errors="replace") as handle:
        next(handle, None)
        next(handle, None)
        for line in handle:
            if line.split():
                count += 1
    return count


def summarize_zpol_dbreak_file(path: Path) -> ZpolBreakupRow:
    key = parse_zpol_dbreak_path(path)
    b_fm, header_events = parse_dbreak_header(path)
    breakup_rows = count_dbreak_rows(path)
    return ZpolBreakupRow(
        target=key.target,
        energy_mev_u=key.energy_mev_u,
        gamma_label=key.gamma_label,
        gamma_value=key.gamma_value,
        pol=key.pol,
        b_index=key.b_index,
        b_fm=b_fm,
        header_events=header_events,
        breakup_rows=breakup_rows,
        breakup_probability=fraction(breakup_rows, header_events),
        source_path=str(path),
    )


def collect_ypol_rows(
    qmd_root: Path,
    targets: Iterable[str],
    energy: int,
    gammas: Iterable[str],
) -> list[ReactionTypeRow]:
    rows: list[ReactionTypeRow] = []
    for target in targets:
        base = qmd_root / "20260413ypol" / f"d+{target}E{energy}"
        for gamma in gammas:
            for pol in ("ynp", "ypn"):
                path = base / f"d+{target}E{energy}g{gamma}{pol}-RP360" / "reactiontype.dat"
                if path.exists():
                    rows.append(summarize_reaction_type_file(path))
    return sorted(rows, key=lambda row: (row.target, row.gamma_value, row.pol))


def collect_zpol_rows(
    qmd_root: Path,
    targets: Iterable[str],
    energy: int,
    gammas: Iterable[str],
) -> list[ZpolBreakupRow]:
    rows: list[ZpolBreakupRow] = []
    for target in targets:
        for gamma in gammas:
            for pol in ("znp", "zpn"):
                dataset = qmd_root / "z_pol" / "b_discrete" / f"d+{target}E{energy}g{gamma}{pol}"
                if not dataset.exists():
                    continue
                for path in sorted(dataset.glob("dbreakb*.dat")):
                    rows.append(summarize_zpol_dbreak_file(path))
    return sorted(rows, key=lambda row: (row.target, row.gamma_value, row.pol, row.b_index))


def configure_matplotlib() -> object:
    os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plt.rcParams.update(
        {
            "font.size": 10,
            "axes.titlesize": 11,
            "axes.labelsize": 10,
            "legend.fontsize": 8,
            "xtick.labelsize": 8,
            "ytick.labelsize": 8,
        }
    )
    return plt


def plot_ypol_stacked_bars(rows: list[ReactionTypeRow], result_dir: Path) -> list[Path]:
    plt = configure_matplotlib()
    paths: list[Path] = []
    colors = {
        "dkeep": "#4C78A8",
        "break": "#F58518",
        "spall": "#54A24B",
        "other": "#B279A2",
    }
    for target in sorted({row.target for row in rows}):
        target_rows = [row for row in rows if row.target == target]
        labels = [f"g{row.gamma_label}\n{row.pol}" for row in target_rows]
        x_values = list(range(len(target_rows)))
        bottoms = [0.0 for _ in target_rows]
        fig, ax = plt.subplots(figsize=(12.0, 4.8))
        for reaction_type in REACTION_TYPES:
            values = [getattr(row, f"{reaction_type}_fraction") for row in target_rows]
            ax.bar(
                x_values,
                values,
                bottom=bottoms,
                label=reaction_type,
                color=colors[reaction_type],
                width=0.78,
            )
            bottoms = [bottom + value for bottom, value in zip(bottoms, values)]
        ax.set_title(f"ImQMD y-pol reaction type fractions: {target}, E=190 MeV/u, RP360")
        ax.set_ylabel("fraction of all ImQMD events")
        ax.set_ylim(0.0, 1.0)
        ax.set_xticks(x_values)
        ax.set_xticklabels(labels)
        ax.grid(axis="y", alpha=0.25)
        ax.legend(ncol=4, loc="upper center", bbox_to_anchor=(0.5, 1.16))
        fig.tight_layout()
        path = result_dir / f"ypol_reaction_type_fraction_{target}.png"
        fig.savefig(path, dpi=200)
        plt.close(fig)
        paths.append(path)
    return paths


def average_rows_by_target_gamma(rows: list[ReactionTypeRow]) -> list[dict[str, float | str]]:
    grouped: dict[tuple[str, str, float], list[ReactionTypeRow]] = {}
    for row in rows:
        grouped.setdefault((row.target, row.gamma_label, row.gamma_value), []).append(row)

    averaged: list[dict[str, float | str]] = []
    for (target, gamma_label, gamma_value), group in grouped.items():
        payload: dict[str, float | str] = {
            "target": target,
            "gamma_label": gamma_label,
            "gamma_value": gamma_value,
        }
        for reaction_type in REACTION_TYPES:
            payload[f"{reaction_type}_fraction"] = sum(
                getattr(row, f"{reaction_type}_fraction") for row in group
            ) / len(group)
        averaged.append(payload)
    return sorted(averaged, key=lambda item: (str(item["target"]), float(item["gamma_value"])))


def plot_ypol_trends(rows: list[ReactionTypeRow], result_dir: Path) -> Path:
    plt = configure_matplotlib()
    averaged = average_rows_by_target_gamma(rows)
    fig, axes = plt.subplots(2, 2, figsize=(10.5, 7.2), sharex=True)
    axes_flat = [axis for row_axes in axes for axis in row_axes]
    colors = {"Sn112": "#4C78A8", "Sn124": "#F58518"}
    markers = {"Sn112": "o", "Sn124": "s"}

    for axis, reaction_type in zip(axes_flat, REACTION_TYPES):
        for target in sorted({str(row["target"]) for row in averaged}):
            target_rows = [row for row in averaged if row["target"] == target]
            axis.plot(
                [float(row["gamma_value"]) for row in target_rows],
                [float(row[f"{reaction_type}_fraction"]) for row in target_rows],
                marker=markers[target],
                color=colors[target],
                label=target,
                linewidth=1.8,
            )
        axis.set_title(reaction_type)
        axis.set_ylabel("fraction")
        axis.grid(alpha=0.25)
    axes_flat[2].set_xlabel(r"$\gamma$")
    axes_flat[3].set_xlabel(r"$\gamma$")
    axes_flat[0].legend(loc="best")
    fig.suptitle("ImQMD y-pol reaction type trends, ynp/ypn averaged")
    fig.tight_layout()
    path = result_dir / "ypol_reaction_type_trends.png"
    fig.savefig(path, dpi=200)
    plt.close(fig)
    return path


def plot_zpol_breakup_probability(rows: list[ZpolBreakupRow], result_dir: Path) -> list[Path]:
    plt = configure_matplotlib()
    paths: list[Path] = []
    for target in sorted({row.target for row in rows}):
        fig, axes = plt.subplots(1, 2, figsize=(12.0, 4.8), sharey=True)
        for axis, pol in zip(axes, ("znp", "zpn")):
            pol_rows = [row for row in rows if row.target == target and row.pol == pol]
            for gamma in sorted({row.gamma_label for row in pol_rows}):
                gamma_rows = sorted(
                    [row for row in pol_rows if row.gamma_label == gamma],
                    key=lambda row: row.b_fm,
                )
                axis.plot(
                    [row.b_fm for row in gamma_rows],
                    [100.0 * row.breakup_probability for row in gamma_rows],
                    marker="o",
                    linewidth=1.4,
                    markersize=3.5,
                    label=f"g{gamma}",
                )
            axis.set_title(pol)
            axis.set_xlabel("impact parameter b [fm]")
            axis.grid(alpha=0.25)
        axes[0].set_ylabel("stored dbreak rows / header events [%]")
        axes[1].legend(ncol=2, loc="best")
        fig.suptitle(f"ImQMD z-pol b-discrete breakup probability: {target}, E=190 MeV/u")
        fig.tight_layout()
        path = result_dir / f"zpol_breakup_probability_{target}.png"
        fig.savefig(path, dpi=200)
        plt.close(fig)
        paths.append(path)
    return paths


def report_float(value: float) -> str:
    return f"{100.0 * value:.1f}\\%"


def top_break_fraction(rows: list[ReactionTypeRow], target: str) -> tuple[str, float]:
    target_rows = [row for row in rows if row.target == target]
    row = max(target_rows, key=lambda item: item.break_fraction)
    return f"g{row.gamma_label} {row.pol}", row.break_fraction


def max_zpol_probability(rows: list[ZpolBreakupRow], target: str) -> tuple[str, float, float]:
    target_rows = [row for row in rows if row.target == target]
    row = max(target_rows, key=lambda item: item.breakup_probability)
    return f"g{row.gamma_label} {row.pol}", row.b_fm, row.breakup_probability


def relative_figure_path(report_dir: Path, figure_path: Path) -> str:
    return Path(os.path.relpath(figure_path.resolve(), report_dir.resolve())).as_posix()


def latex_path(path: Path, base_dir: Path) -> str:
    return Path(os.path.relpath(path.resolve(), base_dir.resolve())).as_posix()


def write_latex_report(
    report_dir: Path,
    result_dir: Path,
    ypol_rows: list[ReactionTypeRow],
    zpol_rows: list[ZpolBreakupRow],
    figures: list[Path],
) -> Path:
    report_dir.mkdir(parents=True, exist_ok=True)
    tex_path = report_dir / "imqmd_reaction_category.tex"
    fig = {path.stem: path for path in figures}
    sn112_break = top_break_fraction(ypol_rows, "Sn112")
    sn124_break = top_break_fraction(ypol_rows, "Sn124")
    sn112_z = max_zpol_probability(zpol_rows, "Sn112")
    sn124_z = max_zpol_probability(zpol_rows, "Sn124")
    csv_rel = latex_path(result_dir, report_dir)

    lines = [
        r"\documentclass[11pt]{article}",
        r"\usepackage[a4paper,margin=18mm]{geometry}",
        r"\usepackage{graphicx}",
        r"\usepackage{booktabs}",
        r"\usepackage{hyperref}",
        r"\title{ImQMD Reaction-Class Fractions and z-pol Breakup Profiles}",
        r"\author{}",
        r"\date{}",
        r"\begin{document}",
        r"\maketitle",
        r"\section*{Scope}",
        (
            r"The y-pol sample is "
            r"\texttt{data/qmdrawdata/qmdrawdata/20260413ypol/}: "
            r"$^{112}$Sn and $^{124}$Sn at 190 MeV/u, "
            r"$\gamma=0.50\ldots0.85$, ynp/ypn, RP360. "
            r"Each \texttt{reactiontype.dat} row contributes one ImQMD event. "
            r"The denominator is the total number of rows in that file."
        ),
        (
            r"The z-pol Sn112/Sn124 b-discrete files do not carry "
            r"\texttt{reactiontype.dat} labels.  For z-pol, this note reports "
            r"$P_{\mathrm{breakup}}(b)=N_{\mathrm{dbreak}}(b)/N_{\mathrm{header}}(b)$ "
            r"from \texttt{dbreakbXX.dat}; it is not the same observable as the y-pol "
            r"reaction-class fraction."
        ),
        r"\section*{y-pol reaction categories}",
        (
            r"The four labels present in the files are \texttt{dkeep}, "
            r"\texttt{break}, \texttt{spall}, and \texttt{other}. "
            f"For Sn112 the largest break fraction is {report_float(sn112_break[1])} "
            f"at {sn112_break[0]}; for Sn124 it is {report_float(sn124_break[1])} "
            f"at {sn124_break[0]}."
        ),
        r"\begin{figure}[h]",
        r"\centering",
        rf"\includegraphics[width=0.95\linewidth]{{{relative_figure_path(report_dir, fig['ypol_reaction_type_fraction_Sn112'])}}}",
        r"\caption{Reaction-type fractions for y-pol $^{112}$Sn.}",
        r"\end{figure}",
        r"\begin{figure}[h]",
        r"\centering",
        rf"\includegraphics[width=0.95\linewidth]{{{relative_figure_path(report_dir, fig['ypol_reaction_type_fraction_Sn124'])}}}",
        r"\caption{Reaction-type fractions for y-pol $^{124}$Sn.}",
        r"\end{figure}",
        r"\begin{figure}[h]",
        r"\centering",
        rf"\includegraphics[width=0.88\linewidth]{{{relative_figure_path(report_dir, fig['ypol_reaction_type_trends'])}}}",
        r"\caption{Gamma trends after averaging ynp and ypn at fixed target and gamma.}",
        r"\end{figure}",
        r"\section*{z-pol breakup probability}",
        (
            f"The largest z-pol b-discrete probability in the scanned Sn112 files is "
            f"{report_float(sn112_z[2])} at {sn112_z[0]}, b={sn112_z[1]:.0f} fm. "
            f"For Sn124 the largest value is {report_float(sn124_z[2])} at "
            f"{sn124_z[0]}, b={sn124_z[1]:.0f} fm."
        ),
        r"\begin{figure}[h]",
        r"\centering",
        rf"\includegraphics[width=0.95\linewidth]{{{relative_figure_path(report_dir, fig['zpol_breakup_probability_Sn112'])}}}",
        r"\caption{Stored breakup probability profile for z-pol $^{112}$Sn b-discrete files.}",
        r"\end{figure}",
        r"\begin{figure}[h]",
        r"\centering",
        rf"\includegraphics[width=0.95\linewidth]{{{relative_figure_path(report_dir, fig['zpol_breakup_probability_Sn124'])}}}",
        r"\caption{Stored breakup probability profile for z-pol $^{124}$Sn b-discrete files.}",
        r"\end{figure}",
        r"\section*{Artifacts}",
        rf"CSV tables and figures are written under \texttt{{\detokenize{{{csv_rel}}}}}.",
        r"\end{document}",
        "",
    ]
    tex_path.write_text("\n".join(lines), encoding="utf-8")
    return tex_path


def main() -> None:
    args = parse_args()
    args.result_dir.mkdir(parents=True, exist_ok=True)

    ypol_rows = collect_ypol_rows(args.qmd_root, args.targets, args.energy, args.ypol_gammas)
    zpol_rows = collect_zpol_rows(args.qmd_root, args.targets, args.energy, args.zpol_gammas)
    if not ypol_rows:
        raise RuntimeError("no ypol reactiontype rows found")
    if not zpol_rows:
        raise RuntimeError("no zpol dbreak rows found")

    write_csv(args.result_dir / "ypol_reaction_type_fractions.csv", [asdict(row) for row in ypol_rows])
    write_csv(args.result_dir / "zpol_breakup_probability.csv", [asdict(row) for row in zpol_rows])

    figures: list[Path] = []
    figures.extend(plot_ypol_stacked_bars(ypol_rows, args.result_dir))
    figures.append(plot_ypol_trends(ypol_rows, args.result_dir))
    figures.extend(plot_zpol_breakup_probability(zpol_rows, args.result_dir))
    tex_path = write_latex_report(args.report_dir, args.result_dir, ypol_rows, zpol_rows, figures)

    print(f"wrote {len(ypol_rows)} ypol rows")
    print(f"wrote {len(zpol_rows)} zpol rows")
    print(f"wrote report {tex_path}")


if __name__ == "__main__":
    main()
