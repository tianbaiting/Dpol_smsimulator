# Reco Momentum → Target Frame Alignment (2026-04-19)

## Problem

`apps/run_reconstruction/main.cc` writes two quantities into its output tree that live in **different coordinate frames**, producing a systematic `px` bias:

- `truth_proton_p4 = particle.fMomentum` (from `reader.GetBeamData()`): the event generator's primary, stored in the input "beam-as-Z-axis" frame — i.e. the **target frame**.
- `reco_proton_p4 = reco_result.p4_at_target`: reconstructed from PDC hits and a magnetic field, both of which live in the Geant4 world frame — i.e. the **lab frame**.

The forward simulation rotates direction by `dir.rotateY(-angle_tgt)` (`DeutPrimaryGeneratorAction.cc:68`) inside a local copy, never touching `gBeamSimDataArray`. The NN training pipeline (`scripts/reconstruction/nn_target_momentum/build_dataset.C:58`) already compensates with `momentum.RotateY(-target_angle_rad)`. Main reco does not.

For a target tilt α, residuals become:
- `dpx ≈ p_x(cos α − 1) − p_z sin α`
- `dpy ≈ 0` (py is invariant under Ry)
- `dpz ≈ p_x sin α + p_z(cos α − 1)`

At α = 3° and `p_z ≈ 1 GeV/c`, the false px bias is ≈ −52 MeV/c.

## Decision

**Rotate reco output to target frame at the end of `ProcessSingleFile`.** Truth stays as-is; all downstream comparisons (`reco − truth`) and all plots use the target frame.

Scope: only `apps/run_reconstruction/main.cc`. `PDCMomentumReconstructor` stays lab-frame internally (RK, Fisher, MCMC all remain consistent with magnetic field + PDC geometry). Tests in `tests/analysis/test_PDCMomentumReconstructor.cc` are unaffected.

## Implementation

`angle_tgt = geometry.GetTargetAngleRad()` passed to `ProcessSingleFile`. Before pushing branches:

```cpp
// Apply R_y(+α) to go from lab → target frame.
TVector3 p = reco_result.p4_at_target.Vect();  p.RotateY(angle_tgt);
const double e = reco_result.p4_at_target.E();
// Rotate 3x3 momentum covariance: Σ' = R Σ R^T
auto rot_cov = RotateCovarianceY(reco_result.momentum_covariance, angle_tgt);
auto rot_post = RotateCovarianceY(reco_result.posterior_momentum_covariance, angle_tgt);
```

Per-branch write:
- `reco_proton_px/py/pz/e`: `(p.X(), p.Y(), p.Z(), e)` (py unchanged by construction)
- `reco_proton_px_sigma`: `sqrt(rot_cov[0])`
- `reco_proton_py_sigma`: unchanged (py axis invariant, yy element unchanged)
- `reco_proton_pz_sigma`: `sqrt(rot_cov[8])`
- `reco_proton_p_sigma`: unchanged (|p| is rotation-invariant)
- `reco_proton_{px,py,pz}_lower68/upper68/lower95/upper95`:
  - **Gaussian fallback**: center ± 1.0σ'/1.96σ'. Documented as approximation (original asymmetric MC/MCMC quantiles cannot be rotated without re-sampling; samples are not retained in `RecoResult`).
  - `p` intervals: unchanged.

Helper: `std::array<double,9> RotateCovarianceY(const std::array<double,9>&, double α)` — inline in an anonymous namespace in `main.cc`.

Truth path (`main.cc:580-594`) untouched.

## Re-run & regenerate plots

1. Rebuild: `./build.sh` (incremental via `cmake --build build`).
2. Identify the input ROOT set that produced the existing PNGs (`docs/reports/reconstruction/*.png`). Most recent dated plots are 2026-04-17.
3. Rerun `bin/reconstruct_target_momentum` on that input set.
4. Locate the plot scripts (candidates: `scripts/analysis/*.C`, `scripts/analysis/plot_*.py`) and regenerate figures to overwrite the stale PNGs.

## Docs to update

`docs/reports/reconstruction/`:

1. **New or updated "Coordinate frame" section** in the two primary status docs:
   - `rk_reconstruction_status_20260416_zh.tex`
   - `rk_reconstruction_bugfix_and_error_analysis_20260416.md`
   - `pdc_reconstruction_uncertainty_detailed.tex`
   - `靶点动量重建算法研究.md`

   Content:
   - Define the **lab frame** (Geant4 world frame; z = nominal beam axis in SAMURAI hall).
   - Define the **target frame** (beam-as-Z convention used by the event generator; differs from lab by a rotation `R_y(−α)` where α = target tilt).
   - State explicitly: **all truth + reco comparisons in this report use the target frame**.
   - Note the fix (post-2026-04-19): reco momentum is now rotated to target frame at the write stage in `main.cc`. Prior 2026-04-17 plots were affected by a systematic px bias.

2. **Per-figure data-provenance captions** in the status docs: for each histogram, add a one-line caption note stating:
   - Source tree / ROOT file pattern
   - Branch(es) used (e.g. `reco_proton_px - truth_proton_p4.X()`)
   - Any event selection / cut
   - The run/date of the underlying reconstruction pass

## Credible-interval semantics trade-off

Gaussian-rotated `px/py/pz_lower68` etc. are self-consistent for the Fisher/Laplace posterior (central Gaussian), but lose the asymmetry of the MCMC/MC posterior. The memo already observes systematic under-coverage at 68% — rotating does not fix that; it is a separate issue driven by dE/dx non-Gaussianity and the (u,v,p) linearization. This spec explicitly does not attempt to repair non-Gaussian intervals.

## Non-goals

- Changing `PDCMomentumReconstructor` internals
- Changing NN backend (already consistent via `build_dataset.C`)
- Regenerating NN training data
- Touching legacy `libs/analysis/include/TargetReconstructor.hh`
