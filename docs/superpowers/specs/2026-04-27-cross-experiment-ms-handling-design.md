# Design: Cross-experiment multiple-scattering handling survey (SAMURAI proton/fragment arms + GSI)

> **Language clarification**: One **Chinese-only** monolingual document is produced first. An English sibling is *not* produced unless explicitly requested later. This is a deliberate departure from the `pdc_reco_literature_and_air_ms_gap_20260426` precedent, where two siblings were produced together; here we accept the asymmetry to keep scope tight.

- **Date**: 2026-04-27
- **Author**: TBT
- **Status**: design approved (brainstorming complete)
- **Type**: Documentation (cross-experiment literature survey, MS-handling axis)
- **Inputs**:
  - `docs/reports/reconstruction/methods/pdc_reco_literature_and_air_ms_gap_20260426_zh.tex` — sibling SAMURAI-only PDC literature memo (this doc complements, does not duplicate)
  - `docs/reports/reconstruction/momentum_reco/ms_ablation/ms_ablation_mixed_20260422_zh.md` — current MS ablation finding (rev2); supplies the σ_y gap that motivates this survey
  - `docs/reports/reconstruction/momentum_reco/rk/rk_error_analysis_deep_dive_20260426_zh.pdf` — RK error budget reference (cited, not re-derived)

## 1. Goal

Produce one Chinese-only reference document that surveys, across six charged-particle tracking experiments at ~50 MeV/u – 1 GeV/u, **three specific axes** of multiple-scattering handling:

1. **Hardware / material-budget axis** — magnet-cavity vacuum vs air, exit-window choices, tracking-gas selection, beam-pipe geometry.
2. **Algorithm axis** — Kalman + MS covariance term, GEANE / Runge-Kutta transport with MS, look-up / unfolding corrections.
3. **Simulation / calibration axis** — MC framework choice (GEANT3/GEANT4/Fluka/custom), MS-model tuning approach (off-the-shelf vs tuned to data), residual unfolding, validation observables (residual widths, hit-position distributions, MC vs data closure tests).

The six experiments in scope are:

- **SAMURAI proton arm (PDC1/PDC2)** — primary, our chain
- **SAMURAI fragment arm (FDC1/FDC2 + heavy-ion Bρ + ToF reconstruction)** — adjacent, same dipole
- **R3B / LAND (GSI → FAIR)** — closest external analogue; Si-tracker + drift chambers downstream of GLAD
- **FOPI (GSI)** — CDC + barrel solenoid; mostly 1–2 GeV/u with tail to ~150 MeV/u
- **HADES (GSI)** — MDC + toroidal magnet; p beams up to 3.5 GeV, mostly lepton pairs
- **ALADIN-EOS (GSI)** — TPC + ALADIN dipole; ~250 MeV/u and up, heavy-ion fragmentation

The document answers: "How does each experiment handle MS — at the hardware, algorithm, and simulation/calibration levels — and which of their choices are candidates for closing our σ_y(PDC2) gap?"

## 2. Non-goals

- No new MS ablation runs, no new RK code, no Kalman implementation work, no new GEANT4 MS tuning runs (the simulation/calibration axis is surveyed from the literature only; we do not re-tune anything in scope here).
- No coverage of the **SAMURAI neutron arm** (NEBULA / NeuLAND) — that is a separate ToF reconstruction problem; deferred to a later memo.
- No coverage of energies <50 MeV/u or >1 GeV/u; the comparison band is set to bracket our 190 MeV/u (627 MeV/c) protons.
- No commitment to a specific Region A vacuum/air decision — that remains an open project question handled in the sibling PDC literature memo.
- No English sibling document on initial delivery; can be produced later if needed.
- No re-derivation of σ_p_target propagation; cite the RK error deep-dive instead.

## 3. Format & location

