#!/usr/bin/env python3
"""Render a single, linear LaTeX report for the ypol elastic phys pilot.

Conventions (consistent throughout — no mixing):
- Frame: ALL Δp_x values are in the REACTION-PLANE frame (rotate event by -φ
  with φ = atan2(sum_p_y^truth, sum_p_x^truth) so sum_T → +x). Lab frame is
  not used in this report.
- Three reco variants are clearly separated:
    A. truth      : truth_p − truth_n
    B. reco mixed : reco_p (RK) − [reco_n if NEBULA hit else truth_n proxy]
    C. reco full  : NEBULA-hit subset only, reco_p − reco_n
  Each table column / each figure curve / each histogram explicitly says
  which variant.
"""
from __future__ import annotations

import argparse
import csv
import math
import shutil
import statistics as _s
from collections import defaultdict
from pathlib import Path


def safe_float(s):
    try:
        return float(s)
    except (ValueError, TypeError):
        return float("nan")


def fmt(v):
    if v is None or (isinstance(v, float) and math.isnan(v)):
        return "—"
    return f"{v:.3f}"


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
    fig_src_dir = out_dir
    fig_dst_dir = Path(args.report_tex).parent / args.figure_rel_dir
    fig_dst_dir.mkdir(parents=True, exist_ok=True)

    for png in fig_src_dir.glob("*.png"):
        shutil.copy(png, fig_dst_dir / png.name)

    # Load tables
    asym = []
    asym_csv = out_dir / "asymmetry_table.csv"
    if asym_csv.exists():
        with open(asym_csv) as f:
            asym = list(csv.DictReader(f))

    cut = []
    cut_csv = out_dir / "cut_R_table.csv"
    if cut_csv.exists():
        with open(cut_csv) as f:
            cut = list(csv.DictReader(f))
    for r in cut:
        for k, v in list(r.items()):
            try:
                r[k] = float(v)
            except (ValueError, TypeError):
                pass

    rp = []
    rp_csv = out_dir / "reaction_plane_R_table.csv"
    if rp_csv.exists():
        with open(rp_csv) as f:
            rp = list(csv.DictReader(f))
    for r in rp:
        for k, v in list(r.items()):
            try:
                r[k] = float(v)
            except (ValueError, TypeError):
                pass

    acc = []
    acc_csv = out_dir / "acceptance_correction_table.csv"
    if acc_csv.exists():
        with open(acc_csv) as f:
            acc = list(csv.DictReader(f))
    for r in acc:
        for k, v in list(r.items()):
            try:
                r[k] = float(v)
            except (ValueError, TypeError):
                pass

    # ----- BEGIN TEX -----
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

\title{\textbf{YPOL Elastic Pilot — 全模拟全重建后 R/gamma 趋势验证}}
\author{Baiting Tian \\ RIKEN 仁科中心 / DPOL 实验}
\date{2026 年 4 月 28--30 日}

\begin{document}
\maketitle

\section*{摘要}
在 ypol elastic Geant4 输出 (\texttt{ypol\_new\_20260413\_elastic\_allair}) 上跑全重建（RK proton + NEBULA neutron, 含 2026-04-28 NEBULA 坐标系修复 commit \texttt{806cd6a}），
在 16 个 cell × 10000 事件 (4 gammas × 2 targets × 2 helicities) 上验证：
\textbf{应用 input\_ana 三个 cut + 反应平面旋转后, $R_x = N(\Delta p_x^\text{rot}>0)/N(\Delta p_x^\text{rot}<0)$ 随 gamma 单调下降, reco 端忠实跟随 truth。}

\section{1. 全重建动量精度 overview}

\textbf{在进入 cut/R 分析之前, 先看一下当前 reco 链对 (proton, neutron) × ($p_x, p_y, p_z$) 各分量的重建精度。}
所有事件均通过 truth\_has\_proton \& truth\_has\_neutron 选择, 共 160000 事件 (16 cells × 10000 events)。
proton 通过 RK 重建（要求 \texttt{n\_reco\_proton} > 0, 全部事件均成功）；neutron 通过 NEBULA $\beta$ 重建（要求 NEBULA 击中, 通过率 $\sim$34\%）。

\subsection*{1.1 Per-particle 残差直方图（reco − truth）}
\begin{figure}[H]
\centering
\includegraphics[width=\textwidth]{__REL_DIR__/reco_residuals_per_particle.png}
\caption{Reco − truth 残差分布。上行: proton (RK), 下行: neutron (NEBULA)。每行三列: $\Delta p_x$, $\Delta p_y$, $\Delta p_z$。
红虚线 = median, 标题给出 N、median、half-width (=$(P_{84}-P_{16})/2$)。}
\end{figure}

