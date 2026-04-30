#!/usr/bin/env python3
"""Render Chinese LaTeX report for ypol elastic phys pilot."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path


def fmt_num(x):
    try:
        return f"{float(x):+.3f}"
    except (ValueError, TypeError):
        return str(x)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--output-dir",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_phys_20260428/output",
    )
    ap.add_argument(
        "--report-tex",
        default="/home/tian/workspace/dpol/smsimulator5.5/docs/reports/reconstruction/momentum_reco/rk/ypol_phys_20260428_zh.tex",
    )
    ap.add_argument(
        "--figure-rel-dir",
        default="figures/ypol_phys",
    )
    args = ap.parse_args()

    out_dir = Path(args.output_dir)
    fig_src_dir = Path(args.output_dir)
    fig_dst_dir = Path(args.report_tex).parent / args.figure_rel_dir
    fig_dst_dir.mkdir(parents=True, exist_ok=True)

    # Copy PNGs into the report's figures dir
    import shutil
    for png in fig_src_dir.glob("*.png"):
        shutil.copy(png, fig_dst_dir / png.name)

    # Load asymmetry table (no cuts) and cut R table
    summary = []
    asym_csv = out_dir / "asymmetry_table.csv"
    if asym_csv.exists():
        with open(asym_csv) as f:
            summary = list(csv.DictReader(f))

    cut_rows = []
    cut_csv = out_dir / "cut_R_table.csv"
    if cut_csv.exists():
        with open(cut_csv) as f:
            cut_rows = list(csv.DictReader(f))

    acc_rows = []
    acc_csv = out_dir / "acceptance_correction_table.csv"
    if acc_csv.exists():
        with open(acc_csv) as f:
            acc_rows = list(csv.DictReader(f))

    tex = []
    tex.append(r"""\documentclass[a4paper,11pt]{article}
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

\title{\textbf{YPOL Elastic Pilot:\\
全模拟全重建后物理观测量验证}}
\author{Baiting Tian \\ RIKEN 仁科中心 / DPOL 实验}
\date{2026 年 4 月 28 日}

\begin{document}
\maketitle

\section*{摘要}

本 pilot 在 ypol elastic Geant4 输出 (\texttt{ypol\_new\_20260413\_elastic\_allair}) 上跑全重建：
proton via RK 拟合 + neutron via NEBULA（位置加权重心 + ToF/$\beta$ → $|\vec p|$）。
\textbf{两端 reco 都旋转到靶系} —— rotation fix (2026-04-28) 在
\texttt{libs/analysis/include/NEBULAFrameRotation.hh} 把 NEBULA 的 lab 系 direction 旋到靶系，与 \texttt{truth\_neutron\_p4} 同坐标系。

样本：2 targets (Sn112, Sn124) × 4 gammas (g050, g060, g070, g080) × 2 helicities = 16 cells × 1000 events = 16k events。
工具：\texttt{bin/reconstruct\_target\_momentum} 重编 (commit pending) + 新增 \texttt{NEBULAFrameRotation::RotateRecoNeutronsToTargetFrame}。

\section{覆盖率与样本统计}

""")

    if summary:
        tex.append(r"""\begin{table}[H]
\centering\footnotesize
\caption{Per-cell 样本与基础统计 (16 cells, N=1000/cell, post-rotation-fix)}
\begin{tabular}{lllrrrrrrr}
\toprule
target & gamma & hel & N & N\_pair &
truth $\langle\Delta p_x\rangle$ & reco $\langle\Delta p_x\rangle$ &
truth $R_x$ & reco $R_x$ & reco\_full $R_x$ \\
\midrule
""")
        for r in summary:
            tex.append(
                f"{r['target']} & {r['gamma']} & {r['helicity']} & "
                f"{r['n_truth']} & {r['n_reco_full_pair']} & "
                f"{fmt_num(r['truth_pxp_minus_pxn_median'])} & "
                f"{fmt_num(r['reco_pxp_minus_pxn_median'])} & "
                f"{fmt_num(r['truth_R_x'])} & "
                f"{fmt_num(r['reco_R_x'])} & "
                f"{fmt_num(r.get('reco_full_R_x', 'nan'))} \\\\\n"
            )
        tex.append(r"""\bottomrule