| Item | Decision |
|---|---|
| Format | LaTeX → PDF; follow `pdc_reco_literature_and_air_ms_gap_20260426_zh` precedent |
| Output | **One Chinese-only document** — no English sibling on initial delivery |
| Location | `docs/reports/reconstruction/methods/` (same dir as sibling PDC memo) |
| Filename | `cross_experiment_ms_handling_20260427_zh.tex` → `.pdf` |
| Build artifacts | `.aux`, `.log`, etc. gitignored at repo root (already configured) |
| Estimated length | 25–35 PDF pages (3 axes × 6 experiments) |

LaTeX is chosen over markdown for the same reason as the sibling memo: the document will be referenced by collaborators / advisor (formal status).

## 4. Scope decisions (from brainstorm)

- **SAMURAI internal coverage** — choice A+C from brainstorm round 1: proton arm (PDC1/PDC2 + dipole + Exit Window) **and** fragment arm (FDC1/FDC2 + heavy-ion Bρ + ToF). Neutron arm (B) deferred.
- **External experiments** — choice B from brainstorm round 2: GSI three (FOPI, HADES, ALADIN-EOS) **plus** R3B/LAND. The sibling PDC memo explicitly excludes R3B/LAND because it is outside SAMURAI; this memo intentionally re-includes it because the cross-experiment MS-handling axis is the spine here, and R3B is the closest external analogue (GLAD dipole + downstream tracker is the most direct cousin of SAMURAI dipole + PDC).
- **Comparison axes** — choice D from brainstorm round 3 (hardware + algorithm), **plus** the simulation/calibration axis (C) re-included on user instruction. All three axes are surveyed; only the original two are subdivided into per-element subsections (the C-axis is information-poor in the public literature and may rely heavily on `[unverified]` flags + open questions, see R5).

A short paragraph in §1 (TL;DR) and §2 (why this matters) will explicitly note that **the sibling PDC memo's R3B/LAND exclusion does not apply here**, and explain why — to avoid giving collaborators the impression of inconsistent citation discipline across the two memos.

## 5. Document outline

### §1 TL;DR (~1 PDF page)

Short prose summary + 8–10 bullet takeaways:

1. Per-experiment one-liner on the **hardware** choice (vacuum vs air, dominant material element).
2. Per-experiment one-liner on the **algorithm** choice (Kalman+MS / GEANE / RKF / lookup).
3. Per-experiment one-liner on the **simulation/calibration** choice (MC framework, MS tuning approach, validation observable).
4. Summary line: which experiments accept large air MS and compensate algorithmically vs which evacuate to remove it.
5. Summary line: which experiments validate MS via residual closure tests vs treat MS as an off-the-shelf MC default.
6. Forward reference to §7 implications.
7. Explicit note: scope differs from sibling PDC memo (R3B/LAND included here; neutron arm excluded).

### §2 Why this matters (~1 PDF page)

- Recap σ_y(PDC2) gap from sibling PDC memo §2 (no new numbers; cite the table).
- State the question this memo answers: of the six experiments in scope, which hardware/algorithm/simulation choices are candidates for closing our gap?
- Explicit scope-difference paragraph relative to the sibling PDC memo (§4 above).

### §3 Hardware / material-budget axis (~5–7 PDF pages)

Each subsection covers one hardware element across all six experiments. Closing each subsection: a row of the cross-experiment table.

- §3.1 Magnet-cavity vacuum vs air — cavity volume, vacuum spec, evidence (paper + page).
- §3.2 Exit windows — material, thickness, cited mg/cm² where available.
- §3.3 Tracking-detector gas — gas mixture, cell geometry, cited mg/cm² where available.
- §3.4 Beam pipe / target chamber upstream of tracker — material, thickness.
- §3.5 Cross-experiment hardware comparison table — rows = 6 experiments, columns = (cavity status, exit window, tracker gas, beam pipe, total mg/cm² in proton/fragment flight path). Every cell carries citation; missing cells labeled "not reported".

### §4 Algorithm axis (~4–6 PDF pages)

Each subsection covers one algorithmic family across all six experiments. Closing the section: cross-experiment algorithm comparison table.

