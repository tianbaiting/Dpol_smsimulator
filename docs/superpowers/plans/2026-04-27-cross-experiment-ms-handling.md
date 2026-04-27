# Cross-experiment MS handling survey — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce one Chinese-only LaTeX → PDF reference document at `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (25–35 pages) surveying multiple-scattering handling across SAMURAI proton arm (PDC), SAMURAI fragment arm (FDC), R3B/LAND, FOPI, HADES, and ALADIN-EOS along three axes (hardware / algorithm / simulation).

**Architecture:** Single self-contained LaTeX document built with XeLaTeX + ctex (matches sibling memo `pdc_reco_literature_and_air_ms_gap_20260426_zh.tex`). Three cross-experiment "axis" sections (§3/§4/§5) each close with a 6-row comparison table; six per-experiment "fact-sheet" pages (§6) point back to those table rows. Implications (§7) and open questions (§8) are downstream of §3-§6. References (§9) are accumulated incrementally during §3-§5 then organized in Task 9.

**Tech Stack:** LaTeX (XeLaTeX engine), ctex package for Chinese, latexmk for builds. Research uses WebFetch + WebSearch for citation verification per the citation discipline (every number/method statement must carry a web-verifiable DOI/arXiv/repository URL; unverifiable items flagged `[unverified]`).

**Spec:** `docs/superpowers/specs/2026-04-27-cross-experiment-ms-handling-design.md` (commit `c13be9e`).

---

## Conventions used throughout this plan

- **Working directory:** all paths are relative to `/home/tian/workspace/dpol/smsimulator5.5/` unless noted.
- **Build command:** `cd docs/reports/reconstruction/methods && latexmk -xelatex cross_experiment_ms_handling_20260427_zh.tex`. Re-run after each section edit; expected: zero LaTeX errors, zero unresolved `\ref`/`\cite`.
- **Commit style:** mirror sibling memo's history — `docs(reco): write §X.Y …` (lowercase, imperative, Chinese section title may appear in the body).
- **Citation discipline (HARD):** per spec §6, every numerical claim and method statement must carry a footnote with DOI/arXiv/URL. Use the `\footnote{\label{fn:xxx}…}` + `\footref{fn:xxx}` pattern from the sibling memo for repeat citations. Items not web-verifiable get `[unverified]` flag inline AND an entry in §8.
- **"Not reported":** when a paper does not state a value, write the literal string "未报道" (or "not reported" in any English fragment). Never invent numbers; never estimate without explicit "推断/estimate" labeling.
- **Six experiments, fixed order:** SAMURAI proton arm → SAMURAI fragment arm → R3B/LAND → ALADIN-EOS → FOPI → HADES. Use this order in every comparison table and every fact sheet for consistency.

---

## File structure

| Path | Purpose | Created in |
|---|---|---|
| `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` | Sole deliverable (LaTeX source) | Task 1 |
| `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.pdf` | Built PDF (committed alongside `.tex` per directory precedent) | Task 1, refreshed in every later task |

Build artifacts (`.aux`, `.log`, `.out`, `.toc`, `.fdb_latexmk`, `.fls`, `.xdv`) are gitignored at repo root. No other files created.

---

## Task 1: Set up LaTeX skeleton and verify build

**Files:**
- Create: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex`

- [ ] **Step 1.1: Create the skeleton file**

Write the file with this exact content (preamble matches sibling memo, then empty `\section{}` headers in the §1–§9 order from spec §5):

```latex
% Build: cd docs/reports/reconstruction/methods && latexmk -xelatex cross_experiment_ms_handling_20260427_zh.tex
\documentclass[a4paper, 11pt]{article}

\usepackage[margin=2.4cm]{geometry}
\usepackage{ctex}            % 中文支持
\usepackage{amsmath,amssymb}
\usepackage{booktabs}
\usepackage{multirow}
\usepackage{siunitx}
\usepackage{xcolor}
\usepackage{hyperref}
\usepackage{enumitem}
\usepackage{float}
\usepackage{graphicx}
\usepackage{longtable}
\usepackage{tabularx}

\definecolor{goodgreen}{RGB}{34,139,34}
\definecolor{warnred}{RGB}{200,50,50}

\hypersetup{
  colorlinks=true,
  linkcolor=blue!60!black,
  citecolor=blue!60!black,
  urlcolor=blue!60!black,
}

\title{\textbf{同能区带电粒子径迹重建中的 \\ 多重散射处理对照 \\ （SAMURAI 质子/碎片臂 + GSI）}}
\author{TBT \\ RIKEN 仁科中心 / DPOL 实验}
\date{2026 年 4 月 27 日}