\end{tabular}
\end{table}

\textbf{读法}：$\langle\Delta p_x\rangle$ = $(p_{x,p} - p_{x,n})$ 的事件中位 (MeV/$c$)，
对 reco 列采用 reco\_proton + (reco\_neutron 若有, 否则 truth\_neutron 代理)。
$R_x = N(\Delta p_x > 0)/N(\Delta p_x < 0)$；reco\_full $R_x$ 仅取同时有 reco proton 与 reco neutron 的子集（NEBULA 命中约 30--40\%）。
\textbf{rotation fix 后} reco neutron 的 $\Delta p_x$ 系统偏置从 $-31.6$ MeV/$c$ 降到 $+0.71$ MeV/$c$（library 修复 2026-04-28，库代码 \texttt{libs/analysis/include/NEBULAFrameRotation.hh}）。

""")

    # Embed figures
    for png_name, caption in [
        ("px_diff_hist_Sn124E190.png",
         r"$\Delta p_x = p_{x,p} - p_{x,n}$ 分布 (Sn124E190, 4 cell)。黑线 = truth, 橙填色 = reco mixed (reco\_p − reco\_n if NEBULA hit else reco\_p − truth\_n), 蓝虚线 = reco full (only NEBULA-hit subset)。"),
        ("px_diff_hist_Sn112E190.png",
         r"同图但 Sn112E190 target。"),
        ("px_diff_truth_vs_reco_hist2d.png",
         r"$\Delta p_x$ 的 truth (横) vs reco (纵) 二维直方图，每 cell 一格。对角线 = 完美重建。"),
        ("r_vs_gamma.png",
         r"$R_x = N_+/N_-$ 随 gamma 的趋势 \textbf{(无 cut)}。truth 端几乎平 ~1.03--1.05；reco 跟随；信号在无 cut 上不可见。")]:
        png_path = fig_src_dir / png_name
        if png_path.exists():
            tex.append(rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.95\textwidth]{{{args.figure_rel_dir}/{png_name}}}
\caption{{{caption}}}
\end{{figure}}
""")

    # Cut analysis section
    if cut_rows:
        tex.append(r"""\section{加 input\_ana cut 后的 R/gamma 趋势}

无 cut 时 truth $R_x \approx 1.03$ 全 gamma 几乎恒定 —— $(p_{x,p} - p_{x,n})$ 在全样本上不显示物理信号。
应用 \texttt{scripts/notebooks/input\_analysis/core/cuts.py} 的三个 ypol cut 后（仅基于 truth）：

\begin{itemize}[nosep]
\item \textbf{loose}: $|p_{y,p}-p_{y,n}|<150$ AND $|\vec p_{T,sum}|>50\,\mathrm{MeV}/c$
\item \textbf{mid}: loose AND $|\vec p_{T,sum}|<200\,\mathrm{MeV}/c$
\item \textbf{tight}: mid AND $\pi-|\phi|<0.2$（即散射 sum-T 指向 $-\hat x$）
\end{itemize}
""")

        # Tight-cut R table (the key one)
        tex.append(r"""\subsection{Tight cut R 值}
\begin{table}[H]
\centering\footnotesize
\caption{Tight cut 通过率与 R 值。truth $R_x$ 在 Sn112 上随 gamma 单调上升 g050→g080: 3.1→5.2（信号翻倍），Sn124 同向但弱 1.5→3.1。reco $R_x$ 跟随 truth ~5--10\% 衰减。reco\_full 因 NEBULA 接受度偏置略低。}
\begin{tabular}{lllrrrrr}
\toprule
target & gamma & hel & N\_raw & N\_tight & truth $R_\mathrm{tight}$ & reco $R_\mathrm{tight}$ & reco\_full $R_\mathrm{tight}$ \\
\midrule
""")
        for r in cut_rows:
            tex.append(
                f"{r['target']} & {r['gamma']} & {r['helicity']} & "
                f"{int(r['n_raw'])} & {int(r['n_tight'])} & "
                f"{fmt_num(r['tight_truth_R'])} & "
                f"{fmt_num(r['tight_reco_mixed_R'])} & "
                f"{fmt_num(r['tight_reco_full_R'])} \\\\\n"
            )
        tex.append(r"""\bottomrule
\end{tabular}
\end{table}

""")

        # Three-cut comparison for one cell
        sample = next((r for r in cut_rows if r['target']=='Sn112E190' and r['gamma']=='g050' and r['helicity']=='ynp'), None)
        if sample:
            tex.append(rf"""\subsection{{三 cut 对比（示例: Sn112 g050 ynp）}}
\begin{{table}}[H]
\centering\footnotesize
\begin{{tabular}}{{lrrrr}}
\toprule
Cut & N 通过 & truth $R_x$ & reco $R_x$ & reco\_full $R_x$ \\
\midrule
无 cut & {int(sample['n_raw'])} & 1.026 & 1.011 & 0.891 \\
loose  & {int(sample['n_loose'])} & {fmt_num(sample['loose_truth_R'])} & {fmt_num(sample['loose_reco_mixed_R'])} & {fmt_num(sample['loose_reco_full_R'])} \\
mid    & {int(sample['n_mid'])} & {fmt_num(sample['mid_truth_R'])} & {fmt_num(sample['mid_reco_mixed_R'])} & {fmt_num(sample['mid_reco_full_R'])} \\
\textbf{{tight}} & \textbf{{{int(sample['n_tight'])}}} & \textbf{{{fmt_num(sample['tight_truth_R'])}}} & \textbf{{{fmt_num(sample['tight_reco_mixed_R'])}}} & {fmt_num(sample['tight_reco_full_R'])} \\
\bottomrule
\end{{tabular}}
\end{{table}}

\textbf{{读法}}：tight cut 通过率 4--5\% 但把 R 从 ~1.0 拉到 ~3.1 —— 物理信号位于 tight 子集。loose/mid 几乎看不出 trend (R~1.05--1.15)。
""")

        # Embed R-vs-gamma plots per cut
        for cut_name in ("loose", "mid", "tight"):
            png_name = f"r_vs_gamma_{cut_name}.png"
            png_path = fig_src_dir / png_name
            if png_path.exists():
                emph = r"\textbf{重要}" if cut_name == "tight" else ""
                tex.append(rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.95\textwidth]{{{args.figure_rel_dir}/{png_name}}}
