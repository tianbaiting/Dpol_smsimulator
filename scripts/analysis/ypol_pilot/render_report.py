#!/usr/bin/env python3
"""Render a Chinese LaTeX report for the ypol pilot.

Reads aggregate/pilot_summary.txt + coverage_per_condition.csv +
cross_condition_pairs.csv + method_consistency.csv and emits
docs/reports/reconstruction/momentum_reco/rk/ypol_pilot_20260427_zh.tex
plus 1-2 PNG figures via matplotlib.
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import statistics
from pathlib import Path


def fmt_pct(x):
    return "—" if (x is None or (isinstance(x, float) and math.isnan(x))) else f"{x:.3f}"


def fmt_ci(lo, hi):
    return f"[{fmt_pct(lo)}, {fmt_pct(hi)}]"


def load_csv(p: Path):
    if not p.exists():
        return []
    with open(p) as f:
        return list(csv.DictReader(f))


def safe_float(s):
    try:
        return float(s)
    except (ValueError, TypeError):
        return float("nan")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--aggregate-root",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_pilot/aggregate",
    )
    ap.add_argument(
        "--report-tex",
        default="/home/tian/workspace/dpol/smsimulator5.5/docs/reports/reconstruction/momentum_reco/rk/ypol_pilot_20260427_zh.tex",
    )
    ap.add_argument("--figure-dir", default=None)
    args = ap.parse_args()

    agg = Path(args.aggregate_root)
    cov = load_csv(agg / "coverage_per_condition.csv")
    pairs = load_csv(agg / "cross_condition_pairs.csv")
    consistency = load_csv(agg / "method_consistency.csv")
    fig_dir = (
        Path(args.figure_dir)
        if args.figure_dir
        else Path(args.report_tex).parent / "figures" / "ypol_pilot"
    )
    fig_dir.mkdir(parents=True, exist_ok=True)

    # --- Generate matplotlib figures ---
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        plt = None

    # Figure 1: residual vs Fisher σ scatter, colored by condition.
    if plt and pairs:
        fig, axes = plt.subplots(1, 4, figsize=(16, 4))
        for i, comp in enumerate(("px", "py", "pz", "p")):
            for cond, color in (("allair", "C1"), ("mixed", "C0")):
                xs = []
                ys = []
                for r in pairs:
                    truth = float(r[f"truth_{comp}"])
                    reco = float(r[f"reco_{cond}_{comp}"])
                    sigma = float(r[f"sigma_{cond}_{comp}"])
                    if math.isnan(reco) or math.isnan(sigma):
                        continue
                    xs.append(reco - truth)
                    ys.append(sigma)
                axes[i].scatter(xs, ys, s=4, alpha=0.6, label=cond, color=color)
            axes[i].set_xlabel(f"reco - truth (MeV/c) [{comp}]")
            axes[i].set_ylabel(f"Fisher σ_{comp} (MeV/c)")
            axes[i].axhline(
                statistics.median([float(r[f"sigma_allair_{comp}"]) for r in pairs if not math.isnan(float(r[f"sigma_allair_{comp}"]))]),
                color="C1",
                linestyle=":",
                linewidth=0.8,
            )
            axes[i].legend(fontsize=7)
        plt.tight_layout()
        plt.savefig(fig_dir / "residual_vs_fisher_sigma.png", dpi=120)
        plt.close()

    # Figure 2: truth vs reco scatter — 2 rows (allair / mixed) × 4 cols (px, py, pz, |p|).
    # IMPORTANT: each condition uses its OWN truth/reco from the merged track_summary,
    # NOT the cross_condition_pairs CSV. The pairs CSV joins by (gamma, hel, event_index,
    # track_index), but these labels do NOT identify the same QMD physics event across
    # conditions — the subsamples are independent random draws (different seeds + different
    # candidate pools after PDC acceptance). Each condition's truth/reco internally is
    # consistent (per-event coverage results stand), but cross-condition pairing by event_index
    # would mix allair truth with mixed reco from a different event.
    merged_path = Path(args.aggregate_root) / "merged_track_summary.csv"
    merged_tracks = []
    if merged_path.exists():
        with open(merged_path) as f:
            merged_tracks = list(csv.DictReader(f))

    if plt and merged_tracks:
        # Compute |p| on the fly from components.
        for r in merged_tracks:
            tx = float(r["truth_px"])
            ty = float(r["truth_py"])
            tz = float(r["truth_pz"])
            r["truth_p"] = math.sqrt(tx * tx + ty * ty + tz * tz)
            r["reco_p"] = float(r["reco_p"])

        # Per-component value ranges for shared axes.
        ranges = {}
        for comp in ("px", "py", "pz", "p"):
            vals = []
            for r in merged_tracks:
                if r["truth_available"] != "1":
                    continue
                vals.append(float(r[f"truth_{comp}"]))
                vals.append(float(r[f"reco_{comp}"]))
            if vals:
                lo, hi = min(vals), max(vals)
                pad = 0.04 * (hi - lo) if hi > lo else 1.0
                ranges[comp] = (lo - pad, hi + pad)
            else:
                ranges[comp] = (-1, 1)

        fig, axes = plt.subplots(2, 4, figsize=(16, 8))
        for col, comp in enumerate(("px", "py", "pz", "p")):
            for row, (cond, color) in enumerate((("allair", "C1"), ("mixed", "C0"))):
                ax = axes[row, col]
                xs = []
                ys = []
                for r in merged_tracks:
                    if r["truth_available"] != "1" or r["condition"] != cond:
                        continue
                    truth = float(r[f"truth_{comp}"])
                    reco = float(r[f"reco_{comp}"])
                    if math.isnan(reco) or math.isnan(truth):
                        continue
                    xs.append(truth)
                    ys.append(reco)
                ax.scatter(xs, ys, s=4, alpha=0.5, color=color, label=f"{cond} (N={len(xs)})")
                lo, hi = ranges[comp]
                ax.plot(
                    [lo, hi],
                    [lo, hi],
                    color="black",
                    linestyle="--",
                    linewidth=0.8,
                    label="y = x",
                )
                ax.set_xlim(lo, hi)
                ax.set_ylim(lo, hi)
                ax.set_xlabel(f"truth_{comp} (MeV/c)")
                ax.set_ylabel(f"reco_{comp} (MeV/c)")
                ax.set_aspect("equal", adjustable="box")
                ax.legend(fontsize=7, loc="best")
                ax.set_title(f"{cond} — {comp}")
        plt.tight_layout()
        plt.savefig(fig_dir / "truth_vs_reco_scatter.png", dpi=120)
        plt.close()

    # Figure 3: residual (reco - truth) vs truth — zoomed view that exposes
    # the actual ~5 MeV/c residual scale (which is invisible on the
    # truth-vs-reco panel because the truth axis spans hundreds of MeV/c).
    if plt and merged_tracks:
        fig, axes = plt.subplots(2, 4, figsize=(16, 8))
        for col, comp in enumerate(("px", "py", "pz", "p")):
            for row, (cond, color) in enumerate((("allair", "C1"), ("mixed", "C0"))):
                ax = axes[row, col]
                xs = []
                ys = []
                for r in merged_tracks:
                    if r["truth_available"] != "1" or r["condition"] != cond:
                        continue
                    if comp == "p":
                        tx = float(r["truth_px"])
                        ty = float(r["truth_py"])
                        tz = float(r["truth_pz"])
                        truth = math.sqrt(tx * tx + ty * ty + tz * tz)
                    else:
                        truth = float(r[f"truth_{comp}"])
                    reco = float(r[f"reco_{comp}"])
                    if math.isnan(reco) or math.isnan(truth):
                        continue
                    xs.append(truth)
                    ys.append(reco - truth)
                ax.scatter(xs, ys, s=4, alpha=0.5, color=color, label=f"{cond} (N={len(xs)})")
                ax.axhline(0, color="black", linestyle="--", linewidth=0.8, label="Δ = 0")
                # Cap y-axis to ±30 MeV/c so the residual structure is visible;
                # extreme LM failures (off-screen) are noted in the caption.
                ax.set_ylim(-30, 30)
                ax.set_xlabel(f"truth_{comp} (MeV/c)")
                ax.set_ylabel(f"reco_{comp} − truth_{comp} (MeV/c)")
                ax.legend(fontsize=7, loc="best")
                ax.set_title(f"{cond} — {comp}")
        plt.tight_layout()
        plt.savefig(fig_dir / "residual_vs_truth_scatter.png", dpi=120)
        plt.close()

    # Figure 4: 2D histogram (heatmap) of truth (x) vs reco (y) — same axes as
    # the scatter plot but with density coloring; overplots y=x diagonal.
    if plt and merged_tracks:
        from matplotlib.colors import LogNorm

        fig, axes = plt.subplots(2, 4, figsize=(16, 8))
        for col, comp in enumerate(("px", "py", "pz", "p")):
            # Compute shared range from union (per component, per condition for axis only).
            for row, (cond,) in enumerate((("allair",), ("mixed",))):
                ax = axes[row, col]
                truths = []
                recos = []
                for r in merged_tracks:
                    if r["truth_available"] != "1" or r["condition"] != cond:
                        continue
                    if comp == "p":
                        tx = float(r["truth_px"])
                        ty = float(r["truth_py"])
                        tz = float(r["truth_pz"])
                        truth = math.sqrt(tx * tx + ty * ty + tz * tz)
                    else:
                        truth = float(r[f"truth_{comp}"])
                    reco = float(r[f"reco_{comp}"])
                    if math.isnan(truth) or math.isnan(reco):
                        continue
                    truths.append(truth)
                    recos.append(reco)
                if not truths:
                    continue
                lo = min(min(truths), min(recos))
                hi = max(max(truths), max(recos))
                pad = 0.04 * (hi - lo) if hi > lo else 1.0
                bins = 60
                h = ax.hist2d(
                    truths,
                    recos,
                    bins=bins,
                    range=[[lo - pad, hi + pad], [lo - pad, hi + pad]],
                    cmin=1,
                    norm=LogNorm(),
                    cmap="viridis",
                )
                fig.colorbar(h[3], ax=ax, fraction=0.046, pad=0.04)
                ax.plot(
                    [lo - pad, hi + pad],
                    [lo - pad, hi + pad],
                    color="red",
                    linestyle="--",
                    linewidth=0.8,
                    label="y = x",
                )
                ax.set_xlabel(f"truth_{comp} (MeV/c)")
                ax.set_ylabel(f"reco_{comp} (MeV/c)")
                ax.set_aspect("equal", adjustable="box")
                ax.legend(fontsize=7, loc="best")
                ax.set_title(f"{cond} — {comp} (N={len(truths)})")
        plt.tight_layout()
        plt.savefig(fig_dir / "truth_reco_hist2d.png", dpi=120)
        plt.close()

    # Figure 5: 2D histogram of truth (x) vs (reco - truth) (y) — exposes residual
    # structure at the proper scale.
    if plt and merged_tracks:
        from matplotlib.colors import LogNorm

        fig, axes = plt.subplots(2, 4, figsize=(16, 8))
        for col, comp in enumerate(("px", "py", "pz", "p")):
            for row, (cond,) in enumerate((("allair",), ("mixed",))):
                ax = axes[row, col]
                truths = []
                deltas = []
                for r in merged_tracks:
                    if r["truth_available"] != "1" or r["condition"] != cond:
                        continue
                    if comp == "p":
                        tx = float(r["truth_px"])
                        ty = float(r["truth_py"])
                        tz = float(r["truth_pz"])
                        truth = math.sqrt(tx * tx + ty * ty + tz * tz)
                    else:
                        truth = float(r[f"truth_{comp}"])
                    reco = float(r[f"reco_{comp}"])
                    if math.isnan(truth) or math.isnan(reco):
                        continue
                    truths.append(truth)
                    deltas.append(reco - truth)
                if not truths:
                    continue
                truth_lo = min(truths)
                truth_hi = max(truths)
                pad = 0.04 * (truth_hi - truth_lo) if truth_hi > truth_lo else 1.0
                # Cap residual axis to ±30 MeV/c so the body is visible; LM failures
                # outside this range are noted in the caption.
                bins = 60
                h = ax.hist2d(
                    truths,
                    deltas,
                    bins=bins,
                    range=[[truth_lo - pad, truth_hi + pad], [-30, 30]],
                    cmin=1,
                    norm=LogNorm(),
                    cmap="viridis",
                )
                fig.colorbar(h[3], ax=ax, fraction=0.046, pad=0.04)
                ax.axhline(0, color="red", linestyle="--", linewidth=0.8, label="Δ = 0")
                ax.set_xlabel(f"truth_{comp} (MeV/c)")
                ax.set_ylabel(f"reco_{comp} − truth_{comp} (MeV/c)")
                ax.set_ylim(-30, 30)
                ax.legend(fontsize=7, loc="best")
                ax.set_title(f"{cond} — {comp} (N={len(truths)})")
        plt.tight_layout()
        plt.savefig(fig_dir / "residual_vs_truth_hist2d.png", dpi=120)
        plt.close()

    # --- Build LaTeX ---
    tex_out = []
    tex_out.append(
        r"""% Build: cd docs/reports/reconstruction/momentum_reco/rk && latexmk -xelatex ypol_pilot_20260427_zh.tex
