#   如何运行
#   python3 run_analysis.py --dataset ypol --target Pb208

#   输出位置

#   - output/{dataset}/{target}/{cut}/gamma_{gamma}/
#   - HTML：3D 图
#   - PNG：2D 图

#   你最可能改的地方

#   - cut 逻辑：core/cuts.py
#   - 读数据路径或能量：run_analysis.py 参数 --energy
#   - 输出目录：--output-dir
#   - 3D 采样数量：--max-3d
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
import random
from typing import Dict, Iterable, List, Tuple, Optional

from core.accumulators import Accumulator
from core.cuts import CUTS
from core.features import compute_derived
from core.io import iter_ypol_phi_random, iter_zpol_b_discrete
from core.paths import default_data_root
from plots.plot_2d import (
    plot_phi_corr_raw,
    plot_pxp_pxn_corr_raw,
    plot_pxn_minus_pxp,
    plot_pxn_pxp_corr,
)
from plots.plot_ratio import plot_ratio_vs_gamma
from plots.plot_3d import plot_3d_delta, plot_3d_proton_neutron


def _require_plot_dependencies() -> None:
    try:
        import matplotlib  # noqa: F401
    except Exception as exc:
        print("Missing dependency: matplotlib")
        print("Install with: python3 -m pip install matplotlib")
        raise SystemExit(1) from exc
    try:
        import plotly  # noqa: F401
    except Exception as exc:
        print("Missing dependency: plotly")
        print("Install with: python3 -m pip install plotly")
        raise SystemExit(1) from exc


def _gamma_to_value(gamma: str) -> float:
    try:
        return int(gamma) / 100.0
    except ValueError:
        return float(gamma)


def _compute_ratio_from_rotated(pxn_minus_pxp: List[float]) -> Optional[float]:
    pos = sum(1 for v in pxn_minus_pxp if v < 0.0)
    neg = sum(1 for v in pxn_minus_pxp if v > 0.0)
    if neg == 0:
        return None
    return pos / neg


def _ratio_plot_payload(
    gammas: List[str],
    ratio_summary: Dict[str, Dict[str, Optional[float]]],
    cut_names: List[str],
) -> Dict[str, List[Tuple[float, float]]]:
    payload: Dict[str, List[Tuple[float, float]]] = {}
    for cut in cut_names:
        points: List[Tuple[float, float]] = []
        for gamma in gammas:
            ratio = ratio_summary[cut].get(gamma)
            if ratio is None:
                continue
            points.append((_gamma_to_value(gamma), ratio))
        points.sort(key=lambda pair: pair[0])
        payload[cut] = points
    return payload


def _build_store(cut_names: Iterable[str], gammas: Iterable[str], max_points_3d: int) -> Dict[str, Dict[str, Accumulator]]:
    rng = random.Random(12345)
    store: Dict[str, Dict[str, Accumulator]] = {}
    for cut in cut_names:
        store[cut] = {}
        for gamma in gammas:
            store[cut][gamma] = Accumulator(max_points_3d, random.Random(rng.randint(0, 10**9)))
    return store


def _process_ypol(
    data_root: Path,
    target: str,
    energy: str,
    gammas: List[str],
    pols: List[str],
    cut_names: List[str],
    max_points_3d: int,
) -> Dict[str, Dict[str, Accumulator]]:
    cut_funcs = {name: CUTS["ypol"][name] for name in cut_names}
    store = _build_store(cut_names, gammas, max_points_3d)

    for gamma in gammas:
        for event in iter_ypol_phi_random(data_root, target, energy, gamma, pols):
            d = compute_derived(event)
            for cut_name, fn in cut_funcs.items():
                if fn(event, d):
                    store[cut_name][gamma].add(event, d)
        print(f"[ypol] gamma={gamma} done")

    return store


def _process_zpol(
    data_root: Path,
    target: str,
    energy: str,
    gammas: List[str],
    pols: List[str],
    cut_names: List[str],
    max_points_3d: int,
    b_min: int,
    b_max: int,
    bmax_for_event_filter: int,
) -> Dict[str, Dict[str, Accumulator]]:
    cut_funcs = {name: CUTS["zpol"][name] for name in cut_names}
    store = _build_store(cut_names, gammas, max_points_3d)

    for gamma in gammas:
        for event in iter_zpol_b_discrete(
            data_root,
            target,
            energy,
            gamma,
            pols,
            b_min,
            b_max,
            bmax_for_event_filter,
        ):
            d = compute_derived(event)
            for cut_name, fn in cut_funcs.items():
                if fn(event, d):
                    store[cut_name][gamma].add(event, d)
        print(f"[zpol] gamma={gamma} done")

    return store