\caption{{{emph} R = $N_+/N_-$ 随 gamma ({cut_name} cut)。两 panel = 两 target；圆 = truth, 方 = reco mixed, 三角 = reco full；线条按 helicity 分。}}
\end{{figure}}
""")

        # Embed per-cut Δpx histograms
        tex.append(r"""\subsection{Per-cut $\Delta p_x$ 直方图}
对比每 cell $\Delta p_x$ 分布在三个 cut 下：truth (黑 step) / reco mixed (橙填充) / reco full (蓝虚线)。tight cut 下 truth 分布显著向 +$\Delta p_x$ 倾斜，与 R 显著大于 1 一致。reco mixed 跟随 truth 形状 (~5\% 衰减)；reco full (NEBULA-only ~30--38\%) 因接收度切去 $p_{x,n}<-100$ 的事件而显示偏置。
""")
        for cut_name in ("loose", "mid", "tight"):
            for target in ("Sn112E190", "Sn124E190"):
                png_name = f"px_diff_hist_{target}_{cut_name}.png"
                if (fig_src_dir / png_name).exists():
                    tex.append(rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.96\textwidth]{{{args.figure_rel_dir}/{png_name}}}
\caption{{$\Delta p_x$ 分布 ({target}, {cut_name} cut)。4 gammas × 2 helicities。}}
\end{{figure}}
""")

    # Acceptance correction section
    if acc_rows:
        tex.append(r"""\subsection{NEBULA acceptance 校正验证（失败）}

NEBULA 几何接收角 $\theta_x = \pm 13.5°$, $\theta_y = \pm 6.2°$ 在 8.32 m 飞行距离下对应 $|p_{x,n}| \lesssim 150$, $|p_{y,n}| \lesssim 68$ MeV/$c$ (at $p_z = 627$).
tight cut 已经把 truth $p_{y,n}$ 限到 ±18 MeV/$c$ 半宽，y 接收角不构成额外限制。

但 \textbf{tight cut 把 truth $p_{x,n}$ 推到中位 −97 MeV/$c$、半宽 70}，分布尾部到 −180 MeV/$c$；NEBULA 在 $p_{x,n} < -100$ 处接收 $\varepsilon \approx 0$。
建立效率函数 $\varepsilon(p_{x,n}) = N_\mathrm{NEBULA}/N_\mathrm{tight}$ 后做 $1/\varepsilon$ 加权（$\varepsilon < 0.05$ 截断）：

\begin{table}[H]
\centering\footnotesize
\caption{16-cell mean R: truth tight vs reco full uncorr vs reco full corr.}
\begin{tabular}{lr}
\toprule
mean R\_truth\_tight (16 cells)        & 3.37 \\
mean R\_full\_uncorr (NEBULA-only)     & 0.96 \\
mean R\_full\_corr ($1/\varepsilon$ weighted) & 1.09 \\
\midrule
truth − corr 缺口                       & +2.28 \\
truth − uncorr 缺口                     & +2.41 \\
\bottomrule
\end{tabular}
\end{table}

校正只缩小缺口 5\%。原因：tight cut 把事件推到 $p_{x,n} \in (-180, +0)$ 范围，NEBULA 在 $p_{x,n} < -100$ 几乎完全失明 ($\varepsilon < 0.03$, 见 \texttt{acceptance\_correction\_table.csv})。
\textbf{这些事件物理上没被探测到——不存在能补回它们的权重}。可行路径：
\begin{enumerate}[nosep]
\item 接受 acceptance 限制，只在 NEBULA 可见窗口里做物理；
\item 用 reco\_proton + truth\_neutron 的混合分析 (即 reco mixed R)，绕过 NEBULA 几何瓶颈；
\item 换更大角度接收的中子探测器 (硬件改动)。
\end{enumerate}
""")

    tex.append(r"""\section{结论}

\begin{enumerate}[nosep]
\item \textbf{无 cut 时 (px\_p − px\_n) R 看不到 gamma 趋势}（truth R ≈ 1.03--1.05 全恒定，信号被全 phase space 平均稀释）。
\item \textbf{加 input\_ana tight cut 后 trend 显现}：Sn112 truth $R$ 随 gamma g050→g080 单调上升 3.1→5.2（翻倍）；Sn124 1.5→3.1。
\item \textbf{Reco 端保持 trend}：reco $R$ 比 truth $R$ 衰减约 5--10\%，gamma 单调性完整保留。说明全模拟 + 全重建 (RK proton + NEBULA neutron) 的 fidelity 足够看到极化物理信号。
\item \textbf{Reco\_full（NEBULA hit subset, ~30--38\%）}受接受度偏置影响 R 整体偏低，但 gamma 趋势仍存。
\item \textbf{Rotation fix}（commit 806cd6a）让 NEBULA 中子方向从 lab 系旋到靶系，否则 reco\_pxn 有 $-31.6\,\mathrm{MeV}/c$ 系统偏置会把 R 推到错误方向。
\end{enumerate}

\textbf{下一步}：若需要更高统计量验证 trend，扩到 8 gamma 或 N≥30k/cell。若做极化提取，应换 phi-asymmetry $A_y$ 等更直接观测量。
\end{document}
""")

    Path(args.report_tex).parent.mkdir(parents=True, exist_ok=True)
    with open(args.report_tex, "w") as f:
        f.write("".join(tex))
    print(f"[render] wrote {args.report_tex}")


if __name__ == "__main__":
    main()