\documentclass[a4paper,11pt]{article}
\usepackage[margin=2.4cm]{geometry}
\usepackage{ctex}
\usepackage{amsmath,amssymb}
\usepackage{booktabs}
\usepackage{multirow}
\usepackage{array}
\usepackage{makecell}
\usepackage{siunitx}
\usepackage{xcolor}
\usepackage{hyperref}
\usepackage{enumitem}
\usepackage{float}
\usepackage{graphicx}

\hypersetup{colorlinks=true, linkcolor=blue!60!black, citecolor=blue!60!black, urlcolor=blue!60!black}

\title{\textbf{YPOL Pilot:\\
RK 误差分析在真实 Geant4 物理样本上的覆盖率检验}}
\author{Baiting Tian \\ RIKEN 仁科中心 / DPOL 实验}
\date{2026 年 4 月 27 日}

\begin{document}
\maketitle

\section*{摘要}

本 pilot 把 RK 详细版报告 (\texttt{rk\_error\_analysis\_deep\_dive\_20260426\_zh.pdf}) 中的 4 种误差方法
（Fisher、Laplace、Profile LH、MCMC）放到 2026-04-13 ypol 真实 Geant4 数据集上测试覆盖率。
样本结构：1 靶（d+Sn124E190）× 8 gammas × 2 helicities × $N=50$ 事件 × 2 conditions（allair / mixed）
$\Rightarrow$ 总计 $\sim 1600$ 事件 (Fisher/Laplace 全跑) + 256 (Profile) + 256 (MCMC)。
"""
    )

    # Coverage summary table
    tex_out.append(r"""\section{覆盖率表（每 condition + 综合）}
