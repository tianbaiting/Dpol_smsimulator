from __future__ import annotations

import argparse
import csv
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

import matplotlib.pyplot as plt
import numpy as np

from core.features import compute_derived
from core.io import iter_ypol_phi_random, iter_zpol_b_discrete, iter_ypol_20260413
from core.paths import default_data_root
from core.schema import Event


@dataclass(frozen=True)
class RatioResult:
    x: float
    n_pos: int
    n_neg: int
    r: Optional[float]


def _safe_ratio(n_pos: int, n_neg: int) -> Optional[float]:
    if n_neg == 0:
        return None
    return n_pos / n_neg


def _unphysical_cut(dataset: str, event: Event, delta_limit: float) -> bool:
    # [EN] Remove unphysical breakup by constraining the relative momentum on the polarization axis / [CN] 通过极化轴相对动量约束去除非物理破裂
    if dataset == "ypol":
        return abs(event.pyp - event.pyn) < delta_limit
    if dataset == "zpol":
        return abs(event.pzp - event.pzn) < delta_limit
    raise ValueError(f"Unknown dataset: {dataset}")


def _collect_events(
    dataset: str,
    data_root: Path,
    target: str,
    energy: str,
    gammas: Iterable[str],
    delta_limit: float,
    b_min: int,
    b_max: int,
    bmax_event_filter: int,
    data_layout: str = "default",
) -> Dict[str, List[Tuple[float, float, float, float, float]]]:
    rows: Dict[str, List[Tuple[float, float, float, float, float]]] = {}
    for gamma in gammas:
        selected: List[Tuple[float, float, float, float, float]] = []
        if dataset == "ypol":
            ypol_iter_fn = iter_ypol_20260413 if data_layout == "20260413ypol" else iter_ypol_phi_random
            iterator = ypol_iter_fn(
                root=data_root,
                target=target,
                energy=energy,
                gamma=gamma,
                pols=["ynp", "ypn"],
            )
        elif dataset == "zpol":
            iterator = iter_zpol_b_discrete(
                root=data_root,
                target=target,
                energy=energy,
                gamma=gamma,
                pols=["znp", "zpn"],
                b_min=b_min,
                b_max=b_max,
                bmax_for_event_filter=bmax_event_filter,
            )
        else:
            raise ValueError(f"Unknown dataset: {dataset}")

        for event in iterator:
            if event.b is None:
                continue
            if not _unphysical_cut(dataset, event, delta_limit):
                continue
            d = compute_derived(event)
            delta_px = d.rot_pxp - d.rot_pxn
            sum_px = d.rot_pxp + d.rot_pxn
            selected.append((event.b, sum_px, delta_px, d.rot_pxp, d.rot_pxn))
        rows[gamma] = selected
    return rows


def _calc_r_vs_b(
    rows_by_gamma: Dict[str, List[Tuple[float, float, float, float, float]]],
    b_edges: np.ndarray,
) -> Dict[str, List[RatioResult]]:
    out: Dict[str, List[RatioResult]] = {}
    for gamma, rows in rows_by_gamma.items():
        arr = np.array(rows, dtype=float) if rows else np.empty((0, 5), dtype=float)
        results: List[RatioResult] = []
        for i in range(len(b_edges) - 1):
            lo = b_edges[i]
            hi = b_edges[i + 1]
            center = 0.5 * (lo + hi)
            if arr.size == 0:
                results.append(RatioResult(center, 0, 0, None))
                continue
            mask = (arr[:, 0] >= lo) & (arr[:, 0] < hi)
            sample = arr[mask, 2]
            n_pos = int(np.sum(sample > 0.0))
            n_neg = int(np.sum(sample < 0.0))
            results.append(RatioResult(center, n_pos, n_neg, _safe_ratio(n_pos, n_neg)))
        out[gamma] = results
    return out