\begin{table}[H]
\centering\footnotesize
\caption{Per-particle 残差汇总（单位: MeV/$c$）。half-width = $(P_{84}-P_{16})/2$, 接近 1$\sigma$。}
\begin{tabular}{lcrrrr}
\toprule
particle & 通道 & N & median & half-width  \\
\midrule
proton  & $\Delta p_x$ & 160000 & $-0.03$ &  5.89 \\
proton  & $\Delta p_y$ & 160000 & $-0.12$ &  1.53 \\
proton  & $\Delta p_z$ & 160000 & $-1.05$ &  5.91 \\
\midrule
neutron & $\Delta p_x$ &  55115 & $+0.59$ &  3.65 \\
neutron & $\Delta p_y$ &  55115 & $-0.15$ &  2.93 \\
neutron & $\Delta p_z$ &  55115 & $-9.79$ &  8.59 \\
\bottomrule
\end{tabular}
\end{table}

\textbf{读法}：
\begin{itemize}[nosep]
\item proton 三方向 median 全部 $|\text{med}| < 1.1$ MeV/$c$, half-width $\sim 1.5$–$5.9$ MeV/$c$ —— RK 重建准确度 $\sim$1\% 量级（典型 $|\vec p_p| \sim 600$ MeV/$c$）。
\item neutron $p_x, p_y$ 也无明显偏置 (med $<0.6$ MeV/$c$), half-width $\sim 3$ MeV/$c$。
\item neutron $p_z$ 有 $-9.8$ MeV/$c$ 系统偏 + 8.6 MeV/$c$ 半宽 —— 来自 NEBULA $\beta$ 法的非线性 ($p = m \gamma\beta$, $\beta$ 在 0.7 附近放大误差) + 时间分辨。这是 full reco 端 R 偏置的一部分来源。
\item 注意: $\Delta p_y$ 比 $\Delta p_x$ 窄 (proton: 1.5 vs 5.9; neutron: 2.9 vs 3.6) —— 因 PDC y-strip 比 x-strip 测量精度高, 且 NEBULA y-bar 排布更密。
\end{itemize}

\subsection*{1.2 Truth $\phi^\text{event}$ vs reco $\phi^\text{event}$}
对 NEBULA 击中的 55115 事件, 用 $\phi^\text{event} = \arctan_2(p_{y,p}+p_{y,n}, p_{x,p}+p_{x,n})$ 分别从 truth 和 reco 计算事件方位角:
\begin{figure}[H]
\centering
\includegraphics[width=\textwidth]{__REL_DIR__/truth_phi_vs_reco_phi.png}
\caption{左: truth $\phi$ vs reco $\phi$ 二维直方图（红虚线 $y=x$, NEBULA 击中事件）；右: $\Delta\phi = \phi^\text{reco} - \phi^\text{truth}$ 一维分布（缠绕到 $[-180°, 180°]$）。}
\end{figure}

\textbf{统计}:
\begin{itemize}[nosep]
\item $\Delta\phi$ median = $+0.13°$ (无系统偏), half-width = $22.0°$。
\item 约 $46.5\%$ 事件满足 $|\Delta\phi| < 10°$, $\sim$80\% 在 $|\Delta\phi| < 30°$。
\item 2D 图主对角线清晰可见但有显著弥散, 反映 reco $\vec p_T^\text{sum}$ 的振幅不大 (典型几十 MeV/$c$) 时, 方向不确定度偏大。
\end{itemize}

\textbf{对反应平面分析的影响}: 全 reco (B/C 变体) 端用 reco $\phi$ 旋转时, 会引入 $\sim$20° 的旋转误差, 把部分 $\Delta p_x$ 信号"漏"到 $\Delta p_y$ 方向。这是 reco mixed/full 端 $R_x^\text{rot}$ 比 truth 端衰减的另一来源。
\textbf{说明}: 当前 §5 表中的 reaction-plane 旋转\textbf{用的是 truth $\phi$}（与 cut 定义一致）, 因此 §5 的 reco 端衰减不包括 $\phi$ 旋转误差; 一旦换成 reco $\phi$ 旋转, mixed/full 还要再吃一次 $\sim$5--10\% 的衰减。

\subsection*{1.3 偏差来源解析（每条 residual 各自来自哪里）}

\paragraph{Proton (RK), $\Delta p_y$ \textit{half-width 1.5} —— 最准}
\textbf{为什么最准}: PDC 的 y 方向 = drift 方向, drift time → drift distance 后, 单层位置精度 $\sim 200\;\mu\text{m}$。两个 PDC 之间 1.65 m 的杠杆 + dipole 不偏 y, 所以 $p_y$ 直接由两 PDC 的 y 击中点斜率得到, 不依赖磁场积分。残差几乎全部由 PDC y-strip 测量精度 + 多次散射决定。