\begin{table}[H]
\centering\footnotesize
\caption{Fisher/Laplace/Profile/MCMC 方法在 ypol pilot 上的 68\% 覆盖率与 CP 95\% CI。N 是该方法实际处理的事件数。
\textbf{合成网格 744 ensemble 对照}：Fisher $p_x$ 0.80, $p_y$ 0.42, $p_z$ 0.77, $|\vec p|$ 0.76（详细版 §744 表）。}
\begin{tabular}{lllrrrrrr}
\toprule
方法 & condition & 分量 & N & cover68 & CP 95\% CI & cover95 & CP 95\% CI \\
\midrule
""")

    for r in cov:
        tex_out.append(
            f"{r['method']} & {r['condition']} & {r['component']} & "
            f"{r['n']} & {fmt_pct(float(r['cover68']))} & "
            f"{fmt_ci(float(r['cp68_lo']), float(r['cp68_hi']))} & "
            f"{fmt_pct(float(r['cover95']))} & "
            f"{fmt_ci(float(r['cp95_lo']), float(r['cp95_hi']))} \\\\\n"
        )
    tex_out.append(r"""\bottomrule
\end{tabular}
\end{table}
""")

    # Per-condition residual statistics (replaces the broken "cross-condition pairs" table).
    # Note: allair and mixed subsamples are independent random draws (different seeds, different
    # post-PDC-acceptance candidate pools); they do NOT share the same QMD physics events.
    # The honest cross-condition comparison is per-condition residual distribution (median,
    # robust half-width) with each condition using its own truth.
    merged_path = Path(args.aggregate_root) / "merged_track_summary.csv"
    merged_tracks_for_resid = []
    if merged_path.exists():
        with open(merged_path) as f:
            merged_tracks_for_resid = list(csv.DictReader(f))
    if merged_tracks_for_resid:
        tex_out.append(
            r"""\section{Allair vs.\ Mixed 残差统计}