def _calc_r_vs_sum_px(
    rows_by_gamma: Dict[str, List[Tuple[float, float, float, float, float]]],
    px_edges: np.ndarray,
) -> Dict[str, List[RatioResult]]:
    out: Dict[str, List[RatioResult]] = {}
    for gamma, rows in rows_by_gamma.items():
        arr = np.array(rows, dtype=float) if rows else np.empty((0, 5), dtype=float)
        results: List[RatioResult] = []
        for i in range(len(px_edges) - 1):
            lo = px_edges[i]
            hi = px_edges[i + 1]
            center = 0.5 * (lo + hi)
            if arr.size == 0:
                results.append(RatioResult(center, 0, 0, None))
                continue
            mask = (arr[:, 1] >= lo) & (arr[:, 1] < hi)
            sample = arr[mask, 2]
            n_pos = int(np.sum(sample > 0.0))
            n_neg = int(np.sum(sample < 0.0))
            results.append(RatioResult(center, n_pos, n_neg, _safe_ratio(n_pos, n_neg)))
        out[gamma] = results
    return out


def _write_ratio_csv(path: Path, x_name: str, ratio_map: Dict[str, List[RatioResult]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["gamma", x_name, "n_pos", "n_neg", "R"])
        for gamma in sorted(ratio_map.keys()):
            for row in ratio_map[gamma]:
                writer.writerow([gamma, f"{row.x:.6f}", row.n_pos, row.n_neg, "" if row.r is None else f"{row.r:.8f}"])


def _plot_ratio_map(
    ratio_map: Dict[str, List[RatioResult]],
    x_label: str,
    y_label: str,
    title: str,
    out_png: Path,
    y_min: Optional[float] = None,
    y_max: Optional[float] = None,
) -> None:
    out_png.parent.mkdir(parents=True, exist_ok=True)
    plt.figure(figsize=(8, 5))
    for gamma in sorted(ratio_map.keys()):
        xs: List[float] = []
        ys: List[float] = []
        for row in ratio_map[gamma]:
            if row.r is None:
                continue
            xs.append(row.x)
            ys.append(row.r)
        if xs:
            plt.plot(xs, ys, marker="o", linewidth=1.5, markersize=4, label=f"gamma={gamma}")
    plt.xlabel(x_label)
    plt.ylabel(y_label)
    plt.title(title)
    if y_min is not None and y_max is not None:
        plt.ylim(y_min, y_max)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_png, dpi=180)
    plt.close()


def _global_summary(
    rows_by_gamma: Dict[str, List[Tuple[float, float, float, float, float]]]
) -> Dict[str, Dict[str, float]]:
    summary: Dict[str, Dict[str, float]] = {}
    for gamma, rows in rows_by_gamma.items():
        arr = np.array(rows, dtype=float) if rows else np.empty((0, 5), dtype=float)
        if arr.size == 0:
            summary[gamma] = {"n_total": 0, "n_pos": 0, "n_neg": 0, "R_global": float("nan")}
            continue
        delta = arr[:, 2]
        n_pos = int(np.sum(delta > 0.0))
        n_neg = int(np.sum(delta < 0.0))
        summary[gamma] = {
            "n_total": int(delta.size),
            "n_pos": n_pos,
            "n_neg": n_neg,
            "R_global": float("nan") if n_neg == 0 else float(n_pos / n_neg),
        }
    return summary


def _calc_r_vs_particle_px(
    rows_by_gamma: Dict[str, List[Tuple[float, float, float, float, float]]],
    px_edges: np.ndarray,
    particle: str,
) -> Dict[str, List[RatioResult]]:
    if particle == "proton":
        col = 3
    elif particle == "neutron":
        col = 4
    else:
        raise ValueError(f"Unknown particle: {particle}")

    out: Dict[str, List[RatioResult]] = {}
    for gamma, rows in rows_by_gamma.items():
        arr = np.array(rows, dtype=float) if rows else np.empty((0, 5), dtype=float)
        results: List[RatioResult] = []
        for i in range(len(px_edges) - 1):
            lo = px_edges[i]
            hi = px_edges[i + 1]
            center = 0.5 * (lo + hi)
            if arr.size == 0:
                results.append(RatioResult(center, 0, 0, None))
                continue
            mask = (arr[:, col] >= lo) & (arr[:, col] < hi)
            sample = arr[mask, 2]
            n_pos = int(np.sum(sample > 0.0))
            n_neg = int(np.sum(sample < 0.0))
            results.append(RatioResult(center, n_pos, n_neg, _safe_ratio(n_pos, n_neg)))
        out[gamma] = results
    return out


