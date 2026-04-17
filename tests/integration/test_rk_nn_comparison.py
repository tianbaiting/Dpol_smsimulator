#!/usr/bin/env python3
"""
[EN] Compare RK and NN reconstruction accuracy from E2E evaluation CSVs.
[CN] 从E2E评估CSV文件比较RK和NN重建精度。

Reads the evaluation summary CSVs produced by the E2E tests and prints
a side-by-side comparison table. This script does NOT invent numbers —
every value comes directly from the test output files.

Usage:
    python3 test_rk_nn_comparison.py <test_output_dir>

Example:
    python3 test_rk_nn_comparison.py build/test_output/pdc_target_momentum_scan
"""

import csv
import os
import sys
from pathlib import Path


def read_eval_events(csv_path: str) -> list[dict]:
    """Read per-event evaluation CSV."""
    rows = []
    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def read_error_analysis_summary(csv_path: str) -> dict[str, str]:
    """Read metric,value summary CSV."""
    result = {}
    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            result[row["metric"]] = row["value"]
    return result


def read_error_analysis_tracks(csv_path: str) -> list[dict]:
    """Read track_summary.csv from error analysis."""
    rows = []
    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def compute_stats(events: list[dict]) -> dict:
    """Compute MAE, bias, RMSE for px, py, pz from per-event data."""
    n = len(events)
    if n == 0:
        return {}
    stats = {}
    for comp in ["px", "py", "pz"]:
        deltas = [float(e[f"d{comp}"]) for e in events]
        abs_deltas = [abs(d) for d in deltas]
        stats[f"bias_{comp}"] = sum(deltas) / n
        stats[f"mae_{comp}"] = sum(abs_deltas) / n
        stats[f"rmse_{comp}"] = (sum(d**2 for d in deltas) / n) ** 0.5
    return stats


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <test_output_dir>")
        sys.exit(1)

    base_dir = Path(sys.argv[1])

    # --- Read E2E evaluation data ---
    backends = {}
    eval_dirs = {
        "NN": base_dir / "nn" / "eval" / "events_nn.csv",
        "RK (fixed-target)": base_dir / "rk_fixed_target_pdc_only" / "eval" / "events_rk_fixed_target_pdc_only.csv",
        "RK (two-point)": base_dir / "rk_two_point_backprop" / "eval" / "events_rk_two_point_backprop.csv",
        "RK (three-point)": base_dir / "rk_three_point_free" / "eval" / "events_rk_three_point_free.csv",
    }
    for name, path in eval_dirs.items():
        if path.exists():
            backends[name] = read_eval_events(str(path))
        else:
            print(f"  SKIP: {name} — {path} not found")

    if not backends:
        print("ERROR: no evaluation CSVs found")
        sys.exit(1)

    # --- Print per-event comparison table ---
    print("=" * 100)
    print("E2E Reconstruction Accuracy: Per-event comparison (from actual test output)")
    print("=" * 100)
    print()

    for name, events in backends.items():
        print(f"--- {name} ---")
        print(f"  {'truth_px':>10s} {'truth_py':>10s} {'truth_pz':>10s} | "
              f"{'reco_px':>10s} {'reco_py':>10s} {'reco_pz':>10s} | "
              f"{'Δpx':>8s} {'Δpy':>8s} {'Δpz':>8s}")
        for e in events:
            print(f"  {float(e['truth_px']):10.1f} {float(e['truth_py']):10.1f} {float(e['truth_pz']):10.1f} | "
                  f"{float(e['reco_px']):10.1f} {float(e['reco_py']):10.1f} {float(e['reco_pz']):10.1f} | "
                  f"{float(e['dpx']):8.1f} {float(e['dpy']):8.1f} {float(e['dpz']):8.1f}")

        stats = compute_stats(events)
        print(f"  MAE:  px={stats['mae_px']:.1f}  py={stats['mae_py']:.1f}  pz={stats['mae_pz']:.1f} MeV/c")
        print(f"  RMSE: px={stats['rmse_px']:.1f}  py={stats['rmse_py']:.1f}  pz={stats['rmse_pz']:.1f} MeV/c")
        print(f"  Bias: px={stats['bias_px']:.1f}  py={stats['bias_py']:.1f}  pz={stats['bias_pz']:.1f} MeV/c")
        print()

    # --- Print summary comparison table ---
    print("=" * 100)
    print("Summary comparison (MAE in MeV/c)")
    print("=" * 100)
    print(f"  {'Backend':<25s} {'MAE_px':>8s} {'MAE_py':>8s} {'MAE_pz':>8s} | "
          f"{'RMSE_px':>8s} {'RMSE_py':>8s} {'RMSE_pz':>8s}")
    print(f"  {'-'*25} {'-'*8} {'-'*8} {'-'*8}   {'-'*8} {'-'*8} {'-'*8}")
    for name, events in backends.items():
        stats = compute_stats(events)
        print(f"  {name:<25s} {stats['mae_px']:8.1f} {stats['mae_py']:8.1f} {stats['mae_pz']:8.1f} | "
              f"{stats['rmse_px']:8.1f} {stats['rmse_py']:8.1f} {stats['rmse_pz']:8.1f}")
    print()

    # --- Read error analysis data if available ---
    error_dirs = {
        "RK": base_dir.parent / "error_analysis_rk",
        "NN (RK refit)": base_dir.parent / "error_analysis_nn",
    }
    print("=" * 100)
    print("Error Analysis Results (Fisher information, Profile Likelihood, MCMC)")
    print("=" * 100)
    for name, edir in error_dirs.items():
        summary_path = edir / "summary.csv"
        track_path = edir / "track_summary.csv"
        val_path = edir / "validation_summary.csv"

        if not summary_path.exists():
            print(f"\n  SKIP: {name} — {summary_path} not found")
            continue

        summary = read_error_analysis_summary(str(summary_path))
        tracks = read_error_analysis_tracks(str(track_path))

        print(f"\n--- {name} error analysis ---")
        print(f"  Tracks analyzed: {summary.get('successful_tracks', '?')}")
        print(f"  Median Fisher |p| width (68%): {summary.get('median_fisher_p_width68', '?')} MeV/c")
        print(f"  Median Profile |p| width (68%): {summary.get('median_profile_p_width68', '?')} MeV/c")
        print(f"  Median MCMC |p| width (68%): {summary.get('median_mcmc_p_width68', '?')} MeV/c")
        print(f"  Median MCMC acceptance rate: {summary.get('median_mcmc_acceptance_rate', '?')}")
        print(f"  Median MCMC ESS: {summary.get('median_mcmc_effective_sample_size', '?')}")
        print()

        print(f"  Per-track Fisher σ (MeV/c):")
        print(f"    {'event':>5s} {'σ_px':>8s} {'σ_py':>8s} {'σ_pz':>8s} {'σ_|p|':>8s} {'χ²_red':>8s}")
        for t in tracks:
            print(f"    {t['event_index']:>5s} {float(t['fisher_px_sigma']):8.2f} "
                  f"{float(t['fisher_py_sigma']):8.2f} {float(t['fisher_pz_sigma']):8.2f} "
                  f"{float(t['fisher_p_sigma']):8.2f} {float(t['chi2_reduced']):8.3f}")

        # Coverage
        if val_path.exists():
            print(f"\n  Coverage validation:")
            with open(str(val_path)) as vf:
                reader = csv.DictReader(vf)
                print(f"    {'method':<15s} {'count':>5s} {'cover68':>8s} {'cover95':>8s}")
                for row in reader:
                    print(f"    {row['method']:<15s} {row['count']:>5s} "
                          f"{row['cover68']:>8s} {row['cover95']:>8s}")
    print()

    print("=" * 100)
    print("All numbers above come directly from test output CSV files.")
    print("No numbers were estimated or calculated outside the test framework.")
    print("=" * 100)


if __name__ == "__main__":
    main()