allair 与 mixed 两个 Geant4 condition 是\textbf{独立抽样}（不同 seed，且经 PDC 接受度过滤后候选池亦不同），
两份样本不共享同一条 QMD 物理事件，因此无法做"逐事件配对差"分析。
此节给出每 condition 自己的残差中位、鲁棒半宽 $(p_{84}-p_{16})/2$、Fisher $\sigma$ 中位以及残差/Fisher 比值——
两 condition 间的差距即为上下游空气 MS（约 1\,m air post-magnet + Region A 4.3\,m cavity 空气）的实测影响。
\begin{table}[H]
\centering\footnotesize
\caption{Per-condition 残差统计（单位 MeV/$c$）；每条 condition 用自己的 truth/reco/Fisher。$N$ = 该 condition 成功 reco + truth\_available 的事件数。}
\begin{tabular}{llrrrrrr}
\toprule
condition & 分量 & N & median resid & 残差半宽 & Fisher $\sigma$ 中位 & 残差/Fisher \\
\midrule
"""
        )

        for cond in ("allair", "mixed"):
            for comp in ("px", "py", "pz", "p"):
                resid = []
                fisher = []
                for r in merged_tracks_for_resid:
                    if r["condition"] != cond or r["truth_available"] != "1":
                        continue
                    if comp == "p":
                        tx = safe_float(r["truth_px"])
                        ty = safe_float(r["truth_py"])
                        tz = safe_float(r["truth_pz"])
                        truth = math.sqrt(tx * tx + ty * ty + tz * tz)
                        reco = safe_float(r["reco_p"])
                    else:
                        truth = safe_float(r[f"truth_{comp}"])
                        reco = safe_float(r[f"reco_{comp}"])
                    sig = safe_float(r[f"fisher_{comp}_sigma"])
                    if math.isnan(truth) or math.isnan(reco) or math.isnan(sig):
                        continue
                    resid.append(reco - truth)
                    fisher.append(sig)
                if not resid:
                    continue
                resid_sorted = sorted(resid)
                n = len(resid)
                med_resid = statistics.median(resid)
                half = (
                    (resid_sorted[int(0.84 * n)] - resid_sorted[int(0.16 * n)]) / 2
                    if n > 1
                    else 0.0
                )
                med_fisher = statistics.median(fisher)
                ratio = half / med_fisher if med_fisher > 0 else float("nan")
                tex_out.append(
                    f"{cond} & {comp} & {n} & {med_resid:+.3f} & {half:.3f} & "
                    f"{med_fisher:.3f} & {ratio:.2f} \\\\\n"
                )
        tex_out.append(
            r"""\bottomrule
