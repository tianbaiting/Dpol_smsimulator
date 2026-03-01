#!/usr/bin/env python3
# [EN] Usage: python3 scripts/analysis/run_pdc_geant4_position_scan.py --outdir "$SMSIMDIR/results/pdc_scan_B115T3deg" / [CN] 用法: python3 scripts/analysis/run_pdc_geant4_position_scan.py --outdir "$SMSIMDIR/results/pdc_scan_B115T3deg"

from __future__ import annotations

import argparse
import csv
import dataclasses
import math
import os
import pathlib
import subprocess
import time
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


@dataclasses.dataclass(frozen=True)
class Candidate:
    x1_cm: float
    x2_cm: float
    angle_deg: float


@dataclasses.dataclass
class CandidateResult:
    stage: str
    index: int
    candidate: Candidate
    status: str
    run_name: str
    macro_path: str
    sim_root: str
    analysis_root: str
    summary_csv: str
    track_eff: float = float("nan")
    eff_mean: float = float("nan")
    eff_std: float = float("nan")
    eff_cv: float = float("nan")
    edge_min: float = float("nan")
    edge_mean: float = float("nan")
    incidence_mean_deg: float = float("nan")
    incidence_rms_deg: float = float("nan")
    error: str = ""


def expand_smsimdir(path: str, smsimdir: str) -> str:
    return (
        path.replace("{SMSIMDIR}", smsimdir)
        .replace("$SMSIMDIR", smsimdir)
    )


def arange_inclusive(start: float, stop: float, step: float, digits: int = 6) -> List[float]:
    if step <= 0:
        raise ValueError("step must be > 0")
    values: List[float] = []
    n = 0
    current = start
    epsilon = abs(step) * 1e-9
    while current <= stop + epsilon:
        values.append(round(current, digits))
        n += 1
        current = start + n * step
    return values


def run_command(
    cmd: Sequence[str],
    log_path: pathlib.Path,
    cwd: pathlib.Path,
    allow_success_if_outputs_exist: Optional[Sequence[pathlib.Path]] = None,
) -> None:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as log:
        log.write("# command: " + " ".join(cmd) + "\n")
        log.write("# cwd: " + str(cwd) + "\n\n")
        proc = subprocess.run(
            cmd,
            cwd=str(cwd),
            stdout=log,
            stderr=subprocess.STDOUT,
            check=False,
            text=True,
        )
    if proc.returncode != 0:
        if allow_success_if_outputs_exist:
            all_outputs_ok = all(p.exists() and p.stat().st_size > 0 for p in allow_success_if_outputs_exist)
            if all_outputs_ok:
                with log_path.open("a", encoding="utf-8") as log:
                    log.write(
                        f"\n# non-zero exit tolerated: {proc.returncode}, "
                        "all expected output files exist and are non-empty.\n"
                    )
                return
        raise RuntimeError(f"Command failed (exit={proc.returncode}), see log: {log_path}")


def format_token(value: float, scale: int = 10) -> str:
    scaled = int(round(value * scale))
    sign = "p" if scaled >= 0 else "m"
    return f"{sign}{abs(scaled):04d}"