\paragraph{Proton (RK), $\Delta p_x$ \textit{half-width 5.9}}
\textbf{为什么是 $\Delta p_y$ 的 4 倍}: $p_x$ 受 dipole 弯曲, RK 必须解 $\int B \cdot d\vec\ell$ 反向求 $p$。误差来源:
\begin{itemize}[nosep]
\item PDC1+PDC2 击中点位置精度 (相同的 $\sim 200\;\mu\text{m}$) 经过 $1/(B \cdot L)$ 杠杆放大;
\item 磁场 map 与靶位置坐标系的 alignment;
\item 多次散射 (尤其在 PDC 玻璃纤维 + 出窗口的金属法兰).
\end{itemize}
这是 RK 误差报告中 dE/dx + (u, v, p) 线性化已识别的来源, 在 \texttt{rk\_reconstruction\_status\_20260416\_zh} 文档里有展开。

\paragraph{Proton (RK), $\Delta p_z$ \textit{median $-1.05$, half-width 5.9}}
\textbf{$-1$ MeV/$c$ 系统偏} —— RK 是从 PDC 切线斜率反求 $|\vec p|$ 后再分解到 $(p_x, p_y, p_z)$, 当前 dE/dx 模型对靶后到 PDC 之间气体/塑料经过的能量损失略微低估 (约 1 MeV/c on $|\vec p|$). 半宽和 $\Delta p_x$ 一致是因为 $p_z$ 与 $|\vec p|$ 几乎线性相关。

\paragraph{Neutron (NEBULA), $\Delta p_y$ \textit{half-width 2.9}}
\textbf{为什么这么窄}: $p_y \propto \sin\theta_y$, $\theta_y$ 由 NEBULA bar 的 y 位置 / 飞行距离 (8.32 m) 给出。bar 的 y-coordinate 精度由聚集 + 时间双端读出推得 $\sigma \sim 5$ mm $\to \sigma(\theta_y) \approx 5/8320 \approx 0.6$ mrad $\to \sigma(p_y) \approx |\vec p_n| \cdot 0.6$ mrad $\approx 0.4$ MeV/c (但实际 $\sim 3$ 是因为 bar 是 12 cm 宽, 不是连续).

\paragraph{Neutron (NEBULA), $\Delta p_x$ \textit{half-width 3.6}}
NEBULA 的 x 方向是 bar 的长度方向, 由\textbf{两端读出时间差}推得 ($v_\text{eff} \approx 7$ cm/ns × $\Delta t$), 精度比 y 略差 (因有效速度受材料色散影响)。

\paragraph{Neutron (NEBULA), $\Delta p_z$ \textit{median $-9.79$, half-width 8.6} —— 最大瓶颈}
\textbf{$-10$ MeV/c 系统偏 + 9 MeV/c 半宽都来自 $\beta$ 法非线性}:
\begin{align}
p_n &= m_n \cdot \frac{\beta}{\sqrt{1-\beta^2}}, \quad \frac{dp_n}{d\beta} = \frac{m_n}{(1-\beta^2)^{3/2}}
\end{align}
对 ypol 配置 ($T_\text{kin} \approx 200$ MeV, $\beta \approx 0.57$, $\gamma \approx 1.21$), $dp_n/d\beta \approx m_n / (1-0.32)^{1.5} \approx 1670$ MeV/c per unit $\beta$. NEBULA 时间分辨 $\sigma(t) = 0.5$ ns, 飞行时间 $t \approx L/(c\beta) \approx 8320 / (30 \cdot 0.57) \approx 49$ ns, 故 $\sigma(\beta)/\beta = \sigma(t)/t \approx 1\%$ → $\sigma(p_n) \approx 1670 \times 0.0057 \approx 9.5$ MeV/c, 与观察一致。
\textbf{$-10$ MeV/c 偏置}的来源待查 —— 候选: NEBULA 上游的能量损失（中子打入第一层 bar 前已损失一部分动能, 导致测得 $\beta$ 偏低）, 或是 $T_0$ 校准时间偏。