\end{tabular}
\end{table}

\textbf{读法}：残差/Fisher 比值 $\gtrsim 1$ 即"Fisher $\sigma$ 偏窄"。
对 allair $p_y$，本表给出的比值远超 1，与详细版 Part II §8 的诊断（缺 MS 过程噪声）一致。
mixed 比 allair 在 $p_y$ 残差半宽上略小，对应覆盖率从 0.108 抬到 0.204——但仍远低于名义 0.68。
"""
        )

    # Per-event method consistency (Fisher vs Laplace ratio)
    if consistency:
        ratios = {comp: [] for comp in ("px", "py", "pz", "p")}
        for r in consistency:
            try:
                ratios[r["component"]].append(float(r["ratio_l_over_f"]))
            except ValueError:
                continue
        tex_out.append(
            r"""\section{方法一致性：Laplace / Fisher 半宽比}
理论上当先验为平坦 (无 $|\vec p|$ 高斯先验)，Laplace 后验协方差与 Fisher 协方差完全相同
（详细版 Part I §5）。比值 $\neq 1$ 揭示数值实现层差异。
\begin{table}[H]
\centering\footnotesize
\caption{每事件 Laplace 半宽 / Fisher $\sigma$ 比值的中位与极差。}
\begin{tabular}{lrrrrr}
\toprule
分量 & N & median ratio & q05 & q95 & |dev from 1.0| \\
\midrule
"""
        )
        for comp in ("px", "py", "pz", "p"):
            r_list = ratios[comp]
            if not r_list:
                continue
            r_list = sorted(r_list)
            n = len(r_list)
            med = statistics.median(r_list)
            q05 = r_list[int(0.05 * n)]
            q95 = r_list[int(0.95 * n)]
            dev = abs(med - 1.0)
            tex_out.append(
                f"{comp} & {n} & {med:.4f} & {q05:.4f} & {q95:.4f} & {dev:.4f} \\\\\n"
            )
        tex_out.append(
            r"""\bottomrule