\begin{document}
\maketitle
\tableofcontents
\clearpage

\section{摘要 TL;DR}
% [content from Task 10 — written LAST]

\section{为什么这件事重要}
% [content from Task 5]

\section{硬件 / 物质预算切面}
% [content from Task 2]

\subsection{磁谱仪腔：真空 vs 空气}
\subsection{出射窗}
\subsection{径迹探测器气体}
\subsection{束流管 / 上游靶室}
\subsection{跨实验硬件对照表}

\section{算法切面}
% [content from Task 3]

\subsection{直线 / RK 反推：无显式 MS 项}
\subsection{Kalman 滤波 + MS 过程噪声协方差}
\subsection{GEANE / RKF 带 MS 自适应步长}
\subsection{查表 / 反卷积修正（事后）}
\subsection{跨实验算法对照表}

\section{模拟 / 校准切面}
% [content from Task 4]

\subsection{所选 MC 框架}
\subsection{MS 模型调谐方法}
\subsection{残差反卷积 / 事后 MS 修正}
\subsection{验证观测量}
\subsection{跨实验模拟/校准对照表}

\section{逐实验事实卡}
% [content from Task 6]

\subsection{SAMURAI 质子臂 (PDC1/PDC2)}
\subsection{SAMURAI 碎片臂 (FDC1/FDC2)}
\subsection{R3B / LAND (GSI/FAIR)}
\subsection{ALADIN-EOS (GSI)}
\subsection{FOPI (GSI)}
\subsection{HADES (GSI)}

\section{对我们 \texorpdfstring{$\sigma_y$}{sigma\_y} 缺口的启示}
% [content from Task 7]

\subsection{硬件路径}
\subsection{算法路径}
\subsection{模拟/校准路径}
\subsection{组合路径}

\section{开放问题 / 待核证}
% [content from Task 8]

\section{参考文献}
% [content from Task 9]

\subsection{SAMURAI 质子臂 (PDC1/PDC2)}
\subsection{SAMURAI 碎片臂 (FDC1/FDC2)}
\subsection{R3B / LAND (GSI/FAIR)}
\subsection{ALADIN-EOS (GSI)}
\subsection{FOPI (GSI)}
\subsection{HADES (GSI)}
\subsection{交叉算法（GENFIT, GEANE, MS 模型）}
\subsection{交叉 MC 框架}

\end{document}
```

- [ ] **Step 1.2: Build the skeleton to verify LaTeX environment works**

Run: `cd docs/reports/reconstruction/methods && latexmk -xelatex cross_experiment_ms_handling_20260427_zh.tex`

Expected: clean build, produces `cross_experiment_ms_handling_20260427_zh.pdf`, exit code 0. If errors, fix immediately — common issues: missing `ctex` package (need `texlive-lang-chinese`), missing XeLaTeX (need `texlive-xetex`).

- [ ] **Step 1.3: Open PDF and verify TOC shows all sections**

Run: `ls -la cross_experiment_ms_handling_20260427_zh.pdf` (just confirm file exists and is non-trivial; it will be ~3 pages, mostly empty TOC).

Verify the TOC lists all 9 sections in the right order with the right Chinese headings.

- [ ] **Step 1.4: Commit skeleton**

```bash
git add docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex \
        docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.pdf
