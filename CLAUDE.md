# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SMSimulator is a Geant4-based particle physics simulation framework for the SAMURAI/DPOL (Deuteron Polarization) experimental setup at RIKEN. It models deuteron nuclear reactions with detector geometry and reconstruction algorithms.

## Build Commands

```bash
# Activate environment (required)
micromamba activate anaroot-env

# Incremental build (preferred for development)
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_APPS=ON -DBUILD_ANALYSIS=ON -DWITH_ANAROOT=ON
make -j$(nproc)

# Run simulation
./run_sim.sh configs/simulation/macros/simulation.mac

# Run all tests
./test.sh
# Or: cd build && ctest --output-on-failure

# Run specific test label
cd build && ctest -L unit

# Run single test by name
cd build && ctest -R test_name

# Clean rebuild (deletes build/, bin/, lib/ - use sparingly)
./build.sh
```

## CMake Options

- `BUILD_TESTS` - Build unit and integration tests (default: ON)
- `BUILD_APPS` - Build all applications (default: ON)
- `BUILD_ANALYSIS` - Build analysis library (default: ON)
- `WITH_ANAROOT` - Build with ANAROOT support (default: ON, set OFF if TARTSYS undefined)
- `WITH_GEANT4_UIVIS` - Build with Geant4 UI and Vis drivers (default: ON)

## Architecture

**Libraries** (`libs/`):
- `smlogger/` - Async logging system (spdlog-based). Use `SM_INFO()`, `SM_DEBUG()`, `SM_WARN()`, `SM_ERROR()` macros instead of std::cout/G4cout.
- `smg4lib/` - Geant4 core wrapper (actions, construction, devices, physics)
- `sim_deuteron_core/` - Deuteron physics simulation core
- `analysis/` - ROOT-based analysis (event loading, geometry parsing, track reconstruction)
- `analysis_pdc_reco/` - PDC-to-target momentum reconstruction (primary runtime framework for new features)
- `geo_acceptance/` - Geometric acceptance analysis
- `qmd_geo_filter/` - QMD geometry filtering

**Executables** (`apps/`):
- `sim_deuteron/` - Main Geant4 simulation (`bin/sim_deuteron <macro.mac>`)
- `run_reconstruction/` - Target momentum reconstruction from PDC data
- `tools/` - Various analysis utilities

**Reconstruction Architecture**:
- `libs/analysis_pdc_reco/` is the primary framework - put new reconstruction features here
- `bin/reconstruct_target_momentum` is the canonical reconstruction CLI
- `libs/analysis/include/TargetReconstructor.hh` is legacy compatibility code - avoid adding new features
- `scripts/reconstruction/nn_target_momentum/` is the NN backend for the main framework

## Coding Conventions

- C++20 standard, clangd for IDE support (compile_commands.json generated)
- 4-space indentation, braces on same line
- Files: `.cc` (source), `.hh` (headers)
- Types: PascalCase; member fields: `f`/`fg` prefix (e.g., `fMagField`)
- Tests: `test_*.cc` naming, GoogleTest framework

## Testing

- Framework: GoogleTest + CTest
- Labels: `unit`, `performance`, `visualization`, `realdata`
- Visualization tests: set `SM_TEST_VISUALIZATION=ON`, outputs to `build/test_output/`

```bash
# Run visualization test
SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor

# Run specific test filter
./bin/test_TargetReconstructor --gtest_filter="*TMinuitOptimizationWithPath*"
```

## Environment Variables

- `SM_LOG_LEVEL` - Control log verbosity (TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL)
- `SM_BATCH_MODE` - Enable batch mode logging
- `SM_TEST_VISUALIZATION` - Enable visualization in tests
- `TARTSYS` - Path to ANAROOT installation (optional)

## Key Directories

- `configs/simulation/macros/` - Geant4 macro files (.mac)
- `configs/simulation/geometry/` - GDML geometry definitions
- `data/input/` - Input data; `data/simulation/` - Simulation outputs
- `scripts/analysis/` - Python analysis scripts
- `scripts/batch/` - Batch processing (e.g., `batch_run_ypol.py`)
- `scripts/reconstruction/nn_target_momentum/` - NN model training/inference

## Commit Style

Short, imperative, often lowercase messages: "add ...", "fix ...", "delete ..."

## Domain Skills Reference

The `skills/` directory contains domain-specific documentation for complex subsystems. Read the relevant SKILL.md before making changes to these areas:

| Skill | When to Use |
|-------|-------------|
| `skills/smsim-pdc-target-reco-overview/` | **Start here** for any PDC reconstruction task - routes to correct sub-skill |
| `skills/smsim-pdc-track-reco/` | FragSimData → PDC1/PDC2 track points |
| `skills/smsim-pdc-momentum-reco/` | Primary runtime framework in `analysis_pdc_reco` (RK modes, solver contracts) |
| `skills/smsim-nn-target-momentum/` | NN backend lifecycle (dataset, training, export, C++ inference) |
| `skills/smsim-geant4-io/` | Geant4 ROOT I/O and QMD raw data columns |
| `skills/smsim-rk-geometry-filter/` | Runge-Kutta geometry filtering |
| `skills/smsim-ips-veto-position-scan/` | IPS veto position scanning |

Each skill contains:
- `SKILL.md` - Overview and workflow
- `references/path-map.md` - Source-of-truth file paths
- `references/workflow.md` - Authoritative data flow
- `scripts/check_sync.sh` - Verify paths are still valid

## Bilingual Comments

For inline comments explaining physics/math logic, use bilingual format:
```cpp
// [EN] Reserve memory to prevent reallocation during Monte Carlo loop / [CN] 预分配内存以防止蒙特卡洛循环中的重新分配
```
