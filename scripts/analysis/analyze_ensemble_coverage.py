#!/usr/bin/env python3
"""[EN] Coverage analysis for the PDC RK ensemble run.
[CN] PDC RK ensemble 的覆盖率分析。

Reads the outputs of ``scripts/analysis/run_ensemble_coverage.sh`` (plus the
existing analyse_pdc_rk_error CSVs) and emits:
  * coverage table with Clopper-Pearson 95% binomial CI
  * pull-distribution summary per component
  * LaTeX snippets ready for \\input{} in the reports

Inputs (directory passed via --input-dir):
  - track_summary.csv         (Fisher/Laplace intervals per event)
  - bayesian_samples.csv      (MCMC intervals per sampled event)
  - profile_samples.csv       (Profile intervals per sampled event)
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.stats import beta

SIGMA_68 = 0.9944578832097531  # [EN] 1-sigma (68.27%) half-width multiplier for Gaussian. / [CN] 这里用的是 z-score 1.0 (~68.27%)
Z_68 = 1.0
Z_95 = 1.959963984540054


def clopper_pearson(k: int, n: int, alpha: float = 0.05) -> tuple[float, float]:
    """[EN] Clopper-Pearson exact binomial CI. / [CN] Clopper-Pearson 精确二项置信区间。"""
    if n <= 0:
        return (math.nan, math.nan)
    if k < 0 or k > n:
        raise ValueError(f"k={k} n={n}")
    lo = 0.0 if k == 0 else beta.ppf(alpha / 2, k, n - k + 1)
    hi = 1.0 if k == n else beta.ppf(1 - alpha / 2, k + 1, n - k)
    return (float(lo), float(hi))


def coverage_from_interval(truth: np.ndarray, lo: np.ndarray, hi: np.ndarray) -> tuple[int, int]:
    """[EN] Returns (covered_count, total_count) with NaNs dropped. / [CN] 返回 (covered, total)。"""
    mask = np.isfinite(truth) & np.isfinite(lo) & np.isfinite(hi)
    if not mask.any():
        return (0, 0)
    covered = int(((lo[mask] <= truth[mask]) & (truth[mask] <= hi[mask])).sum())
    return (covered, int(mask.sum()))


def gaussian_interval(center: np.ndarray, sigma: np.ndarray, z: float) -> tuple[np.ndarray, np.ndarray]:
    lo = center - z * sigma
    hi = center + z * sigma
    return lo, hi


def append_row(rows, method, param, k, n):
    cov = k / n if n > 0 else math.nan
    lo, hi = clopper_pearson(k, n)
    rows.append({
        "method": method,
        "param": param,
        "covered": k,
        "total": n,
        "coverage": cov,
        "cp_low": lo,
        "cp_high": hi,
    })


def build_fisher_laplace_coverage(track_df: pd.DataFrame) -> pd.DataFrame:
    """[EN] Build Fisher/Laplace per-component coverage from per-event track_summary. / [CN] 构建 Fisher/Laplace 每个分量的事件级覆盖率。"""
    rows = []
    comps = [("px", "truth_px", "reco_px", "fisher_px_sigma", "laplace_px_lower68", "laplace_px_upper68"),
             ("py", "truth_py", "reco_py", "fisher_py_sigma", "laplace_py_lower68", "laplace_py_upper68"),
             ("pz", "truth_pz", "reco_pz", "fisher_pz_sigma", "laplace_pz_lower68", "laplace_pz_upper68"),
             ("|p|", None, "reco_p", "fisher_p_sigma", "laplace_p_lower68", "laplace_p_upper68")]

    for name, truth_col, reco_col, sigma_col, lap_lo, lap_hi in comps:
        if name == "|p|":
            truth = np.sqrt(track_df["truth_px"] ** 2 + track_df["truth_py"] ** 2 + track_df["truth_pz"] ** 2).to_numpy()
        else:
            truth = track_df[truth_col].to_numpy()
        reco = track_df[reco_col].to_numpy()
        sigma = track_df[sigma_col].to_numpy()
        lap_lo_v = track_df[lap_lo].to_numpy()
        lap_hi_v = track_df[lap_hi].to_numpy()

        lo_f68, hi_f68 = gaussian_interval(reco, sigma, Z_68)
        lo_f95, hi_f95 = gaussian_interval(reco, sigma, Z_95)
        k, n = coverage_from_interval(truth, lo_f68, hi_f68)
        append_row(rows, "Fisher68", name, k, n)
        k, n = coverage_from_interval(truth, lo_f95, hi_f95)
        append_row(rows, "Fisher95", name, k, n)

        k, n = coverage_from_interval(truth, lap_lo_v, lap_hi_v)
        append_row(rows, "Laplace68", name, k, n)

    return pd.DataFrame(rows)


def build_pull_summary(track_df: pd.DataFrame) -> pd.DataFrame:
    """[EN] Fisher pull distribution stats per component. / [CN] Fisher pull 分布统计。"""
    rows = []
    comps = [("px", "truth_px", "reco_px", "fisher_px_sigma"),
             ("py", "truth_py", "reco_py", "fisher_py_sigma"),
             ("pz", "truth_pz", "reco_pz", "fisher_pz_sigma"),
             ("|p|", None, "reco_p", "fisher_p_sigma")]

    for name, truth_col, reco_col, sigma_col in comps:
        if name == "|p|":
            truth = np.sqrt(track_df["truth_px"] ** 2 + track_df["truth_py"] ** 2 + track_df["truth_pz"] ** 2).to_numpy()
        else:
            truth = track_df[truth_col].to_numpy()
        reco = track_df[reco_col].to_numpy()
        sigma = track_df[sigma_col].to_numpy()

        mask = np.isfinite(truth) & np.isfinite(reco) & np.isfinite(sigma) & (sigma > 0)
        pulls = (reco[mask] - truth[mask]) / sigma[mask]
        if pulls.size == 0:
            rows.append({"param": name, "n": 0, "mean": math.nan, "std": math.nan,
                         "p01": math.nan, "p99": math.nan, "in_3sigma": math.nan})
            continue
        rows.append({
            "param": name,
            "n": int(pulls.size),
            "mean": float(np.mean(pulls)),
            "std": float(np.std(pulls, ddof=1)),
            "p01": float(np.percentile(pulls, 1)),
            "p99": float(np.percentile(pulls, 99)),
            "in_3sigma": float(np.sum(np.abs(pulls) < 3.0) / pulls.size),
        })
    return pd.DataFrame(rows)


def build_mcmc_coverage(bayes_df: pd.DataFrame, track_df: pd.DataFrame) -> pd.DataFrame:
    """[EN] MCMC coverage on |p| only. / [CN] MCMC 仅覆盖 |p|。"""
    if bayes_df.empty:
        return pd.DataFrame()
    merged = bayes_df.merge(
        track_df[["source_file", "event_index", "track_index", "truth_px", "truth_py", "truth_pz"]],
        on=["source_file", "event_index", "track_index"],
        how="inner",
    )
    truth_p = np.sqrt(merged["truth_px"] ** 2 + merged["truth_py"] ** 2 + merged["truth_pz"] ** 2).to_numpy()
    rows = []
    for label, lo_col, hi_col in [("MCMC68", "mcmc_p_lower68", "mcmc_p_upper68"),
                                  ("MCMC95", "mcmc_p_lower95", "mcmc_p_upper95"),
                                  ("LaplaceMCMC68", "laplace_p_lower68", "laplace_p_upper68"),
                                  ("LaplaceMCMC95", "laplace_p_lower95", "laplace_p_upper95")]:
        k, n = coverage_from_interval(truth_p, merged[lo_col].to_numpy(), merged[hi_col].to_numpy())
        append_row(rows, label, "|p|", k, n)
    return pd.DataFrame(rows)


def build_profile_coverage(profile_df: pd.DataFrame, track_df: pd.DataFrame) -> pd.DataFrame:
    if profile_df.empty:
        return pd.DataFrame()
    merged = profile_df.merge(
        track_df[["source_file", "event_index", "track_index", "truth_px", "truth_py", "truth_pz"]],
        on=["source_file", "event_index", "track_index"],
        how="inner",
    )
    rows = []
    # [EN] Profile samples store per-component intervals relative to reco (e.g. profile_px_lower68 is already absolute). / [CN] 轮廓似然CSV里已是绝对区间值。
    comps = [("px", "truth_px", "profile_px_lower68", "profile_px_upper68", "profile_px_lower95", "profile_px_upper95"),
             ("py", "truth_py", "profile_py_lower68", "profile_py_upper68", "profile_py_lower95", "profile_py_upper95"),
             ("pz", "truth_pz", "profile_pz_lower68", "profile_pz_upper68", "profile_pz_lower95", "profile_pz_upper95"),
             ("|p|", None, "profile_p_lower68", "profile_p_upper68", "profile_p_lower95", "profile_p_upper95")]
    for name, truth_col, lo68, hi68, lo95, hi95 in comps:
        if name == "|p|":
            truth = np.sqrt(merged["truth_px"] ** 2 + merged["truth_py"] ** 2 + merged["truth_pz"] ** 2).to_numpy()
        else:
            truth = merged[truth_col].to_numpy()
        k, n = coverage_from_interval(truth, merged[lo68].to_numpy(), merged[hi68].to_numpy())
        append_row(rows, "Profile68", name, k, n)
        k, n = coverage_from_interval(truth, merged[lo95].to_numpy(), merged[hi95].to_numpy())
        append_row(rows, "Profile95", name, k, n)
    return pd.DataFrame(rows)


def emit_latex_coverage_table(df: pd.DataFrame, path: Path) -> None:
    """[EN] Emit booktabs-style LaTeX fragment. / [CN] 生成 booktabs 风格的 LaTeX 片段。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        r"% [EN] Auto-generated by scripts/analysis/analyze_ensemble_coverage.py. / [CN] 由脚本自动生成。",
        r"\begin{tabular}{llrrrr}",
        r"\toprule",
        r"Method & Param & $N$ & Coverage & CP low & CP high \\",
        r"\midrule",
    ]
    for _, row in df.iterrows():
        cov = f"{row['coverage']:.4f}" if math.isfinite(row['coverage']) else "--"
        lo = f"{row['cp_low']:.4f}" if math.isfinite(row['cp_low']) else "--"
        hi = f"{row['cp_high']:.4f}" if math.isfinite(row['cp_high']) else "--"
        param = row['param'].replace('|p|', r'$|p|$')
        lines.append(f"{row['method']} & {param} & {row['total']} & {cov} & {lo} & {hi} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}", ""]
    path.write_text("\n".join(lines), encoding="utf-8")