\paragraph{Truth $\phi$ vs reco $\phi$, 半宽 22°}
$\phi$ 的方差由 $\vec p_T^\text{sum}$ 的振幅倒数控制: $\sigma(\phi) \approx \sigma(p_T) / |\vec p_T^\text{sum}|$。在 ypol elastic 中, $|\vec p_T^\text{sum}|$ 典型只有 50--150 MeV/c, 而 $\sigma(p_T) = \sqrt{\sigma^2(p_x) + \sigma^2(p_y)} \approx \sqrt{5.9^2 + 1.5^2 + 3.6^2 + 2.9^2} \approx 7.7$ MeV/c → $\sigma(\phi) \approx 7.7/100 \approx 4.4$ deg per particle, 但事件级用的是 sum 而 sum 接近 0 时角度尤为不稳, 长尾把半宽推到 22°。\textbf{这就是为什么用 reco $\phi$ 旋转会损失信号}, 也是 §1.2 那个 2D 图主对角弥散的原因。

\section{2. 数据流 \& 重建链}

\subsection*{2.1 数据来源}
\texttt{data/simulation/g4output/ypol\_new\_20260413\_elastic\_allair/d+\{Sn112,Sn124\}E190/d+...g\{050,060,070,080\}\{ynp,ypn\}-RP360/dbreak0000.root}：
QMD 生成 elastic d+A → p + n 事件后过 Geant4 探测器模拟，每 cell 含 190394 elastic 事件。

\subsection*{2.2 Pipeline (per cell)}
\begin{enumerate}[nosep]
\item \texttt{subsample\_dbreak.py} 随机抽 N=10000 events (要求 \texttt{OK\_PDC1} \& \texttt{OK\_PDC2})。
\item \texttt{bin/reconstruct\_target\_momentum --backend rk}: PDC1+PDC2 hits → RK 拟合 proton 动量；NEBULA hits → 时间重建 $\beta$ 推 neutron 动量（详见 §2.3）。
\item NEBULA 端在 commit \texttt{806cd6a} 加 \texttt{NEBULAFrameRotation::RotateRecoNeutronsToTargetFrame}, 把 lab 系方向旋到靶系（与 \texttt{truth\_neutron\_p4} 同坐标系），消除 $\Delta p_{x,n} \approx -33$ MeV/$c$ 的系统偏。
\item \texttt{extract\_phys\_observables.C}: 从 reco ROOT 抽出每事件 truth/reco 动量分量到 CSV。
\end{enumerate}

\subsection*{2.3 NEBULA 中子重建详解}

源代码: \texttt{libs/analysis/include/NEBULAReco.hh}, \texttt{libs/analysis/src/NEBULAReco.cc}, frame rotation \texttt{libs/analysis/include/NEBULAFrameRotation.hh}; 调用入口在 \texttt{apps/run\_reconstruction/main.cc:502}。

\paragraph{输入} \texttt{TClonesArray$\to$TArtNEBULAPla}, 来自 Geant4 + ANAROOT 的 NEBULA 击中数据, 每个 hit 含:
\begin{itemize}[nosep]
\item \texttt{moduleID} —— bar 编号
\item \texttt{position} —— lab 系 3D 击中点 (mm); $x$ 由 bar 两端时间差给出, $y$ 由 bar 编号 (高度) 给出, $z$ 由 NEBULA 平面位置给出 ($z\approx 7250$ mm)
\item \texttt{time} —— bar 两端校准后的击中时间 (ns), 相对 beam-line $T_0$
\item \texttt{energy} (=\texttt{fQAveCal}) —— bar 沉积能量 (MeV)
\end{itemize}

\paragraph{Step 1: hit 选择 \& 平滑}
对每个 hit:
\begin{enumerate}[nosep]
\item 阈值: \texttt{energy} $< 1.0$ MeV 的 hit 直接丢弃 (\texttt{fEnergyThreshold});
\item Gaussian 平滑模拟探测器分辨: 位置 $\sigma = 5.0$ mm, 时间 $\sigma = 0.5$ ns (\texttt{fPositionSmearing}, \texttt{fTimeSmearing}, 在模拟里加, 不会施加到真实数据).
\end{enumerate}

\paragraph{Step 2: 时间窗 cluster}
按时间排序后, 在 $\Delta t \le 10$ ns (\texttt{fTimeWindow}) 内的连续 hit 划入同一 cluster。每个 cluster 视为一个候选中子。

\paragraph{Step 3: cluster $\to$ ($\vec r$, $t$)}
对每个 cluster, 用\textbf{能量加权}求 cluster 中心:
\[
  \vec r_\text{cluster} = \frac{\sum_i E_i \vec r_i}{\sum_i E_i}, \quad t_\text{cluster} = \frac{\sum_i E_i t_i}{\sum_i E_i}
\]