git commit -m "docs(reco): add skeleton for cross-experiment MS handling memo"
```

---

## Task 2: Write §3 Hardware / material-budget axis

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (replace `% [content from Task 2]` placeholder + fill all five §3.x subsections)

**Approach:** for each of the five subsections, gather per-experiment data via WebFetch/WebSearch on the canonical references listed below, draft the prose with inline footnote citations, then close with the cross-experiment table at §3.5.

**Canonical reference seeds** (every entry here is the *starting point*; the agent must verify the actual paper contains the claimed value before citing):

| Experiment | Primary references for hardware/material |
|---|---|
| SAMURAI proton arm | Kobayashi 2013 NIM B 317, Sato 2013 IEEE TAS 23, Shimizu 2013 NIM B 317 (cavity vacuum), `Detector-PDC.pdf` (RIKEN tech note), sibling memo `pdc_reco_literature_and_air_ms_gap_20260426_zh.tex` §3.1 |
| SAMURAI fragment arm | Kobayashi 2013 NIM B 317 §3.5 (FDC1/FDC2), Otsu 2016 NIM B 376, Kondo 2023 Nature methods section |
| R3B/LAND | Heil 2022 EPJ A 58 (R3B detector overview), Aumann 2007 PPNP 53, Reifarth 2017 EPJ Web Conf (GLAD magnet), Panin 2016 PLB 753, Atar 2018 PRL 120 |
| ALADIN-EOS | Schüttauf 1996 NPA 607 (ALADIN), Pochodzalla 1995 PRL 75, Trautmann 2007 NPA 787, Lukasik 1997 NIM A 397 (TPC) |
| FOPI | Ritman 1995 NPA 583 (FOPI Collaboration overview), Reisdorf 2010 NPA 848, FOPI Collaboration NIM A 1995 design paper |
| HADES | Agakishiev 2009 EPJ A 41 (HADES design paper), Adamczewski-Musch 2017 EPJ A 53 (tracking/MS) |

- [ ] **Step 2.1: Research §3.1 magnet-cavity vacuum vs air for all six experiments**

For each experiment, use WebFetch on the primary reference and extract: (a) does the magnet have an evacuated chamber? (b) cavity volume / dimensions? (c) operating vacuum spec (Pa or mbar)? (d) cite exact paper section.

For SAMURAI proton arm, you can rely on sibling memo §2.1 + §4.3 + Shimizu 2013 (already verified there). For others, web-verify each.

Save findings as inline notes in your scratchpad — they will be transcribed in Step 2.2.

- [ ] **Step 2.2: Draft §3.1 prose**

Replace the empty `\subsection{磁谱仪腔：真空 vs 空气}` body with prose covering all six experiments in fixed order. Each experiment gets 2–4 sentences with at least one citation. Example structure for one experiment:

```latex
\subsection{磁谱仪腔：真空 vs 空气}
本节比较六个实验的偶极/超导磁铁腔在运行期间的真空状态。