def emit_latex_pull_table(df: pd.DataFrame, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        r"% [EN] Auto-generated by scripts/analysis/analyze_ensemble_coverage.py. / [CN] 由脚本自动生成。",
        r"\begin{tabular}{lrrrrrr}",
        r"\toprule",
        r"Param & $N$ & mean & std & $p_{1\%}$ & $p_{99\%}$ & $<3\sigma$ \\",
        r"\midrule",
    ]
    for _, row in df.iterrows():
        param = row['param'].replace('|p|', r'$|p|$')
        n = int(row['n']) if math.isfinite(row['n']) else 0
        mean = f"{row['mean']:.3f}" if math.isfinite(row['mean']) else "--"
        std = f"{row['std']:.3f}" if math.isfinite(row['std']) else "--"
        p01 = f"{row['p01']:.3f}" if math.isfinite(row['p01']) else "--"
        p99 = f"{row['p99']:.3f}" if math.isfinite(row['p99']) else "--"
        in3 = f"{row['in_3sigma']:.3f}" if math.isfinite(row['in_3sigma']) else "--"
        lines.append(f"{param} & {n} & {mean} & {std} & {p01} & {p99} & {in3} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}", ""]
    path.write_text("\n".join(lines), encoding="utf-8")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Ensemble coverage analysis for PDC RK reconstruction.")
    parser.add_argument("--input-dir", required=True, help="Directory produced by analyze_pdc_rk_error (contains track_summary.csv, bayesian_samples.csv, profile_samples.csv).")
    parser.add_argument("--output-dir", required=True, help="Directory to write coverage CSVs, pull plots, and LaTeX fragments.")
    args = parser.parse_args(argv)

    input_dir = Path(args.input_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    track_path = input_dir / "track_summary.csv"
    if not track_path.exists():
        print(f"[analyze_ensemble_coverage] missing: {track_path}", file=sys.stderr)
        return 1

    track_df = pd.read_csv(track_path)
    track_df = track_df[track_df["truth_available"] == 1].reset_index(drop=True)
    print(f"[analyze_ensemble_coverage] loaded {len(track_df)} track rows with truth")

    coverage_rows = [build_fisher_laplace_coverage(track_df)]

    bayes_path = input_dir / "bayesian_samples.csv"
    if bayes_path.exists():
        bayes_df = pd.read_csv(bayes_path)
        coverage_rows.append(build_mcmc_coverage(bayes_df, track_df))
        print(f"[analyze_ensemble_coverage] MCMC samples: {len(bayes_df)}")

    profile_path = input_dir / "profile_samples.csv"
    if profile_path.exists():
        profile_df = pd.read_csv(profile_path)
        coverage_rows.append(build_profile_coverage(profile_df, track_df))
        print(f"[analyze_ensemble_coverage] Profile samples: {len(profile_df)}")

    coverage = pd.concat([d for d in coverage_rows if not d.empty], ignore_index=True)
    coverage.to_csv(output_dir / "coverage_with_ci.csv", index=False)

    pull_df = build_pull_summary(track_df)
    pull_df.to_csv(output_dir / "pull_summary.csv", index=False)

    emit_latex_coverage_table(coverage, output_dir / "coverage_table.tex")
    emit_latex_pull_table(pull_df, output_dir / "pull_table.tex")

    # [EN] Pull histograms (matplotlib). / [CN] pull 直方图。
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as ex:  # noqa: BLE001
        print(f"[analyze_ensemble_coverage] matplotlib unavailable ({ex}); skipping histograms", file=sys.stderr)
    else:
        fig, axes = plt.subplots(2, 2, figsize=(11, 8))
        comps = [("px", "truth_px", "reco_px", "fisher_px_sigma"),
                 ("py", "truth_py", "reco_py", "fisher_py_sigma"),
                 ("pz", "truth_pz", "reco_pz", "fisher_pz_sigma"),
                 ("|p|", None, "reco_p", "fisher_p_sigma")]
        x = np.linspace(-5, 5, 400)
        gauss = np.exp(-0.5 * x ** 2) / math.sqrt(2 * math.pi)
        for ax, (name, truth_col, reco_col, sigma_col) in zip(axes.flat, comps):
            if name == "|p|":
                truth = np.sqrt(track_df["truth_px"] ** 2 + track_df["truth_py"] ** 2 + track_df["truth_pz"] ** 2).to_numpy()
            else:
                truth = track_df[truth_col].to_numpy()
            reco = track_df[reco_col].to_numpy()
            sigma = track_df[sigma_col].to_numpy()
            mask = np.isfinite(truth) & np.isfinite(reco) & np.isfinite(sigma) & (sigma > 0)
            pulls = (reco[mask] - truth[mask]) / sigma[mask]
            ax.hist(pulls, bins=40, range=(-5, 5), density=True, color="#4472c4", alpha=0.75, edgecolor="black")
            ax.plot(x, gauss, color="red", linewidth=1.5, label="N(0,1)")
            ax.set_title(f"{name}: pull (N={pulls.size})")
            ax.set_xlabel("(reco - truth) / sigma")
            ax.set_ylabel("density")
            ax.legend(fontsize=8)
            ax.grid(True, alpha=0.3)
        fig.tight_layout()
        fig.savefig(output_dir / "pull_distributions.png", dpi=150)
        plt.close(fig)

    # [EN] Coverage vs expected (68/95) with Clopper-Pearson CI error bars. / [CN] 覆盖率与理想值对比图。
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception:  # noqa: BLE001
        pass
    else:
        fig, ax = plt.subplots(figsize=(11, 5))
        labels = []
        y = []
        y_lo = []
        y_hi = []
        targets = []
        for _, row in coverage.iterrows():
            method = row['method']
            if method.endswith("95"):
                target = 0.95
            elif method.endswith("68"):
                target = 0.68
            elif method.startswith("Fisher"):
                target = 0.68
            else:
                target = math.nan
            labels.append(f"{method}/{row['param']}")
            y.append(row['coverage'])
            y_lo.append(row['coverage'] - row['cp_low'])
            y_hi.append(row['cp_high'] - row['coverage'])
            targets.append(target)
        x = np.arange(len(labels))
        ax.errorbar(x, y, yerr=[y_lo, y_hi], fmt="o", color="#1f4e79", capsize=3, label="coverage ± CP95%CI")
        ax.plot(x, targets, "r--", label="expected", alpha=0.6)
        ax.set_xticks(x)
        ax.set_xticklabels(labels, rotation=90, fontsize=7)
        ax.set_ylim(-0.05, 1.05)
        ax.set_ylabel("coverage")
        ax.set_title("Interval coverage vs expected (Clopper-Pearson 95% CI error bars)")
        ax.grid(True, alpha=0.3)
        ax.legend()
        fig.tight_layout()
        fig.savefig(output_dir / "coverage_vs_expected.png", dpi=150)
        plt.close(fig)

    print(f"[analyze_ensemble_coverage] wrote outputs to {output_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