- §4.1 Straight-line / RK back-tracking with **no explicit MS term** — baseline approach; note which experiments use this and how they justify it (typically: MS budget already small enough, or σ dominated by detector resolution).
- §4.2 Kalman filter with MS process-noise covariance — list which experiments implement this; cite the specific MS process-noise model (Highland formula, Lynch-Dahl, GEANT4-derived lookup).
- §4.3 GEANE / RKF transport with MS-aware step adaptation — typically ROOT/GENFIT/PandaRoot frameworks; list adopters.
- §4.4 Look-up / per-event MS correction applied **during** the track fit (drawn from MC tables) — rare but used at lower energies; list adopters. **Distinct from §5.3** which is post-fit; see R7.
- §4.5 Cross-experiment algorithm table — rows = 6 experiments, columns = (back-track method, MS treatment in fit, framework, citation). Every cell carries citation.

### §5 Simulation / calibration axis (~3–4 PDF pages)

Each subsection covers one MC/calibration sub-question across all six experiments. Closing the section: cross-experiment simulation table.

- §5.1 MC framework chosen — GEANT3, GEANT4, Fluka, custom; cite the framework version where reported. Note which experiments rely on framework defaults vs custom physics lists.
- §5.2 MS-model tuning approach — three families: (a) off-the-shelf (default GEANT MS, no tuning); (b) tuned to data (per-detector MS scale factor or material-budget adjustment); (c) parameterized lookup from external MS measurements. State which family each experiment belongs to and cite the evidence (paper / thesis / framework manual).
- §5.3 Residual unfolding / per-event posterior MS correction — typically only at lower energies or higher-precision measurements; list which experiments do this and cite the algorithm reference. (Distinct from §4.4 which is about the *fit-time* lookup; §5.3 is about *post-fit* correction from MC.)
- §5.4 Validation observables — what each experiment compares MC vs data on as evidence that MS is correctly modeled (residual widths, hit-position distributions, target-vertex closure, mass-resolution closure). Cite the figure/table in each paper.
- §5.5 Cross-experiment simulation/calibration table — rows = 6 experiments, columns = (MC framework, MS tuning family, residual-unfolding yes/no, primary validation observable, citation). Every cell carries citation; many cells expected to read "not reported" (see R5).

### §6 Per-experiment fact sheets (~6 PDF pages, 1 page each)

Compact one-page summary per experiment, ordered by relevance to our chain:

1. SAMURAI proton arm (PDC1/PDC2)
2. SAMURAI fragment arm (FDC1/FDC2)
3. R3B/LAND (GSI/FAIR)
4. ALADIN-EOS (GSI)
5. FOPI (GSI)
6. HADES (GSI)

Each fact sheet contains:
- Geometry sketch (text-only — magnet, tracker positions, target).
- Particle and energy band.
- Hardware choices (with pointers to §3 table rows).
- Algorithm choices (with pointers to §4 table rows).
- Simulation/calibration choices (with pointers to §5 table rows).
- Reported σ_p/p or equivalent performance figure (with citation; "not reported" if absent).
- One-sentence relevance statement: how this experiment's choice maps onto our σ_y(PDC2) gap.

### §7 Implications for our σ_y gap (~2–3 PDF pages)

Exploratory tone — list candidate paths, do not commit. Per `feedback_experimental_reality_claims` memory, do not assert simulation-side conditions are physical reality.