\textbf{SAMURAI 质子臂。} SAMURAI 偶极磁铁腔（\texttt{vchamber\_box}）的设计真空度为几 Pa，由 Shimizu 等 2013 年原始调试论文记录\footnote{\label{fn:shimizu2013_v2}Y.~Shimizu et al., Nucl. Instr. Meth. B \textbf{317}, 739--742 (2013), DOI: \href{https://doi.org/10.1016/j.nimb.2013.08.051}{10.1016/j.nimb.2013.08.051}.}。腔体内部尺寸 $\approx 2{\times}2{\times}3.5$~m$^3$\footref{fn:sato2013_v2}（见姊妹备忘录 §3.1 同一来源）。

\textbf{SAMURAI 碎片臂。} 碎片臂共用同一偶极腔；FDC1/FDC2 安装在腔下游，与 PDC 的 Region~A 真空配置一致\ldots

\textbf{R3B / LAND.} \ldots [continue]
```

- [ ] **Step 2.3: Build after §3.1**

Run: `cd docs/reports/reconstruction/methods && latexmk -xelatex cross_experiment_ms_handling_20260427_zh.tex`. Expected: clean build, no unresolved citations.

- [ ] **Step 2.4: Commit §3.1**

```bash
git add docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex \
        docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.pdf
git commit -m "docs(reco): write §3.1 cavity vacuum/air across six experiments"
```

- [ ] **Step 2.5–2.16: Repeat 2.1–2.4 for §3.2 (exit windows), §3.3 (tracking-detector gas), §3.4 (beam pipe / upstream target chamber)**

Use the same per-experiment-then-build-then-commit pattern, one commit per subsection. For each subsection:

1. Research: extract material, thickness, mg/cm² per experiment from primary refs. Where mg/cm² is computable from material+thickness (Bethe-Bloch tables OR PDG `X_0` values), compute it inline with the formula in a footnote and label `[计算]`. Where it is not derivable, write `未报道`.
2. Draft prose: 2–4 sentences per experiment, fixed order, footnote citations.
3. Build: latexmk; verify no unresolved.
4. Commit: `docs(reco): write §3.X <subsection-title-in-en>`

- [ ] **Step 2.17: Draft §3.5 cross-experiment hardware comparison table**

Replace the empty `\subsection{跨实验硬件对照表}` body with a `longtable` (sibling memo precedent) with columns: 实验 | 腔状态 | 出射窗 | 探测器气体 | 束流管 | 总 mg/cm² (proton 飞行路径). Six rows, fixed order. Each cell carries a `\footref{}` to the citation introduced in §3.1–§3.4.

For the "总 mg/cm²" column: sum of computable contributions, with one footnote explaining the sum, OR `未报道` if any contributing element is unreported.

- [ ] **Step 2.18: Build + commit §3.5**

```bash
git add docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex \
        docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.pdf
git commit -m "docs(reco): write §3.5 cross-experiment hardware comparison table"
```

---

## Task 3: Write §4 Algorithm axis

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (fill §4.1–§4.5)

**Algorithm reference seeds:**

| Topic | Reference seeds |
|---|---|
| §4.1 No-MS RK / straight-line | Sibling memo §3.2 (SAMURAI proton arm with `analysis_pdc_reco`), Otsu 2016 |
| §4.2 Kalman + MS process noise | Frühwirth 1987 NIM A 262 (original Kalman for tracking), Lynch-Dahl 1991 NIM B 58 (MS model), Highland 1975 NIM 129 (MS model), R3B Panin 2016 PLB §3 (Kalman use), HADES Adamczewski-Musch 2017 EPJ A 53 (tracking) |
| §4.3 GEANE / RKF | Fontana 2007 IEEE NSS (GENFIT), Rauch & Schlüter 2014 J. Phys. Conf. Ser. (GENFIT2), GEANE manual (CERN), PandaRoot reference paper |
| §4.4 Look-up / unfolding fit-time | Less common — search for "MS-aware track refit" or "per-event MS correction" in FOPI / ALADIN-EOS papers |

- [ ] **Step 3.1: Research and draft §4.1 (no explicit MS term)**

For each experiment, identify whether their published track-fit uses a Kalman filter with MS process noise, or a plain RK / straight-line back-track. Cite the paper section that states the method. If a paper says "Kalman filter" but does not specify whether MS process noise is included, write "Kalman 实现，论文未明示 MS 协方差项是否纳入 [unverified]" and add to the §8 list.

For SAMURAI proton arm, cite our sibling memo §3.2 and `libs/analysis_pdc_reco/` source: as of writing, our RK chain does NOT include explicit MS process noise (verify this by reading `libs/analysis_pdc_reco/include/PDCRecoRuntime.hh` and confirming no `processNoise` / `MS` term appears in the fit class). Cite the source file path.

- [ ] **Step 3.2: Build + commit §4.1**

Same build+commit pattern as Task 2.

```bash
git commit -m "docs(reco): write §4.1 baseline RK/straight-line back-tracking comparison"
```

- [ ] **Step 3.3–3.10: Repeat the research-draft-build-commit cycle for §4.2 (Kalman+MS), §4.3 (GEANE/RKF), §4.4 (lookup/unfolding fit-time)**

For §4.2 specifically: cite the *exact MS process-noise model* each experiment uses (Highland formula vs Lynch-Dahl vs GEANT4-derived lookup vs other). If unspecified, `[unverified]` and §8 entry.

For §4.4: this category may have zero adopters among our six experiments. If so, write one paragraph "本类别在六个实验中无明确采用者；最相近的是 X 实验的事后查表（见 §5.3 而非本节）" and explain why — this distinguishes §4.4 (fit-time lookup) from §5.3 (post-fit MC unfolding) per spec R7.

- [ ] **Step 3.11: Draft §4.5 cross-experiment algorithm comparison table**

`longtable`, columns: 实验 | 反推方法 | 拟合中 MS 处理 | 框架 | 主要引用. Six rows, fixed order. Cells with citation `\footref{}`.

- [ ] **Step 3.12: Build + commit §4.5**

```bash
git commit -m "docs(reco): write §4.5 cross-experiment algorithm comparison table"
```

---

## Task 4: Write §5 Simulation / calibration axis

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (fill §5.1–§5.5)

**WARNING:** per spec R5, this section is information-poor. Many cells will be `未报道` or `[unverified]`. Do not invent. The point is to honestly map what *is* publicly known.

**Reference seeds:**

| Topic | Reference seeds |
|---|---|
| §5.1 MC framework | Each experiment's primary detector paper (cited in §3 already); search for "GEANT4" / "GEANT3" / "Fluka" + experiment name |
| §5.2 MS tuning | Hard to find in primary refs; search PhD theses (Kahlbow 2019, Lehr 2023, Atar 2015 GSI report, Panin 2012 TUprints 3170); look for "material budget tuning" / "MS scale factor" |
| §5.3 Residual unfolding | Look for "MS unfolding" / "residual correction" in FOPI/ALADIN heavy-ion papers (lower-energy regime more likely to need it) |
| §5.4 Validation observable | Search each experiment's primary paper figures for "residual width" / "data-MC comparison" plots near tracking-performance discussion |

- [ ] **Step 4.1: Research §5.1 MC framework chosen**

For each experiment, identify the MC framework (GEANT3 / GEANT4 / Fluka / custom) and version where reported. Distinguish between "framework used for design simulations" and "framework used for offline data analysis" — they may differ.

- [ ] **Step 4.2: Draft §5.1**

Six paragraphs (one per experiment), fixed order. For each: framework, version (if reported), brief note on whether they use defaults or a custom physics list (cite exact paper section if reported; `未报道` otherwise).

- [ ] **Step 4.3: Build + commit §5.1**

```bash
git commit -m "docs(reco): write §5.1 MC framework choices across six experiments"
```

- [ ] **Step 4.4–4.13: Repeat for §5.2 (MS tuning approach), §5.3 (residual unfolding), §5.4 (validation observables)**

For §5.3: explicitly state in the opening sentence that this is *post-fit MC unfolding*, not the *fit-time lookup* of §4.4 (per spec R7). If an experiment uses both, cross-reference §4.4.

For §5.4: cite the figure or table number in each paper where MC vs data closure is shown for a tracking-residual-related observable. If the paper has no such figure, `未报道`.

- [ ] **Step 4.14: Draft §5.5 cross-experiment simulation/calibration table**

`longtable`, columns: 实验 | MC 框架 | MS 调谐家族 | 残差反卷积 (是/否/未报道) | 主要验证观测量 | 主要引用. Six rows, fixed order.

Many cells expected to be `未报道` per R5. That is the honest answer.

- [ ] **Step 4.15: Build + commit §5.5**

```bash
git commit -m "docs(reco): write §5.5 cross-experiment simulation/calibration table"
```

---

## Task 5: Write §2 Why this matters

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (fill §2)

This is a short section (~1 page) with no new research — it links to the sibling memo and states the question.

- [ ] **Step 5.1: Draft §2 prose**

Replace the empty section body with three paragraphs:

1. Recap σ_y(PDC2) gap: cite sibling memo §2 table 1 directly (e.g., `\footnote{见 \texttt{docs/reports/reconstruction/methods/pdc\_reco\_literature\_and\_air\_ms\_gap\_20260426\_zh.tex} §2.}`) — give the three numbers (12.83–14.93 mm all-air, 6.80–8.21 mm A=vacuum, ~1.0 mm all-vacuum) but **do not re-derive**; cite the table.
2. State the question: of the six experiments in scope, which hardware/algorithm/simulation choices are candidates for closing this gap?
3. Scope-difference paragraph: explicitly note that the sibling memo excluded R3B/LAND; this memo intentionally re-includes them because the cross-experiment MS-handling axis is the spine here, and R3B is the closest external analogue. Cite the sibling memo's §7.5 (excluded list) for transparency.

- [ ] **Step 5.2: Build + commit §2**

```bash
git commit -m "docs(reco): write §2 why this matters + scope-difference vs sibling memo"
```

---

## Task 6: Write §6 Per-experiment fact sheets

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (fill §6.1–§6.6)

Each fact sheet ≤1 PDF page (hard cap per spec R6). Mostly synthesizes pointers to §3/§4/§5 table rows + cites a reported σ_p/p (or `未报道`).

- [ ] **Step 6.1: Draft fact sheet template (will be reused for all six)**

Each fact sheet has this structure:

```latex
\subsection{<实验名称>}
\textbf{几何}（文字示意）。<one paragraph: dipole/solenoid type, magnet position, target position, tracker positions; 4–6 sentences max>

\textbf{粒子与能区。} <particle, energy band, primary physics use case>

\textbf{硬件选择}（详见 §3.5 表 \ref{tab:hardware}）。<bullet list pointing to §3.x rows for cavity, exit window, gas, beam pipe>

\textbf{算法选择}（详见 §4.5 表 \ref{tab:algorithm}）。<bullet list>

\textbf{模拟/校准选择}（详见 §5.5 表 \ref{tab:simulation}）。<bullet list; many items 未报道>

\textbf{实测性能。} <reported σ_p/p or σ_x or σ_y, with citation; 未报道 if absent>

\textbf{与我们的相关性。} <one sentence on what this experiment's choice maps onto for our σ_y(PDC2) gap>
```

You will need to add `\label{tab:hardware}`, `\label{tab:algorithm}`, `\label{tab:simulation}` to the §3.5/§4.5/§5.5 tables retroactively (Step 6.2).

- [ ] **Step 6.2: Add `\label` to §3.5/§4.5/§5.5 tables**

Edit the three tables to include `\label{tab:hardware}`, `\label{tab:algorithm}`, `\label{tab:simulation}` immediately after the `\caption{}`.

- [ ] **Step 6.3: Write §6.1 SAMURAI proton arm fact sheet**

Use the template. Most data already in §3/§4/§5 — just synthesize. For "实测性能" cite Kobayashi 2013 design 0.8% / commissioning 1/1500 / our σ_y 13 mm (cite ms_ablation memo).

- [ ] **Step 6.4: Build + commit §6.1**

```bash
git commit -m "docs(reco): write §6.1 SAMURAI proton arm fact sheet"
```

- [ ] **Step 6.5–6.14: Repeat steps 6.3–6.4 for §6.2 SAMURAI fragment arm, §6.3 R3B/LAND, §6.4 ALADIN-EOS, §6.5 FOPI, §6.6 HADES**

For HADES specifically: many "硬件选择" / "算法选择" cells will be marked "本实验主轴非带电粒子径迹动量重建（侧重轻子对），与本备忘录主轴关系有限" — that is OK and per spec R1.

---

## Task 7: Write §7 Implications for our σ_y gap

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (fill §7.1–§7.4)

Exploratory tone — list candidate paths, do not commit. Per memory `feedback_experimental_reality_claims`, do not assert simulation conditions are physical reality.

- [ ] **Step 7.1: Draft §7.1 hardware paths**

For each external experiment with a "tighter" hardware choice than ours, write 1–2 sentences on what porting that choice would imply for our σ_y(PDC2). Example: "若我们采用 R3B 在 GLAD 腔体下游使用的 X mg/cm² 出射窗（见 §3.2 第三行），则 Region~B 的 σ_B 贡献将从当前 7.43~mm 缩减至 …（定性）。具体值不在本备忘录推导（依据姊妹备忘录纪律）。"

Do not derive new σ values. Use phrases like "推断方向" / "定性减小" / "与 R3B 报告的 X mm 接近". Cite the source σ value.

- [ ] **Step 7.2: Build + commit §7.1**

```bash
git commit -m "docs(reco): write §7.1 hardware paths for closing σ_y gap"
```

- [ ] **Step 7.3: Draft §7.2 algorithm paths**

Address the question: would adding MS process-noise to our RK chain (§4.2 family) close the σ_y gap, partially close it, or just shift it from σ_y to a properly-quantified posterior? Honest discussion. Cite §4.2 entries showing which experiments do this and what they report.

Caveat explicitly: adding MS to the fit reduces *χ²* and reduces *bias*, but does not necessarily reduce the *posterior σ* — in some regimes it just makes the wider σ statistically defensible. Cite Frühwirth 1987 or a Kalman textbook for this point.

- [ ] **Step 7.4: Build + commit §7.2**

```bash
git commit -m "docs(reco): write §7.2 algorithm paths (Kalman + MS vs current RK)"
```

- [ ] **Step 7.5: Draft §7.3 simulation/calibration paths**

Two specific actionable questions per spec §7.3:
(a) is our use of GEANT4 default MS sufficient given that R3B/HADES tune MS to data, or should we add a tuning step?
(b) what residual / closure observable should we add to our ms_ablation chain to mirror the validation step that other experiments publish?

For each, give an honest "what we'd need to do" without committing. Cite §5.2 and §5.4 entries.

- [ ] **Step 7.6: Build + commit §7.3**

```bash
git commit -m "docs(reco): write §7.3 simulation/calibration paths"
```

- [ ] **Step 7.7: Draft §7.4 combined paths + table 7.4**

Bundle (a)–(d) per spec §7.4. Build a `longtable` with columns: 组合 | 硬件改动 | 算法改动 | 模拟改动 | σ_y 定性方向. Four rows for the four bundles.

For "σ_y 定性方向": qualitative direction only ("→ R3B 量级 X mm" / "→ 设计指标方向" / "→ 持平"). No new derived values.

- [ ] **Step 7.8: Build + commit §7.4**

```bash
git commit -m "docs(reco): write §7.4 combined paths + bundles table"
```

---

## Task 8: Write §8 Open questions / could-not-verify

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (fill §8)

- [ ] **Step 8.1: Harvest "未报道" and `[unverified]` items from §3, §4, §5**

Run: `grep -nE '未报道|\[unverified\]' docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex`

For each match, note: which experiment, which axis (§3/§4/§5), what specifically is missing, and what would resolve it (web search? collaborator contact? PhD thesis fetch?).

- [ ] **Step 8.2: Draft §8 prose**

Three categories of bullet items per spec:
1. Items where the literature is silent (e.g., FOPI air budget at 150 MeV/u, ALADIN-EOS MS-tuning closure test).
2. Items requiring collaborator contact (e.g., R3B PandaRoot internal tuning notes, ALADIN-EOS internal chamber drawings).
3. Items flagged `[unverified]` in §9.

Each item: one sentence, with `\footref{}` back to where it was first marked.

- [ ] **Step 8.3: Build + commit §8**

```bash
git commit -m "docs(reco): write §8 open questions (harvested from §3-§5 placeholders)"
```

---

## Task 9: Curate §9 References

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (fill §9.1–§9.8)

References were accumulated incrementally as inline footnotes during §3-§5. This task organizes them into a per-experiment reference section with verification flags.

- [ ] **Step 9.1: Extract all `\label{fn:xxx}` from the document**

Run: `grep -nE 'label\{fn:' docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex | sort -u`

Make a working list of all unique `fn:` labels and what they cite.

- [ ] **Step 9.2: Group references by experiment**

Sort the unique citations into eight groups matching §9.1–§9.8 (six per-experiment + cross-cutting algorithm + cross-cutting MC framework). Each ref keeps its `[verified]` or `[unverified]` flag. Sibling memo's §7.X format is the precedent — use the same `\begin{itemize}` + DOI link + flag pattern.

- [ ] **Step 9.3: Draft §9 prose**

Write each subsection as a bulleted list of references in alphabetical order by first author, with the flag at the end. For §9.1 (SAMURAI proton arm), per spec §9.1 the format is "refer to sibling memo's §7 for the canonical list; only re-cite items used in this memo" — so write a one-sentence redirect + a short list of items actually used in this memo.

- [ ] **Step 9.4: Build + commit §9**

```bash
git commit -m "docs(reco): write §9 references organized by experiment with verification flags"
```

---

## Task 10: Write §1 TL;DR

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (fill §1)

Written LAST so it accurately summarizes §3-§9.

- [ ] **Step 10.1: Draft §1 prose**

Follow spec §5 §1 outline: 8–10 bullets, covering:

1. Per-experiment one-liner on **hardware** (vacuum vs air, dominant material element). Six bullets — one per experiment.
2. Per-experiment one-liner on **algorithm** (Kalman+MS / GEANE / RKF / lookup). Six bullets.
3. Per-experiment one-liner on **simulation/calibration**. Six bullets.
4. Summary line: which experiments accept large air MS and compensate algorithmically vs which evacuate to remove it.
5. Summary line: which experiments validate MS via residual closure tests vs treat MS as off-the-shelf MC default.
6. Forward reference to §7 implications.
7. Explicit scope-difference note (R3B/LAND included here; neutron arm excluded).

To keep within ~1 page, condense the 18 per-experiment items into a compact table or three 3-column lists (one per axis).

Every bullet cites at least one footnote — re-use existing `\footref{}` from §3-§5.

- [ ] **Step 10.2: Build + commit §1**

```bash
git commit -m "docs(reco): write §1 TL;DR (3 axes × 6 experiments compact summary)"
```

---

## Task 11: Web-verify all citations

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (only as needed to fix bad URLs/DOIs; replace `[unverified]` with `[verified]` where possible)

This mirrors sibling memo's commit `71f9797` precedent.

- [ ] **Step 11.1: Extract all DOI / arXiv / URL citations**

Run: `grep -oE '(doi.org/[^}]+|arxiv.org/[^}]+|tuprints[^}]+|ribf.riken.jp/[^}]+|nishina.riken.jp/[^}]+)' docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex | sort -u > /tmp/citations.txt`

- [ ] **Step 11.2: WebFetch each citation in batch**

For each URL in `/tmp/citations.txt`, run WebFetch. If the URL resolves (200 OK with the expected paper title in the response), mark `[verified]`. If it 404s, redirects unexpectedly, or returns wrong content, leave as `[unverified]` and add a §8 entry.

When more than 5 URLs need verifying in a batch, dispatch them in parallel as multiple WebFetch tool calls in a single message.

- [ ] **Step 11.3: Update flags inline**

For each citation that web-verified successfully, change its `[unverified]` flag in §9 to `[verified]`. For each that failed, ensure §8 has a corresponding entry.

- [ ] **Step 11.4: Build + commit verification pass**

```bash
git commit -m "docs(reco): web-verify all citations in cross-experiment MS memo"
```

---

## Task 12: Final self-review against design spec

**Files:**
- Modify: `docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex` (only fixes from review)

- [ ] **Step 12.1: Read the design spec end-to-end alongside the deliverable**

Open `docs/superpowers/specs/2026-04-27-cross-experiment-ms-handling-design.md` and the deliverable side-by-side. For every requirement in the spec (§1 goal, each §X.Y subsection promise, every Tbl/Fig in §7 inventory, every Risk mitigation), verify the deliverable contains the corresponding content. Make a checklist.

- [ ] **Step 12.2: Page-budget check**

Open the built PDF. If page count exceeds 35 (spec R6 hard ceiling), trim by:
1. Move "未报道"-heavy bullet items in §3-§5 to §8 open questions instead of cluttering the body.
2. Compact §6 fact sheets to true 1-page-each (per spec R6 mitigation).
3. Do NOT trim §3.5, §4.5, §5.5 tables — they are the spine.

- [ ] **Step 12.3: Citation discipline check**

Run: `grep -nE '[0-9]+\s*(mg/cm²|mm|MeV/c|GeV|MeV/u|keV)' docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex | grep -v footref | grep -v label`

For each numerical line not citing a `\footref{}` or being inside a footnote, verify the citation context comes from a nearby `\footnote{}` in the same sentence. If genuinely uncited, fix by adding a citation or relabeling as `[计算]` / `未报道`.

- [ ] **Step 12.4: §X cross-reference consistency**

Run: `grep -oE '§[0-9]+(\.[0-9]+)?' docs/reports/reconstruction/methods/cross_experiment_ms_handling_20260427_zh.tex | sort -u`

For each `§X.Y` reference, verify the target exists in the doc. (Common bug: a §6.X reference left over from before the renumbering in spec revision `c13be9e`.)

- [ ] **Step 12.5: Final build and verify PDF page count**

Run: `cd docs/reports/reconstruction/methods && latexmk -xelatex cross_experiment_ms_handling_20260427_zh.tex && pdfinfo cross_experiment_ms_handling_20260427_zh.pdf | grep Pages`

Expected: 25–35 pages (spec target). If outside this range, document the deviation in the commit message and consider further trim/expand.

- [ ] **Step 12.6: Final commit**

```bash
git commit -m "docs(reco): finalize cross-experiment MS handling memo (self-review pass)"
```

---

## Self-review of this plan

**1. Spec coverage:**
- Spec §1 Goal (3 axes × 6 experiments) → Tasks 2/3/4 (axes) + Task 6 (per-experiment).
- Spec §2 Non-goals → enforced throughout (no new MS runs, no RK code changes, no neutron arm, no <50 or >1 GeV/u, no English sibling, no σ_p_target re-derivation).
- Spec §3 Format & location → Task 1 (skeleton with correct path/preamble/title).
- Spec §4 Scope decisions → Tasks 2/3/4/6 cover all six experiments per fixed order; R3B re-inclusion is called out in Task 5 (§2 prose).
- Spec §5 Document outline §1-§9 → Tasks 10/5/2/3/4/6/7/8/9 (note §1 written last).
- Spec §6 Citation discipline → Conventions section + Task 11 + Step 12.3.
- Spec §7 Figures & tables inventory → Task 2 (Tbl 3.5), Task 3 (Tbl 4.5), Task 4 (Tbl 5.5), Task 6 (Fig 6.x sketches), Task 7 (Tbl 7.4).
- Spec §8 Risks: R1 HADES → Step 6.5–6.14 acknowledges; R2 FOPI → Task 4 acknowledges; R3 R3B inclusion → Task 5 §2 scope-difference paragraph; R4 mg/cm² → Task 2 inline `[计算]` rule; R5 §5 info-poor → Task 4 warning; R6 page budget → Step 12.2; R7 §4.4 vs §5.3 distinction → Task 3 §4.4 explicit + Task 4 §5.3 explicit.
- Spec §9 Cross-references → already cited in inputs.

**2. Placeholder scan:** No "TBD", "TODO", "implement later". The few "<one paragraph: …>" angle-bracket prompts are intentional — they describe content the agent must research and write, not boilerplate to leave.

**3. Type/name consistency:** Six experiments in fixed order (SAMURAI proton → SAMURAI fragment → R3B/LAND → ALADIN-EOS → FOPI → HADES) used everywhere in §3.5/§4.5/§5.5 tables and §6 fact sheets. Three table labels (`tab:hardware`, `tab:algorithm`, `tab:simulation`) defined in Step 6.2 and used in §6 fact sheets. `\footref{fn:xxx}` repeat-citation pattern matches sibling memo (Step 1.1 commits to it via the preamble import of `hyperref`).