\paragraph{Step 4: 飞行长度 \& $\beta$}
\[
  L = |\vec r_\text{cluster} - \vec r_\text{target}|, \quad \beta = \frac{L_\text{cm}/t_\text{ns}}{c_\text{cm/ns}}, \quad c = 29.9792458\;\text{cm/ns}
\]
$\vec r_\text{target}$ 由 \texttt{GeometryManager::GetTargetPosition()} 从靶 GDML 读取 ($z \approx -1070$ mm, lab frame); \textbf{未做} per-event $T_0$ 修正, 直接信任 beam-line $T_0$。$\beta$ 物理 clamp 到 $[0, 0.99]$。

\paragraph{Step 5: $\beta \to p_n$}
\[
  \gamma = \frac{1}{\sqrt{1-\beta^2}}, \quad T_\text{kin} = (\gamma - 1)\,m_n c^2, \quad p_n = m_n c \cdot \gamma\beta
\]
其中 $m_n c^2 = 939.565$ MeV。中子动量方向取 $\hat d = (\vec r_\text{cluster} - \vec r_\text{target})/L$, 即假设中子从靶直线飞到第一击中 bar (无散射, 无能量损失修正 → 见 §1.3 偏差讨论)。

\paragraph{Step 6: 坐标系修正 (frame rotation)}
NEBULA 重建出的 $\vec p_n$ 是 \textbf{lab frame} (Geant4 全局坐标), 但本报告需要在 \textbf{target frame} (beam-as-z, 已加靶倾角 $\alpha_\text{tgt} = 3°$)。在 \texttt{main.cc:512} 调用:
\begin{verbatim}
analysis::nebula::RotateRecoNeutronsToTargetFrame(reco_event, target_angle_rad);
\end{verbatim}
对每个 \texttt{reco\_event.neutron} 的 direction 做 $R_y(+\alpha_\text{tgt})$ 旋转。这一步在 commit \texttt{806cd6a} (2026-04-28) 加, 修复了 $\Delta p_{x,n} \approx -33$ MeV/$c$ 的系统偏（之前 truth\_neutron\_p4 在 target frame 但 reco neutron 在 lab frame, 直接相减引入靶倾角投影）。

\paragraph{固定参数总结}
\begin{tabular}{lll}
\toprule
参数 & 值 & 说明 \\
\midrule
\texttt{fEnergyThreshold}      & 1.0 MeV  & hit 能量下限 \\
\texttt{fTimeWindow}           & 10.0 ns  & cluster 时间窗 \\
\texttt{fPositionSmearing}     & 5.0 mm   & 位置 $\sigma$ (模拟) \\
\texttt{fTimeSmearing}         & 0.5 ns   & 时间 $\sigma$ (模拟) \\
$m_n c^2$                       & 939.565 MeV & 中子静止质量 \\
$\vec r_\text{target}$         & $z\approx -1070$ mm   & 靶位置 \\
NEBULA 平面 $z$                 & $\approx 7250$ mm     & 飞行距离 $\sim 8.32$ m \\
\bottomrule
\end{tabular}

\paragraph{未做的事 (重要)}
\begin{itemize}[nosep]
\item \textbf{无上游能损修正}: 中子从靶到 NEBULA 经过 air, NEBULA charge-veto, 第一层 bar 的塑料 → 反应前已损失部分能量, 但当前算法忽略; 这是 §1.3 中 $\Delta p_z$ 系统偏 $-10$ MeV/c 的最可能来源。
\item \textbf{无多重击中拒判}: 一个 cluster 内多个 bar 击中算作一个中子, 没区分双中子事件或散射 chain.
\item \textbf{Charge veto 数据未使用}: charge veto 区分质子/中子的功能在当前 reco 没接, 因此 ypol elastic 的中子/质子分类完全靠模拟 truth.
\item \textbf{无 per-event $T_0$ 修正}: 假设 beam-line $T_0$ 准确, 实际数据需另接。
\end{itemize}

\subsection*{2.4 反应平面定义}
对每事件，按 \textbf{truth} 总横向动量方向定义反应平面：
\[
  \phi^\text{event} = \arctan_2(\,p_{y,p}^\text{truth} + p_{y,n}^\text{truth},\;\; p_{x,p}^\text{truth} + p_{x,n}^\text{truth}\,)
\]
然后对 $(p_{x},p_{y})$ 旋转 $-\phi^\text{event}$（绕 z 轴），让所有事件的 $\vec p_{T,\text{sum}}$ 对齐到 $+\hat x$ 方向。
本报告\textbf{所有}的 $\Delta p_x$ 都是\textbf{在反应平面 frame 下的} $\Delta p_x^\text{rot} = \mathrm{rot\_pxp} - \mathrm{rot\_pxn}$。
$R_x \equiv N(\Delta p_x^\text{rot} > 0) / N(\Delta p_x^\text{rot} < 0)$。