def _render_outputs(
    dataset: str,
    target: str,
    energy: str,
    gammas: List[str],
    cut_names: List[str],
    store: Dict[str, Dict[str, Accumulator]],
    output_dir: Path,
) -> None:
    summary: Dict[str, Dict[str, int]] = {}
    ratio_summary: Dict[str, Dict[str, float]] = {}

    for cut_name in cut_names:
        summary[cut_name] = {}
        ratio_summary[cut_name] = {}
        for gamma in gammas:
            acc = store[cut_name][gamma]
            summary[cut_name][gamma] = acc.count
            ratio_summary[cut_name][gamma] = _compute_ratio_from_rotated(acc.pxn_minus_pxp)
            if acc.count == 0:
                continue

            base = output_dir / dataset / target / cut_name / f"gamma_{gamma}"
            title_prefix = f"{dataset.upper()} {target} E{energy} gamma={gamma} cut={cut_name}"

            plot_3d_proton_neutron(
                acc.proton_3d.data,
                acc.neutron_3d.data,
                title=f"{title_prefix} | Proton/Neutron 3D",
                output_html=base / "momentum_3d_proton_neutron.html",
            )

            plot_3d_delta(
                acc.delta_3d.data,
                title=f"{title_prefix} | Delta 3D",
                output_html=base / "momentum_3d_delta.html",
            )

            plot_pxn_pxp_corr(
                acc.pxp_rot,
                acc.pxn_rot,
                title=f"{title_prefix} | Pxn vs Pxp (RP)",
                output_png=base / "pxn_vs_pxp.png",
            )

            plot_pxn_minus_pxp(
                acc.pxn_minus_pxp,
                title=f"{title_prefix} | Pxn - Pxp (RP)",
                output_png=base / "pxn_minus_pxp.png",
            )

            plot_pxp_pxn_corr_raw(
                acc.pxp_raw,
                acc.pxn_raw,
                title=f"{title_prefix} | Pxp vs Pxn (raw)",
                output_png=base / "pxp_vs_pxn_raw.png",
            )

            plot_phi_corr_raw(
                acc.phi_p_raw,
                acc.phi_n_raw,
                title=f"{title_prefix} | phi_p vs phi_n (raw)",
                output_png=base / "phi_p_vs_phi_n_raw.png",
            )

    summary_path = output_dir / dataset / target / "summary.json"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text(json.dumps(summary, indent=2))

    ratio_path = output_dir / dataset / target / "ratio_summary.json"
    ratio_path.write_text(json.dumps(ratio_summary, indent=2))

    ratio_by_cut = _ratio_plot_payload(gammas, ratio_summary, cut_names)
    plot_ratio_vs_gamma(
        ratio_by_cut=ratio_by_cut,
        title=f"{dataset.upper()} {target} E{energy} | ratio vs gamma",
        output_png=output_dir / dataset / target / "ratio_vs_gamma.png",
    )


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="QMD raw data analysis")
    parser.add_argument("--dataset", choices=["ypol", "zpol", "both"], default="both")
    parser.add_argument("--data-root", type=str, default=None)
    parser.add_argument("--target", type=str, default="Pb208")
    parser.add_argument("--energy", type=str, default="190")
    parser.add_argument("--gammas", nargs="+", default=["050", "060", "070", "080"])
    parser.add_argument("--cuts", nargs="+", default=["loose", "mid", "tight"])
    parser.add_argument("--max-3d", type=int, default=20000)
    parser.add_argument("--b-min", type=int, default=5)
    parser.add_argument("--b-max", type=int, default=10)
    parser.add_argument("--bmax-event-filter", type=int, default=10)
    parser.add_argument("--output-dir", type=str, default="output")
    return parser.parse_args()


def main() -> None:
    _require_plot_dependencies()
    args = _parse_args()

    data_root = Path(args.data_root) if args.data_root else default_data_root()
    if not data_root.exists():
        print(f"data root not found: {data_root}")
        sys.exit(1)

    output_dir = Path(args.output_dir)
    gammas = list(args.gammas)
    cut_names = list(args.cuts)

    for dataset in ("ypol", "zpol"):
        if args.dataset != "both" and dataset != args.dataset:
            continue
        for cut_name in cut_names:
            if cut_name not in CUTS[dataset]:
                print(f"unknown cut '{cut_name}' for dataset '{dataset}'")
                sys.exit(1)

    if args.dataset in ("ypol", "both"):
        store = _process_ypol(
            data_root,
            args.target,
            args.energy,
            gammas,
            pols=["ynp", "ypn"],
            cut_names=cut_names,
            max_points_3d=args.max_3d,
        )
        _render_outputs(
            dataset="ypol",
            target=args.target,
            energy=args.energy,
            gammas=gammas,
            cut_names=cut_names,
            store=store,
            output_dir=output_dir,
        )

    if args.dataset in ("zpol", "both"):
        store = _process_zpol(
            data_root,
            args.target,
            args.energy,
            gammas,
            pols=["znp", "zpn"],
            cut_names=cut_names,
            max_points_3d=args.max_3d,
            b_min=args.b_min,
            b_max=args.b_max,
            bmax_for_event_filter=args.bmax_event_filter,
        )
        _render_outputs(
            dataset="zpol",
            target=args.target,
            energy=args.energy,
            gammas=gammas,
            cut_names=cut_names,
            store=store,
            output_dir=output_dir,
        )


if __name__ == "__main__":
    main()