- §7.1 Hardware paths — for each external experiment, what would porting their hardware choice imply for our σ_y(PDC2)? (e.g., R3B's evacuated GLAD chamber + thin Kapton windows as a comparison point).
- §7.2 Algorithm paths — would adding MS process-noise to our RK chain (§4.2 family) close the σ_y gap, partially close it, or just shift it from σ_y to a properly-quantified posterior? Honest discussion, no commitment.
- §7.3 Simulation/calibration paths — what does each experiment's MS-tuning approach (§5.2) and validation observable (§5.4) imply for *our* ms_ablation pipeline? Concretely: (a) is our use of GEANT4 default MS sufficient given that R3B/HADES tune MS to data, or should we add a tuning step? (b) what residual / closure observable should we add to our ms_ablation chain to mirror the validation step that other experiments publish? Honest discussion, no commitment.
- §7.4 Combined paths — practical bundles: (a) keep current hardware + add MS to RK; (b) Region A → vacuum + keep current RK; (c) add MS data-tuning step to our GEANT4 chain; (d) combinations. Each bundle linked to a *qualitative directional estimate* of where it would push our σ_y (e.g., "would reduce σ_y toward R3B's reported value of X mm"). **No new σ values are derived in this memo**; cited values come from sibling PDC memo / ms_ablation memo / per-experiment publications.

### §8 Open questions / could-not-verify (~1 PDF page)

- Items where the literature is silent on a relevant detail (e.g., FOPI's air budget at 150 MeV/u operations; whether ALADIN-EOS published an MS-tuning closure test).
- Items that require collaborator contact rather than literature search (e.g., ALADIN-EOS internal target-chamber drawings, internal R3B PandaRoot tuning notes).
- Items flagged `[unverified]` in §9.

### §9 References (~2–3 PDF pages)

References grouped by experiment, each entry carrying a verification flag inherited from the sibling PDC memo's flag system:

- §9.1 SAMURAI proton arm (PDC1/PDC2) — refer to sibling PDC memo's §7 for the canonical list; only re-cite items used in this memo.
- §9.2 SAMURAI fragment arm (FDC1/FDC2)
- §9.3 R3B / LAND (GSI/FAIR) — Panin 2016 PLB, Atar 2018 PRL, Lehr 2023 PhD, Kahlbow 2019 PhD (R3B chapters), R3B TDRs.
- §9.4 ALADIN-EOS (GSI) — Pochodzalla, Trautmann, Schüttauf-style references; ALADIN TPC NIM papers.
- §9.5 FOPI (GSI) — FOPI Collaboration NIM A paper, Reisdorf reviews.
- §9.6 HADES (GSI) — HADES NIM A 2009 design paper, Adamczewski-Musch HADES tracking papers.
- §9.7 Cross-cutting algorithm (GENFIT, GEANE, MS models) — Lynch-Dahl 1991, Highland 1975, Rauch & Schlüter (GENFIT), GEANE manual.
- §9.8 Cross-cutting MC framework (for §5) — GEANT4 NIM A 2003 / 2016 papers, GEANT3 manual, Fluka manual; per-experiment framework version where reported in primary paper.

Every entry: full citation + DOI/URL/repository handle + `[verified]` / `[unverified]` flag. `[unverified]` items also appear in §8's could-not-verify list.

## 6. Citation discipline (HARD CONSTRAINT)

Inherited verbatim from sibling PDC memo. Per memory `feedback_data_must_have_sources`:

**Every numerical value, performance figure, design specification, or factual claim must carry an explicit source.**

Operating rules:

- For literature numbers in tables: cite by paper DOI / thesis URL / "infer from §X of Y" / "not reported".
- For project-internal numbers: cite by memo path + section.
- For prose claims about "how the literature does X": cite the specific paper or thesis section, not generic "the literature".
- If a number is genuinely not in any source: write "not reported"; never invent or estimate without explicit "infer/estimate" labeling.
- When in doubt: keep the placeholder rather than write an unsourced number.

Web verification: every external citation must be accessible via DOI/arXiv/repository URL at writing time. Items that cannot be web-verified are flagged `[unverified]` and listed in §8. The sibling PDC memo's commit `71f9797` (web-verify all citations) sets the precedent for this discipline.

## 7. Figures & tables inventory

| ID | Type | Source | Section |
|---|---|---|---|
| Tbl 3.5 | Cross-experiment hardware table (6 exp × 5 hardware columns) | per-paper citations | end of §3 |
| Tbl 4.5 | Cross-experiment algorithm table (6 exp × 4 algorithm columns) | per-paper citations | end of §4 |
| Tbl 5.5 | Cross-experiment simulation/calibration table (6 exp × 5 columns: MC framework, MS tuning family, residual unfolding, validation observable, citation) | per-paper citations | end of §5 |
| Fig 6.x | Per-experiment geometry sketches (text-only ASCII or simple TikZ) | per-paper figures, redrawn | §6 each |
| Tbl 7.4 | Combined-paths candidate table (4 bundles × 4 columns: hardware change / algorithm change / simulation change / candidate σ_y) | synthesized from §3, §4, §5, sibling PDC memo | §7 |

No new analysis figures (no σ plots, no RK output). Geometry sketches in §6 may be ASCII art if TikZ is too heavy.

## 8. Risks & open questions

- **R1**: HADES at 1.25–3.5 GeV with lepton-pair priorities may be a poor fit for the table — many cells will read "not applicable" or "not reported". Mitigation: keep HADES in scope but clearly mark non-applicable cells; do not force-fit.
- **R2**: FOPI's MS handling at the low-energy tail (~150 MeV/u) is not well-published — most FOPI MS literature targets the 1–2 GeV/u peak. Mitigation: honest "not reported" labels per the citation discipline (markdown §6); note explicitly in §8 open questions.
- **R3**: R3B/LAND inclusion may surprise collaborators who saw the sibling PDC memo exclude it. Mitigation: explicit scope-difference paragraph in §1 TL;DR and §2 (per §4 above).
- **R4**: The cross-experiment hardware table requires per-experiment mg/cm² values that are often only computable from drawings, not directly cited. Mitigation: where mg/cm² is not reported, cite the geometry source and write "infer/compute" with the formula; never invent values.
- **R5**: The simulation/calibration axis (§5) is **information-poor in the public literature** — most experiments treat MC tuning as a working-group internal matter rather than publishing per-detector MS scale factors or framework-version tuning notes. Many cells in Tbl 5.5 are expected to read "not reported" or `[unverified]`. Mitigation: (a) accept that §5 is the thinnest of the three axes; (b) honest "not reported" labels per §6 citation discipline; (c) explicitly list the gaps in §8 open questions; (d) avoid implying that absence of evidence is evidence of absence — a missing tuning paper does not mean the experiment did not tune.
- **R6**: Page budget overrun. The sibling PDC memo is ~30 pages. With 6 experiments × 3 axes, this memo could reach 40+ pages. Mitigation: hard-cap §6 fact sheets at 1 page each; if §3, §4, or §5 grows beyond budget, split a sub-axis into §8 open questions rather than expanding the body. Hard ceiling: 35 PDF pages.
- **R7**: Conflating §4.4 (fit-time MS lookup) with §5.3 (post-fit MC unfolding). These are distinct mechanisms — §4.4 is a step in the track fit; §5.3 is a correction applied to the fit's output. Mitigation: explicit one-sentence definition at the top of each subsection; cross-reference between the two when an experiment uses both.

## 9. Cross-references & dependencies

- Upstream:
  - `pdc_reco_literature_and_air_ms_gap_20260426_zh.tex` — sibling SAMURAI-only PDC memo; this memo cites its §2 σ table and §4 gap analysis without re-deriving them.
  - `ms_ablation_mixed_20260422_zh.md` — supplies the σ_y(PDC2) numbers in §2.
  - `rk_error_analysis_deep_dive_20260426_zh.pdf` — supplies σ_p_target propagation values cited in §7 (implications).
- Downstream:
  - Future SAMURAI neutron-arm MS memo (the B-arm deferred from this scope).
  - Any future "would adding MS process-noise to RK help" implementation work — this memo's §7.2 supplies the literature precedent for that decision.
  - Any future ms_ablation MC-tuning or residual-closure work — this memo's §5 and §7.3 supply the literature precedent for what "validating MS in MC" looks like in comparable experiments.

## 10. Future English sibling (deferred)

If an English sibling is needed later, follow the sibling PDC memo's two-document parity protocol verbatim (independent monolingual files; byte-identical numerical content; same-commit update rule). Filename would be `cross_experiment_ms_handling_20260427.tex` (no `_zh` suffix). Not a current deliverable.