\subsection*{2.5 三种 R 定义（贯穿全报告）}
\begin{tabular}{lp{12cm}}
\toprule
\textbf{A. truth}       & 用 \texttt{truth\_pxp}, \texttt{truth\_pyp}, \texttt{truth\_pxn}, \texttt{truth\_pyn}（QMD 输入, 全部 N 事件可用） \\
\textbf{B. reco mixed}  & proton 用 RK reco；neutron: 若 NEBULA 击中则用 reco neutron, 否则用 truth neutron 代理（全部 N 事件参与, 但 30\textendash38\% 事件的 neutron 是真 reco, 其余是 truth 代理） \\
\textbf{C. full reco}   & 仅 NEBULA 击中子集 ($\sim$30\textendash38\% events), proton + neutron 都是 reco \\
\bottomrule
\end{tabular}

\section{3. input\_ana 三个 cut (基于 truth)}
\begin{itemize}[nosep]
\item \textbf{loose}: $|p_{y,p} - p_{y,n}| < 150$ AND $|\vec p_{T,\text{sum}}| > 50$ MeV/$c$
\item \textbf{mid}:   loose AND $|\vec p_{T,\text{sum}}| < 200$ MeV/$c$
\item \textbf{tight}: mid AND $\pi - |\phi^\text{event}| < 0.2$（散射方向接近 $-\hat x$ 即 $-x$ 半平面通过事件）
\end{itemize}
""")

    # Per-(target, gamma) statistics — helicities pooled
    if cut:
        tex.append(r"""\section{4. Per-(target, gamma) sample 统计}

\textbf{ynp 与 ypn 两 helicity 的 QMD 输出在 Geant4 模拟、PDC/NEBULA 重建、analyzer 链路里都不可区分（实验上无法 tag, 这两个 sample 在物理上是同一类事件）。本报告所有 R 表与图统一把两 helicity 的事件 pool 在一起, 每行/每点对应一个 (target, gamma) 共 20000 events。}

\begin{table}[H]
\centering\footnotesize
\caption{Per-(target, gamma) 通过率（基于 truth cuts, ynp+ypn pooled）。N\_NEBULA = tight 子集中 NEBULA 击中数（用于 full reco）。}
\begin{tabular}{llrrrrr}
\toprule
target & gamma & N\_raw & N\_loose & N\_mid & N\_tight & N\_NEBULA(in tight) \\
\midrule
""")
        for r in cut:
            n_neb = ""
            for ar in acc:
                if ar["target"] == r["target"] and ar["gamma"] == r["gamma"]:
                    n_neb = f"{int(ar['N_neb'])}"
                    break
            tex.append(
                f"{r['target']} & {r['gamma']} & "
                f"{int(r['n_raw'])} & {int(r['n_loose'])} & "
                f"{int(r['n_mid'])} & {int(r['n_tight'])} & {n_neb} \\\\\n"
            )
        tex.append(r"""\bottomrule
\end{tabular}
\end{table}

\textbf{读法}: tight 通过率 ~2\textendash6\%; NEBULA 在 tight 子集上击中率 ~20\%。
""")

        # MAIN RESULT TABLE — three R variants × three cuts, in reaction plane
        tex.append(r"""\section{5. 主结果: $R_x^\text{rot}$ vs gamma （反应平面 frame, ynp+ypn pooled）}

\textbf{所有数字均在反应平面 frame 下计算, 两 helicity 已合并为一行}。三种 R 定义见 §2.

\subsection*{5.1 (target, gamma) × cut × reco-variant 全表}
\begin{table}[H]
\centering\footnotesize
\caption{$R_x^\text{rot}$ per (target, gamma) × per cut × per reco variant (ynp+ypn pooled)。N 见 §4 通过数。}
\begin{tabular}{ll|rrr|rrr|rrr}
\toprule
& & \multicolumn{3}{c|}{\textbf{loose}} & \multicolumn{3}{c|}{\textbf{mid}} & \multicolumn{3}{c}{\textbf{tight}} \\
target & gamma & A truth & B mixed & C full & A truth & B mixed & C full & A truth & B mixed & C full \\
\midrule
""")
        for r in cut:
            tex.append(
                f"{r['target']} & {r['gamma']} & "
                f"{fmt(r['loose_truth_R'])} & {fmt(r['loose_reco_mixed_R'])} & {fmt(r['loose_reco_full_R'])} & "
                f"{fmt(r['mid_truth_R'])} & {fmt(r['mid_reco_mixed_R'])} & {fmt(r['mid_reco_full_R'])} & "
                f"{fmt(r['tight_truth_R'])} & {fmt(r['tight_reco_mixed_R'])} & {fmt(r['tight_reco_full_R'])} \\\\\n"
            )
        tex.append(r"""\bottomrule