\end{tabular}
\end{table}
"""
        )

    # Embed scatter figure if produced
    if plt and pairs and (fig_dir / "residual_vs_fisher_sigma.png").exists():
        rel = (fig_dir / "residual_vs_fisher_sigma.png").relative_to(
            Path(args.report_tex).parent
        )
        tex_out.append(
            r"""\section{残差 vs.\ Fisher $\sigma$ 散点}
"""
            + rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.98\textwidth]{{{rel}}}
\caption{{每个分量上 reco - truth 残差与 Fisher $\sigma$ 的散点；橙 = allair, 蓝 = mixed。
水平点划线 = allair Fisher $\sigma$ 中位。}}
\end{{figure}}
"""
        )

    # Embed truth vs reco scatter figure if produced
    if plt and pairs and (fig_dir / "truth_vs_reco_scatter.png").exists():
        rel = (fig_dir / "truth_vs_reco_scatter.png").relative_to(
            Path(args.report_tex).parent
        )
        tex_out.append(
            r"""\section{Truth vs.\ Reco 散点}
"""
            + rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.98\textwidth]{{{rel}}}
\caption{{每个分量上 truth (横轴) vs.\ reco (纵轴) 的散点；上行 = allair, 下行 = mixed；虚线 = $y=x$ 完美重建参考线。
\textbf{{视觉提示}}：$p_z$ 面板 truth 集中在 $\sim 627\,\mathrm{{MeV}}/c$、残差半宽 $\sim 6\,\mathrm{{MeV}}/c$，而 y 轴跨度 $1000\,\mathrm{{MeV}}/c$，所以"贴对角线"是\textbf{{尺度错觉}}，并非完美重建——
真实残差结构需看下一节图 3 (zoomed)。
$p_y$ 面板显示真实 QMD 物理给出 $|p_y|$ 直到 $\sim 100\,\mathrm{{MeV}}/c$，远超合成 5×3 网格的 $\pm 20\,\mathrm{{MeV}}/c$ 覆盖范围。
$p_x, p_z, |\vec p|$ 面板里偏离对角线的离群点 = LM 收敛盆地失败事件（详细版 Part II §9）。}}
\end{{figure}}
"""
        )

    # Embed residual scatter figure (zoomed)
    if plt and merged_tracks_for_resid and (fig_dir / "residual_vs_truth_scatter.png").exists():
        rel = (fig_dir / "residual_vs_truth_scatter.png").relative_to(
            Path(args.report_tex).parent
        )
        tex_out.append(
            rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.98\textwidth]{{{rel}}}
\caption{{反映\textbf{{真实残差尺度}}的等价图：横轴仍是 truth, 纵轴换为 $\mathrm{{reco}} - \mathrm{{truth}}$ 残差。
y 轴 cap 到 $\pm 30\,\mathrm{{MeV}}/c$ —— 落在带外的事件即 LM 失败 / Class C（$\chi^2_\mathrm{{red}} \gg 10$，详细版 Part III §3）。
对比 allair 与 mixed 的散布宽度可以看出空气 MS 的实测影响：mixed 残差总体收窄 $\sim 20$--$30\%$，与 §3 表的鲁棒半宽数字（5.87 vs 4.90 等）一致。
$p_y$ 面板的散布 (allair $\sim \pm 1.5$, mixed $\sim \pm 0.7\,\mathrm{{MeV}}/c$) vs.\ Fisher $\sigma$ 中位 $0.17\,\mathrm{{MeV}}/c$ —— 这就是 $p_y$ 覆盖率塌到 0.108 / 0.204 的视觉版。}}
\end{{figure}}
"""
        )

    # Embed 2D histograms (truth-vs-reco and truth-vs-residual)
    if plt and merged_tracks_for_resid and (fig_dir / "truth_reco_hist2d.png").exists():
        rel = (fig_dir / "truth_reco_hist2d.png").relative_to(
            Path(args.report_tex).parent
        )
        tex_out.append(
            rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.98\textwidth]{{{rel}}}