def extract_metrics(summary_csv: pathlib.Path) -> Dict[str, Tuple[float, float, float]]:
    metrics: Dict[str, Tuple[float, float, float]] = {}
    with summary_csv.open("r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            metric = row.get("metric", "").strip()
            if not metric:
                continue
            count = float(row.get("count", "nan"))
            mean = float(row.get("mean", "nan"))
            rms = float(row.get("rms", "nan"))
            metrics[metric] = (count, mean, rms)
    return metrics


def get_metric_mean(metrics: Dict[str, Tuple[float, float, float]], key: str, default: float = float("nan")) -> float:
    val = metrics.get(key)
    return default if val is None else val[1]


def get_metric_rms(metrics: Dict[str, Tuple[float, float, float]], key: str, default: float = float("nan")) -> float:
    val = metrics.get(key)
    return default if val is None else val[2]


def candidate_sort_key(res: CandidateResult) -> Tuple[float, float, float, float]:
    edge_min = res.edge_min if math.isfinite(res.edge_min) else -1.0
    eff_cv = res.eff_cv if math.isfinite(res.eff_cv) else 1e9
    incidence_rms = res.incidence_rms_deg if math.isfinite(res.incidence_rms_deg) else 1e9
    track_eff = res.track_eff if math.isfinite(res.track_eff) else -1.0
    # [EN] Higher edge_min and track_eff are better; lower cv and incidence RMS are better.
    # [CN] edge_min与track_eff越大越好；cv与入射角RMS越小越好。
    return (-edge_min, eff_cv, incidence_rms, -track_eff)


def build_macro_text(
    candidate: Candidate,
    run_name: str,
    output_dir: pathlib.Path,
    field_macro: str,
    target_pos_cm: Tuple[float, float, float],
    target_angle_deg: float,
    pdc_z1_cm: float,
    pdc_z2_cm: float,
    px_values: Sequence[float],
    pz_mevc: float,
    repeat_per_px: int,
) -> str:
    proton_mass_mevc2 = 938.2720813

    event_blocks: List[str] = []
    for px_mevc in px_values:
        momentum_mag = math.sqrt(px_mevc * px_mevc + pz_mevc * pz_mevc)
        kinetic_energy_mev = math.sqrt(momentum_mag * momentum_mag + proton_mass_mevc2 * proton_mass_mevc2) - proton_mass_mevc2
        angle_x_deg = math.degrees(math.atan2(px_mevc, pz_mevc))
        event_blocks.append(
            f"/action/gun/Energy {kinetic_energy_mev:.8f} MeV\n"
            f"/action/gun/AngleX {angle_x_deg:.8f} deg\n"
            f"/run/beamOn {repeat_per_px}"
        )
    event_block_text = "\n\n".join(event_blocks)

    return f"""
/control/getEnv SMSIMDIR

/samurai/geometry/Target/SetTarget false
/samurai/geometry/Target/Material Sn
/samurai/geometry/Target/Size 80 80 30 mm
/samurai/geometry/Target/Position {target_pos_cm[0]:.6f} {target_pos_cm[1]:.6f} {target_pos_cm[2]:.6f} cm
/samurai/geometry/Target/Angle {target_angle_deg:.6f} deg

/control/execute {field_macro}
/samurai/geometry/PDC/Angle {candidate.angle_deg:.6f} deg
/samurai/geometry/PDC/Position1 {candidate.x1_cm:.6f} 0 {pdc_z1_cm:.6f} cm
/samurai/geometry/PDC/Position2 {candidate.x2_cm:.6f} 0 {pdc_z2_cm:.6f} cm
/samurai/geometry/FillAir true
/samurai/geometry/Update

/action/file/OverWrite y
/action/file/RunName {run_name}
/action/file/SaveDirectory {output_dir}
/tracking/storeTrajectory 0

/action/gun/Type Pencil
/action/gun/SetBeamParticleName proton
/action/gun/Position 0 0 -400 cm
/action/gun/AngleY 0 deg

{event_block_text}
""".strip() + "\n"


def write_results_csv(path: pathlib.Path, results: Iterable[CandidateResult]) -> None:
    fieldnames = [
        "stage",
        "index",
        "status",
        "run_name",
        "x1_cm",
        "x2_cm",
        "angle_deg",
        "track_eff",
        "eff_mean",
        "eff_std",
        "eff_cv",
        "edge_min",
        "edge_mean",
        "incidence_mean_deg",
        "incidence_rms_deg",
        "edge_pass_70",
        "macro_path",
        "sim_root",
        "analysis_root",
        "summary_csv",
        "error",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for res in results:
            writer.writerow(
                {
                    "stage": res.stage,
                    "index": res.index,
                    "status": res.status,
                    "run_name": res.run_name,
                    "x1_cm": f"{res.candidate.x1_cm:.6f}",
                    "x2_cm": f"{res.candidate.x2_cm:.6f}",
                    "angle_deg": f"{res.candidate.angle_deg:.6f}",
                    "track_eff": res.track_eff,
                    "eff_mean": res.eff_mean,
                    "eff_std": res.eff_std,
                    "eff_cv": res.eff_cv,
                    "edge_min": res.edge_min,
                    "edge_mean": res.edge_mean,
                    "incidence_mean_deg": res.incidence_mean_deg,
                    "incidence_rms_deg": res.incidence_rms_deg,
                    "edge_pass_70": int(math.isfinite(res.edge_min) and res.edge_min >= 0.70),
                    "macro_path": res.macro_path,
                    "sim_root": res.sim_root,
                    "analysis_root": res.analysis_root,
                    "summary_csv": res.summary_csv,
                    "error": res.error,
                }
            )


def generate_candidates(
    x_values: Sequence[float],
    angle_values: Sequence[float],
    independent_x2: bool,
    x2_values: Sequence[float],
) -> List[Candidate]:
    candidates: List[Candidate] = []
    if independent_x2:
        for angle in angle_values:
            for x1 in x_values:
                for x2 in x2_values:
                    candidates.append(Candidate(x1_cm=x1, x2_cm=x2, angle_deg=angle))
    else:
        for angle in angle_values:
            for x in x_values:
                candidates.append(Candidate(x1_cm=x, x2_cm=x, angle_deg=angle))
    return candidates


def build_fine_candidates(
    seeds: Sequence[CandidateResult],
    independent_x2: bool,
    x_window: float,
    x_step: float,
    angle_window: float,
    angle_step: float,
) -> List[Candidate]:
    x_offsets = arange_inclusive(-x_window, x_window, x_step)
    angle_offsets = arange_inclusive(-angle_window, angle_window, angle_step)
    candidate_map: Dict[Tuple[int, int, int], Candidate] = {}
    for seed in seeds:
        base = seed.candidate
        for da in angle_offsets:
            if independent_x2:
                for dx1 in x_offsets:
                    for dx2 in x_offsets:
                        cand = Candidate(
                            x1_cm=round(base.x1_cm + dx1, 6),
                            x2_cm=round(base.x2_cm + dx2, 6),
                            angle_deg=round(base.angle_deg + da, 6),
                        )
                        key = (int(round(cand.x1_cm * 1000)), int(round(cand.x2_cm * 1000)), int(round(cand.angle_deg * 1000)))
                        candidate_map[key] = cand
            else:
                for dx in x_offsets:
                    cand = Candidate(
                        x1_cm=round(base.x1_cm + dx, 6),
                        x2_cm=round(base.x2_cm + dx, 6),
                        angle_deg=round(base.angle_deg + da, 6),
                    )
                    key = (int(round(cand.x1_cm * 1000)), int(round(cand.x2_cm * 1000)), int(round(cand.angle_deg * 1000)))
                    candidate_map[key] = cand
    return list(candidate_map.values())


def resolve_sim_bin(smsimdir: str, user_path: str) -> pathlib.Path:
    if user_path:
        p = pathlib.Path(expand_smsimdir(user_path, smsimdir))
        if p.exists():
            return p
    for candidate in [
        pathlib.Path(smsimdir) / "bin" / "sim_deuteron",
        pathlib.Path(smsimdir) / "build" / "bin" / "sim_deuteron",
    ]:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("Cannot find sim_deuteron executable. Use --sim-bin to specify path.")


def resolve_analyzer_bin(smsimdir: str, user_path: str) -> pathlib.Path:
    if user_path:
        p = pathlib.Path(expand_smsimdir(user_path, smsimdir))
        if p.exists():
            return p
    for candidate in [
        pathlib.Path(smsimdir) / "bin" / "analyze_pdc_scan_root",
        pathlib.Path(smsimdir) / "build" / "bin" / "analyze_pdc_scan_root",
    ]:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("Cannot find analyze_pdc_scan_root executable. Use --analyzer-bin to specify path.")


def run_candidate(
    stage: str,
    index: int,
    total: int,
    candidate: Candidate,
    sim_bin: pathlib.Path,
    analyzer_bin: pathlib.Path,
    smsimdir: str,
    outdir: pathlib.Path,
    field_macro: str,
    px_values: Sequence[float],
    pz_mevc: float,
    repeat_per_px: int,
    target_pos_cm: Tuple[float, float, float],
    target_angle_deg: float,
    pdc_z1_cm: float,
    pdc_z2_cm: float,
    dry_run: bool,
) -> CandidateResult:
    token = (
        f"{stage}_{index:04d}"
        f"_x1{format_token(candidate.x1_cm)}"
        f"_x2{format_token(candidate.x2_cm)}"
        f"_a{format_token(candidate.angle_deg)}"
    )
    stage_dir = outdir / stage
    macro_dir = stage_dir / "macros"
    sim_dir = stage_dir / "sim_root"
    analysis_dir = stage_dir / "analysis_detail"
    summary_dir = stage_dir / "summary_csv"
    log_dir = stage_dir / "logs"
    macro_dir.mkdir(parents=True, exist_ok=True)
    sim_dir.mkdir(parents=True, exist_ok=True)
    analysis_dir.mkdir(parents=True, exist_ok=True)
    summary_dir.mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    macro_path = macro_dir / f"{token}.mac"
    sim_root_pattern = f"{token}*.root"
    analysis_root = analysis_dir / f"{token}.detail.csv"
    summary_csv = summary_dir / f"{token}.csv"

    macro_text = build_macro_text(
        candidate=candidate,
        run_name=token,
        output_dir=sim_dir,
        field_macro=field_macro,
        target_pos_cm=target_pos_cm,
        target_angle_deg=target_angle_deg,
        pdc_z1_cm=pdc_z1_cm,
        pdc_z2_cm=pdc_z2_cm,
        px_values=px_values,
        pz_mevc=pz_mevc,
        repeat_per_px=repeat_per_px,
    )
    macro_path.write_text(macro_text, encoding="utf-8")

    print(
        f"[{stage}] {index + 1}/{total} "
        f"x1={candidate.x1_cm:.1f} cm x2={candidate.x2_cm:.1f} cm "
        f"angle={candidate.angle_deg:.2f} deg"
    )

    if dry_run:
        return CandidateResult(
            stage=stage,
            index=index,
            candidate=candidate,
            status="dry_run",
            run_name=token,
            macro_path=str(macro_path),
            sim_root="",
            analysis_root=str(analysis_root),
            summary_csv=str(summary_csv),
        )

    for old_file in sim_dir.glob(sim_root_pattern):
        old_file.unlink()

    try:
        run_command(
            [str(sim_bin), str(macro_path)],
            log_dir / f"{token}_sim.log",
            pathlib.Path(smsimdir),
        )
    except Exception as ex:
        return CandidateResult(
            stage=stage,
            index=index,
            candidate=candidate,
            status="sim_failed",
            run_name=token,
            macro_path=str(macro_path),
            sim_root="",
            analysis_root=str(analysis_root),
            summary_csv=str(summary_csv),
            error=str(ex),
        )

    sim_roots = sorted(sim_dir.glob(sim_root_pattern), key=lambda p: p.name)
    if not sim_roots:
        return CandidateResult(
            stage=stage,
            index=index,
            candidate=candidate,
            status="missing_sim_root",
            run_name=token,
            macro_path=str(macro_path),
            sim_root="",
            analysis_root=str(analysis_root),
            summary_csv=str(summary_csv),
            error=f"No ROOT output matching: {sim_root_pattern}",
        )
    sim_root_for_analysis = sim_roots[0]
    if len(sim_roots) > 1:
        merged_sim_root = analysis_dir / f"{token}.merged.root"
        if merged_sim_root.exists():
            merged_sim_root.unlink()
        try:
            run_command(
                ["hadd", "-f", str(merged_sim_root), *[str(p) for p in sim_roots]],
                log_dir / f"{token}_hadd.log",
                pathlib.Path(smsimdir),
            )
            sim_root_for_analysis = merged_sim_root
        except Exception as ex:
            return CandidateResult(
                stage=stage,
                index=index,
                candidate=candidate,
                status="merge_failed",
                run_name=token,
                macro_path=str(macro_path),
                sim_root="",
                analysis_root=str(analysis_root),
                summary_csv=str(summary_csv),
                error=str(ex),
            )

    try:
        if not px_values:
            raise RuntimeError("px_values is empty")
        px_min = px_values[0]
        px_max = px_values[-1]
        px_step = (px_values[1] - px_values[0]) if len(px_values) > 1 else 1.0
        if px_step <= 0:
            raise RuntimeError(f"Invalid px step derived from grid: {px_step}")

        run_command(
            [
                str(analyzer_bin),
                "--input",
                str(sim_root_for_analysis),
                "--summary",
                str(summary_csv),
                "--detail",
                str(analysis_root),
                "--px-min",
                f"{px_min:.6f}",
                "--px-max",
                f"{px_max:.6f}",
                "--px-step",
                f"{px_step:.6f}",
                "--repeat",
                str(repeat_per_px),
                "--pdc-angle-deg",
                f"{candidate.angle_deg:.6f}",
                "--pdc-z1-cm",
                f"{pdc_z1_cm:.6f}",
                "--pdc-z2-cm",
                f"{pdc_z2_cm:.6f}",
            ],
            log_dir / f"{token}_analysis.log",
            pathlib.Path(smsimdir),
            allow_success_if_outputs_exist=[summary_csv, analysis_root],
        )
    except Exception as ex:
        return CandidateResult(
            stage=stage,
            index=index,
            candidate=candidate,
            status="analysis_failed",
            run_name=token,
            macro_path=str(macro_path),
            sim_root=str(sim_root_for_analysis),
            analysis_root=str(analysis_root),
            summary_csv=str(summary_csv),
            error=str(ex),
        )

    if not summary_csv.exists():
        return CandidateResult(
            stage=stage,
            index=index,
            candidate=candidate,
            status="missing_summary_csv",
            run_name=token,
            macro_path=str(macro_path),
            sim_root=str(sim_root_for_analysis),
            analysis_root=str(analysis_root),
            summary_csv=str(summary_csv),
            error="Summary CSV was not generated.",
        )

    metrics = extract_metrics(summary_csv)
    return CandidateResult(
        stage=stage,
        index=index,
        candidate=candidate,
        status="ok",
        run_name=token,
        macro_path=str(macro_path),
        sim_root=str(sim_root_for_analysis),
        analysis_root=str(analysis_root),
        summary_csv=str(summary_csv),
        track_eff=get_metric_mean(metrics, "pdc_track_eff_vs_truth_proton"),
        eff_mean=get_metric_mean(metrics, "pdc_eff_mean_vs_px"),
        eff_std=get_metric_rms(metrics, "pdc_eff_mean_vs_px"),
        eff_cv=get_metric_mean(metrics, "pdc_eff_cv_vs_px"),
        edge_min=get_metric_mean(metrics, "pdc_eff_edge_min_abs130"),
        edge_mean=get_metric_mean(metrics, "pdc_eff_edge_mean_abs130"),
        incidence_mean_deg=get_metric_mean(metrics, "pdc_incidence_angle_deg"),
        incidence_rms_deg=get_metric_rms(metrics, "pdc_incidence_angle_deg"),
    )


def pick_best(results: Sequence[CandidateResult]) -> Optional[CandidateResult]:
    valid = [r for r in results if r.status == "ok" and math.isfinite(r.edge_min)]
    if not valid:
        return None
    return sorted(valid, key=candidate_sort_key)[0]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Scan PDC1/PDC2 x positions and PDC angle for B=1.15T, target angle=3deg using Geant4 validation."
    )
    parser.add_argument("--sim-bin", default="", help="Path to sim_deuteron executable")
    parser.add_argument("--analyzer-bin", default="", help="Path to analyze_pdc_scan_root executable")
    parser.add_argument("--outdir", default="{SMSIMDIR}/results/pdc_scan_B115T_3deg", help="Output directory")
    parser.add_argument("--field-macro", default="{SMSIMDIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T.mac")

    parser.add_argument("--target-x-cm", type=float, default=-1.2488)
    parser.add_argument("--target-y-cm", type=float, default=0.0009)
    parser.add_argument("--target-z-cm", type=float, default=-106.9458)
    parser.add_argument("--target-angle-deg", type=float, default=3.0)
    parser.add_argument("--pdc-z1-cm", type=float, default=400.0)
    parser.add_argument("--pdc-z2-cm", type=float, default=500.0)

    parser.add_argument("--x-min", type=float, default=-120.0)
    parser.add_argument("--x-max", type=float, default=120.0)
    parser.add_argument("--x-step", type=float, default=20.0)
    parser.add_argument("--angle-min", type=float, default=60.0)
    parser.add_argument("--angle-max", type=float, default=70.0)
    parser.add_argument("--angle-step", type=float, default=1.0)
    parser.add_argument("--independent-x2", action="store_true", help="Scan x2 independently from x1")
    parser.add_argument("--x2-min", type=float, default=None)
    parser.add_argument("--x2-max", type=float, default=None)
    parser.add_argument("--x2-step", type=float, default=None)

    parser.add_argument("--px-min", type=float, default=-150.0)
    parser.add_argument("--px-max", type=float, default=150.0)
    parser.add_argument("--px-step", type=float, default=10.0)
    parser.add_argument("--pz-mevc", type=float, default=627.0)
    parser.add_argument("--coarse-repeat-per-px", type=int, default=6)
    parser.add_argument("--full-repeat-per-px", type=int, default=20)

    parser.add_argument("--top-n", type=int, default=8)
    parser.add_argument("--fine-x-window", type=float, default=10.0)
    parser.add_argument("--fine-x-step", type=float, default=5.0)
    parser.add_argument("--fine-angle-window", type=float, default=1.0)
    parser.add_argument("--fine-angle-step", type=float, default=0.5)
    parser.add_argument("--skip-fine", action="store_true")

    parser.add_argument("--max-coarse-candidates", type=int, default=0, help="0 means no limit")
    parser.add_argument("--max-fine-candidates", type=int, default=0, help="0 means no limit")

    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    smsimdir = os.environ.get("SMSIMDIR", str(pathlib.Path(__file__).resolve().parents[2]))
    outdir = pathlib.Path(expand_smsimdir(args.outdir, smsimdir)).resolve()
    outdir.mkdir(parents=True, exist_ok=True)

    sim_bin = resolve_sim_bin(smsimdir, args.sim_bin)
    analyzer_bin = resolve_analyzer_bin(smsimdir, args.analyzer_bin)
    field_macro = expand_smsimdir(args.field_macro, smsimdir)

    if not args.dry_run:
        for must_exist in [sim_bin, analyzer_bin]:
            if not pathlib.Path(must_exist).exists():
                raise FileNotFoundError(f"Required file not found: {must_exist}")

    logs_dir = outdir / "logs"
    logs_dir.mkdir(parents=True, exist_ok=True)

    start_ts = time.time()

    print(f"[scan] SMSIMDIR: {smsimdir}")
    print(f"[scan] sim_deuteron: {sim_bin}")
    print(f"[scan] analyzer: {analyzer_bin}")
    print(f"[scan] output: {outdir}")
    px_values = arange_inclusive(args.px_min, args.px_max, args.px_step)
    print(
        f"[scan] proton grid: px=[{args.px_min:.1f},{args.px_max:.1f}] step={args.px_step:.1f} MeV/c, "
        f"pz={args.pz_mevc:.1f} MeV/c, points={len(px_values)}"
    )

    x_values = arange_inclusive(args.x_min, args.x_max, args.x_step)
    angle_values = arange_inclusive(args.angle_min, args.angle_max, args.angle_step)
    if args.independent_x2:
        x2_min = args.x2_min if args.x2_min is not None else args.x_min
        x2_max = args.x2_max if args.x2_max is not None else args.x_max
        x2_step = args.x2_step if args.x2_step is not None else args.x_step
        x2_values = arange_inclusive(x2_min, x2_max, x2_step)
    else:
        x2_values = x_values

    coarse_candidates = generate_candidates(
        x_values=x_values,
        angle_values=angle_values,
        independent_x2=args.independent_x2,
        x2_values=x2_values,
    )
    if args.max_coarse_candidates > 0:
        coarse_candidates = coarse_candidates[: args.max_coarse_candidates]
    print(f"[scan] coarse candidates: {len(coarse_candidates)}")

    target_pos_cm = (args.target_x_cm, args.target_y_cm, args.target_z_cm)
    coarse_repeat = max(1, args.coarse_repeat_per_px)
    full_repeat = max(1, args.full_repeat_per_px)

    coarse_results: List[CandidateResult] = []
    for idx, cand in enumerate(coarse_candidates):
        coarse_results.append(
            run_candidate(
                stage="coarse",
                index=idx,
                total=len(coarse_candidates),
                candidate=cand,
                sim_bin=sim_bin,
                analyzer_bin=analyzer_bin,
                smsimdir=smsimdir,
                outdir=outdir,
                field_macro=field_macro,
                px_values=px_values,
                pz_mevc=args.pz_mevc,
                repeat_per_px=coarse_repeat,
                target_pos_cm=target_pos_cm,
                target_angle_deg=args.target_angle_deg,
                pdc_z1_cm=args.pdc_z1_cm,
                pdc_z2_cm=args.pdc_z2_cm,
                dry_run=args.dry_run,
            )
        )

    write_results_csv(outdir / "coarse_results.csv", coarse_results)
    coarse_valid = sorted([r for r in coarse_results if r.status == "ok"], key=candidate_sort_key)
    coarse_top = coarse_valid[: args.top_n]

    print(f"[scan] coarse valid: {len(coarse_valid)}")
    if coarse_top:
        best = coarse_top[0]
        print(
            "[scan] coarse best: "
            f"x1={best.candidate.x1_cm:.1f} x2={best.candidate.x2_cm:.1f} "
            f"angle={best.candidate.angle_deg:.2f} edge_min={best.edge_min:.4f} "
            f"cv={best.eff_cv:.4f} incidence_rms={best.incidence_rms_deg:.3f}"
        )

    final_results: List[CandidateResult] = []
    if args.skip_fine or not coarse_top:
        final_results = coarse_results
    else:
        fine_candidates = build_fine_candidates(
            seeds=coarse_top,
            independent_x2=args.independent_x2,
            x_window=args.fine_x_window,
            x_step=args.fine_x_step,
            angle_window=args.fine_angle_window,
            angle_step=args.fine_angle_step,
        )
        if args.max_fine_candidates > 0:
            fine_candidates = fine_candidates[: args.max_fine_candidates]
        print(f"[scan] fine candidates: {len(fine_candidates)}")

        for idx, cand in enumerate(fine_candidates):
            final_results.append(
                run_candidate(
                    stage="fine",
                    index=idx,
                    total=len(fine_candidates),
                    candidate=cand,
                    sim_bin=sim_bin,
                    analyzer_bin=analyzer_bin,
                    smsimdir=smsimdir,
                    outdir=outdir,
                    field_macro=field_macro,
                    px_values=px_values,
                    pz_mevc=args.pz_mevc,
                    repeat_per_px=full_repeat,
                    target_pos_cm=target_pos_cm,
                    target_angle_deg=args.target_angle_deg,
                    pdc_z1_cm=args.pdc_z1_cm,
                    pdc_z2_cm=args.pdc_z2_cm,
                    dry_run=args.dry_run,
                )
            )

        write_results_csv(outdir / "fine_results.csv", final_results)

    if not (outdir / "fine_results.csv").exists():
        write_results_csv(outdir / "fine_results.csv", final_results)

    best_final = pick_best(final_results)
    best_coarse = pick_best(coarse_results)
    best = best_final if best_final is not None else best_coarse

    elapsed = time.time() - start_ts
    print(f"[scan] elapsed: {elapsed:.1f} s")

    best_txt = outdir / "best_candidate.txt"
    with best_txt.open("w", encoding="utf-8") as f:
        if best is None:
            f.write("status=not_found\n")
            if args.dry_run:
                print("[scan] dry-run finished (no Geant4 execution, no ranking metrics).")
                return 0
            print("[scan] no valid candidate found.")
            return 2
        f.write(f"stage={best.stage}\n")
        f.write(f"run_name={best.run_name}\n")
        f.write(f"x1_cm={best.candidate.x1_cm:.6f}\n")
        f.write(f"x2_cm={best.candidate.x2_cm:.6f}\n")
        f.write(f"angle_deg={best.candidate.angle_deg:.6f}\n")
        f.write(f"edge_min={best.edge_min:.6f}\n")
        f.write(f"edge_mean={best.edge_mean:.6f}\n")
        f.write(f"eff_cv={best.eff_cv:.6f}\n")
        f.write(f"track_eff={best.track_eff:.6f}\n")
        f.write(f"incidence_mean_deg={best.incidence_mean_deg:.6f}\n")
        f.write(f"incidence_rms_deg={best.incidence_rms_deg:.6f}\n")
        f.write(f"macro_path={best.macro_path}\n")
        f.write(f"sim_root={best.sim_root}\n")
        f.write(f"summary_csv={best.summary_csv}\n")

    print(
        "[scan] best candidate: "
        f"x1={best.candidate.x1_cm:.2f} cm, "
        f"x2={best.candidate.x2_cm:.2f} cm, "
        f"angle={best.candidate.angle_deg:.2f} deg, "
        f"edge_min={best.edge_min:.4f}, cv={best.eff_cv:.4f}, "
        f"incidence_rms={best.incidence_rms_deg:.3f} deg"
    )
    print(f"[scan] best summary: {best.summary_csv}")
    print(f"[scan] best macro: {best.macro_path}")
    print(f"[scan] report: {best_txt}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