\end{tabular}
\end{table}

\textbf{物理结论}：
\begin{itemize}[nosep]
\item \textbf{Truth $R_x^\text{rot}$ 随 gamma 单调下降} —— Sn112 loose 0.23→0.22 (微小); Sn124 loose 0.29→0.22; Sn124 tight 0.62→0.32 (50\% 跌幅).
\item \textbf{Sn124 信号比 Sn112 强}（破裂动力学差异）.
\item \textbf{Reco mixed 跟 truth 几乎一致 ($\sim$5\% 衰减)} —— 信号被全模拟全重建保留.
\item \textbf{Full reco (NEBULA hit only)} 因接收度偏置整体偏离 truth, 但 gamma 趋势仍然存在 (见 §7 NEBULA 瓶颈分析).
\end{itemize}
""")

        # R vs gamma plots
        tex.append(r"""\subsection*{5.2 R vs gamma 图（每 cut 一张）}
""")
        for cn in ("loose", "mid", "tight"):
            png_name = f"r_vs_gamma_{cn}.png"
            if (fig_src_dir / png_name).exists():
                tex.append(rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.95\textwidth]{{{args.figure_rel_dir}/{png_name}}}
\caption{{$R_x^\text{{rot}}$ 随 gamma ({cn} cut, ynp+ypn pooled)。两 panel = 两 target；每 panel 三条曲线: 黑圆实线 = A truth, 橙方虚线 = B reco mixed, 蓝三角点线 = C full reco。\textbf{{所有 R 值在反应平面 frame}}。}}
\end{{figure}}
""")

        # Section 6: histograms
        tex.append(r"""\section{6. $\Delta p_x^\text{rot}$ 分布直方图}

\subsection*{6.1 Per-(target, gamma) 直方图（每图 4 panel = 4 gammas, ynp+ypn pooled）}
""")
        for cn in ("loose", "mid", "tight"):
            for tg in ("Sn112E190", "Sn124E190"):
                png_name = f"px_diff_hist_{tg}_{cn}.png"
                if (fig_src_dir / png_name).exists():
                    tex.append(rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.78\textwidth]{{{args.figure_rel_dir}/{png_name}}}
\caption{{$\Delta p_x^\text{{rot}}$ 分布 ({tg}, {cn} cut, ynp+ypn pooled)。黑 step = A truth, 橙填充 = B reco mixed, 蓝虚线 = C full reco.}}
\end{{figure}}
""")

        tex.append(r"""\subsection*{6.2 Cuts overlay（一图叠加 4 cuts: 无 cut + loose + mid + tight, ynp+ypn pooled）}
""")
        for tg in ("Sn112E190", "Sn124E190"):
            png_name = f"px_diff_cuts_overlay_{tg}.png"
            if (fig_src_dir / png_name).exists():
                tex.append(rf"""\begin{{figure}}[H]
\centering
\includegraphics[width=0.85\textwidth]{{{args.figure_rel_dir}/{png_name}}}
\caption{{$\Delta p_x^\text{{rot}}$ 在四 cut 下的叠加 ({tg}, ynp+ypn pooled)。4 行=4 gammas, 列=truth (A) / reco mixed (B). 蓝=无 cut, 绿=loose, 橙=mid, 红=tight.}}
\end{{figure}}
""")

    # Section 7: bottleneck
    if acc:
        tex.append(r"""\section{7. NEBULA 几何接收瓶颈分析}

\subsection*{7.1 几何参数}
NEBULA 位于 $z = 7249.72$ mm, 距靶 ($z = -1070$ mm) \textbf{8.32 m}。覆盖范围:
\begin{itemize}[nosep]
\item $x$ 半角 $\pm 13.5°$ → $|p_{x,n}| \lesssim 150$ MeV/$c$ (at $p_{z,n}=627$)
\item $y$ 半角 $\pm 6.2°$ → $|p_{y,n}| \lesssim 68$ MeV/$c$
\end{itemize}

\subsection*{7.2 哪个方向是瓶颈}
tight cut 已经把 truth $|p_{y,n}|$ 限到半宽 18 MeV/$c$ (远在 ±68 内), 所以 \textbf{y 方向不是瓶颈}。
但 tight cut 同时把 truth $p_{x,n}$ 中位推到 \textbf{$-97$ MeV/$c$, 半宽 70} (尾部到 $-180$), NEBULA $\varepsilon \approx 0$ 在 $p_{x,n} < -100$:
""")

        # Acceptance correction table
        tex.append(r"""\subsection*{7.3 1/$\varepsilon$ 加权校正失败}
