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

    # Load asymmetry table
    summary = []
    asym_csv = out_dir / "asymmetry_table.csv"
    if asym_csv.exists():
        with open(asym_csv) as f:
            summary = list(csv.DictReader(f))

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

本 pilot 在 ypol elastic Geant4 输出 (\texttt{ypol\_new\_20260413\_elastic\_allair}) 上跑全重建（proton via RK + neutron via NEBULA），
对 2 个 target (Sn112, Sn124) × 2 个 gamma (g050, g080) × 2 个 helicity (ynp, ypn) = 8 个 cell 各采样 5000 elastic 事件，
检验 reco 后是否仍能看到 (px\_p − px\_n) 不对称 + R/gamma 趋势。
使用工具：\texttt{bin/reconstruct\_target\_momentum} (复用 ypol\_pilot 的二进制)。

\section{覆盖率与样本统计}

""")

    if summary:
        tex.append(r"""\begin{table}[H]
\centering\footnotesize
\caption{Per-cell 样本与基础统计}
\begin{tabular}{lllrrrrrrrr}
\toprule
target & gamma & hel & N\_truth & N\_reco\_pair &
truth $\langle\Delta p_x\rangle$ & reco $\langle\Delta p_x\rangle$ &
truth $R_x$ & reco $R_x$ &
truth $\langle\Delta p_y\rangle$ & reco $\langle\Delta p_y\rangle$ \\
\midrule
""")
        for r in summary:
            tex.append(
                f"{r['target']} & {r['gamma']} & {r['helicity']} & "
                f"{r['n_truth']} & {r['n_reco_pair']} & "
                f"{fmt_num(r['truth_pxp_minus_pxn_median'])} & "
                f"{fmt_num(r['reco_pxp_minus_pxn_median'])} & "
                f"{fmt_num(r['truth_R_x'])} & "
                f"{fmt_num(r['reco_R_x'])} & "
                f"{fmt_num(r['truth_pyp_minus_pyn_median'])} & "
                f"{fmt_num(r['reco_pyp_minus_pyn_median'])} \\\\\n"
            )
        tex.append(r"""\bottomrule
\end{tabular}
\end{table}

\textbf{读法}：$\langle\Delta p_x\rangle$ = (px\_p − px\_n) 的事件中位 (MeV/$c$)；
$R_x$ = $N(\Delta p_x > 0)/N(\Delta p_x < 0)$。
对极化分析，$R$ 应当随 gamma 变化（破裂动力学非各向同性的体现）；
本表对照 truth 与 reco 两栏，看 reco 是否保持同向趋势。

""")

    # Embed figures
    for png_name, caption in [
        ("px_diff_hist_Sn124E190.png",
         r"$\Delta p_x = p_{x,p} - p_{x,n}$ 分布 (Sn124E190, 4 cell)。黑线 = truth, 填色 = reco。reco 应跟随 truth 形状但带 reco 端 smearing。"),
        ("px_diff_hist_Sn112E190.png",
         r"同图但 Sn112E190 target。"),
        ("px_diff_truth_vs_reco_hist2d.png",
         r"$\Delta p_x$ 的 truth (横) vs reco (纵) 二维直方图，每 cell 一格。对角线 = 完美重建。"),
        ("r_vs_gamma.png",
         r"$R_x = N_+/N_-$ 随 gamma 的趋势。两 panel = 两 target；线/标 = helicity；实线 = truth, 虚线 = reco。reco 对 truth 趋势的保留程度即全重建对极化分析的 fidelity。")]:
        png_path = fig_src_dir / png_name
        if png_path.exists():
            tex.append(rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.95\textwidth]{{{args.figure_rel_dir}/{png_name}}}
\caption{{{caption}}}
\end{{figure}}
""")

    tex.append(r"""\section{结论}

\begin{enumerate}[nosep]
\item \textbf{(px\_p − px\_n) 不对称信号}：见上表 $\langle\Delta p_x\rangle$ 列对比 truth vs reco。
\item \textbf{R/gamma 趋势}：见 \texttt{r\_vs\_gamma.png}。reco 与 truth 同方向变化即"reco 没毁掉信号"。
\item \textbf{下一步}：若趋势可见，扩大到 8 gamma + 全 190k events / cell。若不可见，说明 reco fidelity 不够，需要回到详细版报告 §10 的 P0 修复（Bethe-Bloch + Kalman MS）。
\end{enumerate}
\end{document}
""")

    Path(args.report_tex).parent.mkdir(parents=True, exist_ok=True)
    with open(args.report_tex, "w") as f:
        f.write("".join(tex))
    print(f"[render] wrote {args.report_tex}")


if __name__ == "__main__":
    main()
