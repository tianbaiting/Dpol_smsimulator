# Repository Guidelines

## Project Structure & Module Organization
- `libs/` C++ libraries: `smg4lib/` (Geant4 core), `sim_deuteron_core/` (physics core), `analysis/` (reconstruction and analysis).
- `apps/` executables: `sim_deuteron/` main simulator and `tools/` helpers.
- `tests/` unit/integration/analysis tests (GoogleTest + CTest).
- `configs/` simulation macros and geometry files.
- `scripts/` Python analysis, batch, and visualization utilities.
- `data/` input/output datasets; `docs/` reports; `build/` and `bin/` are generated outputs.

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