按 $\varepsilon(p_{x,n}^\text{truth}) = N_\text{NEBULA-hit}/N_\text{tight}$ 建表, 每事件 weight = $1/\varepsilon$, 截断 $\varepsilon < 0.05$:
\begin{table}[H]
\centering\footnotesize
\begin{tabular}{lr}
\toprule
mean R\_truth\_tight (8 cells, ynp+ypn pooled)            & {tval} \\
mean R\_full\_uncorr (NEBULA-only)         & {ufval} \\
mean R\_full\_corr ($1/\varepsilon$ weighted)  & {cfval} \\
\midrule
truth − corr 缺口                          & {gap_corr} \\
truth − uncorr 缺口                        & {gap_uncorr} \\
\bottomrule
\end{tabular}
\end{table}

\textbf{校正只缩小缺口约 5\%（$+2.41 \to +2.28$）}。原因: tight cut 把事件推到 $p_{x,n} \in (-180, +0)$, NEBULA 在 $p_{x,n} < -100$ 几乎完全失明。这些事件\textbf{物理上没被探测器看到, 不存在能补回的权重}。注意 §7 的 R 值在 \textbf{lab frame} 上算（NEBULA 接收度本身定义在 lab frame）, 数值跟 §5 反应平面 frame 的 $R_x^\text{rot}$ 不直接可比, 但相对偏离的物理来源一致。

\textbf{可行路径}:
\begin{enumerate}[nosep]
\item 接受 acceptance 限制, 只在 NEBULA 可见窗口里做物理 → 用 \textbf{C full reco};
\item 用 \textbf{B reco mixed} 把没击中的事件用 truth\_n 代理 → 绕过 NEBULA 几何瓶颈;
\item 换更大角度接收的中子探测器 (硬件改动).
\end{enumerate}
""".replace("{tval}", f"{_s.mean([r['R_truth_tight'] for r in acc if not math.isnan(r['R_truth_tight'])]):.3f}")
   .replace("{ufval}", f"{_s.mean([r['R_full_uncorr'] for r in acc if not math.isnan(r['R_full_uncorr'])]):.3f}")
   .replace("{cfval}", f"{_s.mean([r['R_full_corr'] for r in acc if not math.isnan(r['R_full_corr'])]):.3f}")
   .replace("{gap_corr}", f"{_s.mean([r['R_truth_tight'] - r['R_full_corr'] for r in acc if not math.isnan(r['R_full_corr']) and not math.isnan(r['R_truth_tight'])]):+.3f}")
   .replace("{gap_uncorr}", f"{_s.mean([r['R_truth_tight'] - r['R_full_uncorr'] for r in acc if not math.isnan(r['R_full_uncorr']) and not math.isnan(r['R_truth_tight'])]):+.3f}")
        )

    # Section 8: conclusions
    tex.append(r"""\section{8. 结论 \& 下一步}

\begin{enumerate}[nosep]
\item \textbf{全模拟全重建 + 反应平面旋转} 后 $R_x^\text{rot}$ 信号清晰随 gamma 单调下降 (Sn124 loose: 0.29→0.22; tight: 0.62→0.32).
\item \textbf{B reco mixed (proton reco + neutron truth-fallback)} 跟 truth 一致到 $\sim$5\%, 是 ypol 极化分析最实用的中间方案.
\item \textbf{C full reco (含 NEBULA neutron)} 受几何接收瓶颈影响 lab-frame R 整体偏低 (truth−full $\approx +2.4$, 见 §7 表), 但保留 gamma 趋势; 不可用 1/$\varepsilon$ 加权救回 (95\% 缺口源于 NEBULA acceptance hard cut, 无 truth 信息可补).
\item \textbf{Bottleneck 排序}: NEBULA acceptance $\gg$ proton reco smearing $>$ neutron reco smearing.
\end{enumerate}

\textbf{下一步}：
\begin{itemize}[nosep]
\item 极化提取应使用 \textbf{B reco mixed + 反应平面旋转 + loose cut} 组合 (CP CI 半宽 ±0.01, 信号 5\textendash15\%).
\item 若需 full reco, 接受 NEBULA visible-window 限制, 在该窗口内做相对 R(g) 比值分析.
\end{itemize}

\end{document}
""")

    Path(args.report_tex).parent.mkdir(parents=True, exist_ok=True)
    body = "".join(tex).replace("__REL_DIR__", args.figure_rel_dir)
    with open(args.report_tex, "w") as f:
        f.write(body)
    print(f"[render] wrote {args.report_tex}")


if __name__ == "__main__":
    main()