def _sensitivity_summary(
    ratio_map: Dict[str, List[RatioResult]],
    min_stats: int,
    bin_width: float,
) -> Dict[str, object]:
    bins: Dict[float, Dict[str, object]] = {}
    for gamma in sorted(ratio_map.keys()):
        for row in ratio_map[gamma]:
            item = bins.setdefault(
                row.x,
                {"x_center": row.x, "n_total": 0, "r_values": []},
            )
            item["n_total"] = int(item["n_total"]) + row.n_pos + row.n_neg
            if row.r is not None:
                item["r_values"].append(float(row.r))

    scored: List[Dict[str, float]] = []
    for _, item in bins.items():
        r_values = item["r_values"]
        if len(r_values) < 2:
            continue
        spread = float(max(r_values) - min(r_values))
        n_total = int(item["n_total"])
        if n_total < min_stats:
            continue
        scored.append(
            {
                "x_center": float(item["x_center"]),
                "x_low": float(item["x_center"] - 0.5 * bin_width),
                "x_high": float(item["x_center"] + 0.5 * bin_width),
                "spread": spread,
                "n_total": float(n_total),
            }
        )

    scored.sort(key=lambda x: x["spread"], reverse=True)
    return {
        "best_bin": scored[0] if scored else None,
        "top5": scored[:5],
    }


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze R(b) and R(sum Px) after unphysical-event cut.")
    parser.add_argument("--data-root", type=str, default=None)
    parser.add_argument("--output-dir", type=str, default="output/r_observable")
    parser.add_argument("--target", type=str, default="Pb208")
    parser.add_argument("--energy", type=str, default="190")
    parser.add_argument("--gammas", nargs="+", default=["050", "060", "070", "080"])
    parser.add_argument("--delta-limit", type=float, default=150.0)
    parser.add_argument("--b-min", type=int, default=1)
    parser.add_argument("--b-max", type=int, default=12)
    parser.add_argument("--bmax-event-filter", type=int, default=12)
    parser.add_argument("--sum-px-min", type=float, default=0.0)
    parser.add_argument("--sum-px-max", type=float, default=300.0)
    parser.add_argument("--sum-px-bins", type=int, default=12)
    parser.add_argument("--particle-px-min", type=float, default=-250.0)
    parser.add_argument("--particle-px-max", type=float, default=250.0)
    parser.add_argument("--particle-px-bins", type=int, default=20)
    parser.add_argument("--sensitivity-min-stats", type=int, default=2000)
    parser.add_argument("--data-layout", choices=["default", "20260413ypol"], default="default",
                        help="Directory layout for ypol data")
    parser.add_argument("--dataset", choices=["ypol", "zpol", "both"], default="both",
                        help="Which dataset(s) to process")
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    data_root = Path(args.data_root) if args.data_root else default_data_root()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    b_edges = np.arange(args.b_min, args.b_max + 2, dtype=float)
    px_edges = np.linspace(args.sum_px_min, args.sum_px_max, args.sum_px_bins + 1, dtype=float)
    particle_px_edges = np.linspace(args.particle_px_min, args.particle_px_max, args.particle_px_bins + 1, dtype=float)
    particle_bin_width = float((args.particle_px_max - args.particle_px_min) / args.particle_px_bins)

    all_results: Dict[str, Dict[str, object]] = {}
    datasets = ["ypol", "zpol"] if args.dataset == "both" else [args.dataset]
    for dataset in datasets:
        rows = _collect_events(
            dataset=dataset,
            data_root=data_root,
            target=args.target,
            energy=args.energy,
            gammas=args.gammas,
            delta_limit=args.delta_limit,
            b_min=args.b_min,
            b_max=args.b_max,
            bmax_event_filter=args.bmax_event_filter,
            data_layout=args.data_layout,
        )
        ratio_vs_b = _calc_r_vs_b(rows, b_edges)
        ratio_vs_sum_px = _calc_r_vs_sum_px(rows, px_edges)
        ratio_vs_pxp = _calc_r_vs_particle_px(rows, particle_px_edges, "proton")
        ratio_vs_pxn = _calc_r_vs_particle_px(rows, particle_px_edges, "neutron")
        summary = _global_summary(rows)
        sensitivity = {
            "proton_px": _sensitivity_summary(ratio_vs_pxp, args.sensitivity_min_stats, particle_bin_width),
            "neutron_px": _sensitivity_summary(ratio_vs_pxn, args.sensitivity_min_stats, particle_bin_width),
        }

        ds_out = output_dir / dataset / args.target
        ds_out.mkdir(parents=True, exist_ok=True)

        _write_ratio_csv(ds_out / "R_vs_b.csv", "b_center", ratio_vs_b)
        _write_ratio_csv(ds_out / "R_vs_sum_px.csv", "sum_px_center", ratio_vs_sum_px)
        _write_ratio_csv(ds_out / "R_vs_pxp.csv", "pxp_center", ratio_vs_pxp)
        _write_ratio_csv(ds_out / "R_vs_pxn.csv", "pxn_center", ratio_vs_pxn)

        _plot_ratio_map(
            ratio_map=ratio_vs_b,
            x_label="b (fm, bin center)",
            y_label="R = N(Px_p > Px_n) / N(Px_p < Px_n)",
            title=f"{dataset.upper()} target={args.target} | R vs b (after unphysical cut)",
            out_png=ds_out / "R_vs_b.png",
        )
        _plot_ratio_map(
            ratio_map=ratio_vs_b,
            x_label="b (fm, bin center)",
            y_label="R = N(Px_p > Px_n) / N(Px_p < Px_n)",
            title=f"{dataset.upper()} target={args.target} | R vs b (after unphysical cut), y in [0,3]",
            out_png=ds_out / "R_vs_b_y03.png",
            y_min=0.0,
            y_max=3.0,
        )
        _plot_ratio_map(
            ratio_map=ratio_vs_sum_px,
            x_label="P_x^tot in reaction plane (MeV/c, bin center)",
            y_label="R = N(Px_p > Px_n) / N(Px_p < Px_n)",
            title=f"{dataset.upper()} target={args.target} | R vs P_x^tot (after unphysical cut)",
            out_png=ds_out / "R_vs_sum_px.png",
        )
        _plot_ratio_map(
            ratio_map=ratio_vs_pxp,
            x_label="Proton p_x in reaction plane (MeV/c, bin center)",
            y_label="R = N(Px_p > Px_n) / N(Px_p < Px_n)",
            title=f"{dataset.upper()} target={args.target} | R vs proton p_x (after unphysical cut)",
            out_png=ds_out / "R_vs_pxp.png",
        )
        _plot_ratio_map(
            ratio_map=ratio_vs_pxn,
            x_label="Neutron p_x in reaction plane (MeV/c, bin center)",
            y_label="R = N(Px_p > Px_n) / N(Px_p < Px_n)",
            title=f"{dataset.upper()} target={args.target} | R vs neutron p_x (after unphysical cut)",
            out_png=ds_out / "R_vs_pxn.png",
        )
        _plot_ratio_map(
            ratio_map=ratio_vs_pxn,
            x_label="Neutron p_x in reaction plane (MeV/c, bin center)",
            y_label="R = N(Px_p > Px_n) / N(Px_p < Px_n)",
            title=f"{dataset.upper()} target={args.target} | R vs neutron p_x (after unphysical cut), y in [0,3]",
            out_png=ds_out / "R_vs_pxn_y03.png",
            y_min=0.0,
            y_max=3.0,
        )

        with (ds_out / "sensitivity_summary.json").open("w", encoding="utf-8") as f:
            json.dump(sensitivity, f, indent=2)

        all_results[dataset] = {
            "summary": summary,
            "b_edges": b_edges.tolist(),
            "sum_px_edges": px_edges.tolist(),
            "particle_px_edges": particle_px_edges.tolist(),
            "sensitivity": sensitivity,
        }

    with (output_dir / "summary.json").open("w", encoding="utf-8") as f:
        json.dump(all_results, f, indent=2)

    print(f"analysis done: {output_dir}")


if __name__ == "__main__":
    main()