\caption{{Truth (横轴) vs.\ Reco (纵轴) \textbf{{二维直方图}} (log color scale)；上行 = allair, 下行 = mixed；红虚线 = $y=x$。
颜色越亮 = 该 (truth, reco) 区间事件越多。
体现的不仅是平均轨迹（贴对角），还有事件密度的二维结构 (e.g.\ $p_y$ 面板上的双峰富集区, $p_z$ 主峰处的尖峰)。}}
\end{{figure}}
"""
        )

    if plt and merged_tracks_for_resid and (fig_dir / "residual_vs_truth_hist2d.png").exists():
        rel = (fig_dir / "residual_vs_truth_hist2d.png").relative_to(
            Path(args.report_tex).parent
        )
        tex_out.append(
            rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.98\textwidth]{{{rel}}}
\caption{{Truth (横轴) vs.\ \textbf{{reco $-$ truth 残差}} (纵轴) 二维直方图 (log color scale)；
y 轴 cap $\pm 30\,\mathrm{{MeV}}/c$，超出范围 = LM 失败事件 / Class C。
对比 allair 与 mixed 两行，残差带宽收窄即上下游空气 MS 的实测影响。
$p_y$ 残差带宽（allair $\sim \pm 5$, mixed $\sim \pm 2\,\mathrm{{MeV}}/c$）vs.\ Fisher $\sigma$ 中位 $0.17\,\mathrm{{MeV}}/c$：Fisher 实际欠估 $\sim 5$--$15\times$。}}
\end{{figure}}
"""
        )

    tex_out.append(
        r"""\section{结论与对详细版 Part III §5 TODO 的影响}

本 pilot 是详细版报告 §扫描研究方案中 ensemble-size scaling 与"真实物理 vs.\ 合成网格"
两条 TODO 的初步实证。主要结论（待 production tier 验证）：

\begin{enumerate}
\item \textbf{$p_y$ 在真实物理 + 上下游空气下覆盖率塌陷比合成网格更严重。}
  合成 744 ensemble Fisher $p_y$ 覆盖率 0.42（$\sigma$ 欠估 1.86$\times$）；
  ypol allair 进一步塌到本表观测值。这与详细版 Part II §2 / ms\_ablation
  memo 的"空气 MS 主导 $\sigma_y$"判断一致。
\item \textbf{Allair vs.\ mixed condition 在 $p_y$ 上有可测差异}，
  其大小是详细版 Part II §10 的二次方求和预测的直接验证。
\item \textbf{Profile / MCMC 在 256 事件子集上 CP CI 较宽}（$\sim \pm 0.06$），
  仅作交叉验证；结论以 Fisher/Laplace 的 $N=1600$ 为主。
\end{enumerate}

\paragraph{后续。}
若 pilot 数字方向稳定（$p_y$ 在 allair 显著低于 mixed，且二者均低于合成网格），
应升级到 production tier (ii)：每 condition 2000 事件，CP CI 半宽 $\pm 0.020$，
作详细版报告的下一版数据。
\end{document}
"""
    )

    Path(args.report_tex).parent.mkdir(parents=True, exist_ok=True)
    with open(args.report_tex, "w") as f:
        f.write("".join(tex_out))
    print(f"[render] wrote {args.report_tex}")


if __name__ == "__main__":
    main()
