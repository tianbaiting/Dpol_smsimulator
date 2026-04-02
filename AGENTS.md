# Repository Guidelines

## Project Structure & Module Organization
- `libs/` C++ libraries: `smg4lib/` (Geant4 core), `sim_deuteron_core/` (physics core), `analysis/` (reconstruction and analysis).
- `apps/` executables: `sim_deuteron/` main simulator and `tools/` helpers.
- `tests/` unit/integration/analysis tests (GoogleTest + CTest).
- `configs/` simulation macros and geometry files.
- `scripts/` Python analysis, batch, and visualization utilities.
- `data/` input/output datasets; `docs/` reports; `build/` and `bin/` are generated outputs.

## Reconstruction Architecture
- `libs/analysis_pdc_reco/` is the primary runtime framework for PDC-to-target momentum reconstruction. Put new runtime reconstruction features there.
- `build/bin/reconstruct_target_momentum` is the canonical reconstruction CLI. Treat `build/bin/reconstruct_sn_nn` as an NN-only compatibility wrapper around that entrypoint.
- `build/bin/evaluate_target_momentum_reco` is the canonical evaluator for reconstructed `*_reco.root` outputs. Treat `build/bin/evaluate_reconstruct_sn_nn` as a compatibility wrapper around that evaluator.
- Treat `scripts/reconstruction/nn_target_momentum/` as the NN backend lifecycle for the main reconstruction framework, not as a separate reconstruction architecture.
- `libs/analysis/include/TargetReconstructor.hh` is compatibility-only legacy code. Do not add new features there unless required for regression fixes or migration support.
- Reuse the existing geometry and target-position sources of truth. Do not introduce another private PDC placement convention or ad hoc target-position calculation in new code.
- When moving reconstruction ownership, entrypoints, or file layout, update the corresponding `skills/*/references/path-map.md` and `scripts/check_sync.sh` in the same change.

## Build, Test, and Development Commands

Env: should use micromamba. look at the .envrc.

```bash
micromamba activate anaroot-env
```

Preferred incremental build (does not delete artifacts):
```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_APPS=ON -DBUILD_ANALYSIS=ON -DWITH_ANAROOT=ON
make -j$(nproc)
```
Other useful commands:
```bash
./run_sim.sh configs/simulation/macros/simulation.mac  # run a macro with bin/sim_deuteron
./test.sh                                   # ctest --output-on-failure
cd build && ctest -L unit                   # run labeled tests
```
Note: `./build.sh` performs a clean rebuild and deletes `build/`, `bin/`, and `lib/`. Use it only when you want a full clean build.

## Coding Style & Naming Conventions
- C++20 is standard; clangd uses `clangd_db` compile database (see `.clangd`).
- Indentation is 4 spaces with braces on the same line; match nearby file style.
- Files use `.cc`/`.hh`; tests are named `test_*.cc`.
- Types use PascalCase; member fields often use `f`/`fg` prefixes (e.g., `fMagField`).

## Testing Guidelines
- Framework: GoogleTest discovered by CTest.
- Common labels: `unit`, `performance`, `visualization`, `realdata` (see `tests/analysis`).
- Visualization tests require `SM_TEST_VISUALIZATION=ON`; outputs land in `build/test_output/`.

## Commit & Pull Request Guidelines
- Commit history favors short, imperative, often lowercase messages (e.g., “add …”, “fix …”, “delete …”); keep it concise and specific.
- PRs should describe physics/config changes, link related issues, list commands run (e.g., `cmake ..`, `make`, `ctest`), and include screenshots/plots for visual changes.

## Configuration Tips
- Source `setup.sh` for environment variables; ROOT, Geant4, and optional ANAROOT must be on the path.
- Disable ANAROOT with `cmake .. -DWITH_ANAROOT=OFF` when it is unavailable.
