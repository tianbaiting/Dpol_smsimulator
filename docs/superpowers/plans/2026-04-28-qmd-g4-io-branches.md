# QMD raw → g4 input → g4 output b_phi metadata Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace z_pol "alignment" rotation with per-event uniform random rotation, propagate truth `b_phi`, `bimp`, `phi_np_truth` from QMD raw → g4 input ROOT → g4 output ROOT for reaction-plane reconstruction validation.

**Architecture:** Offline rotation at `GenInputRoot_qmdrawdata` time (deterministic with `--rotation-seed`). Per-particle `TBeamSimData::fUserDouble[]` carries the per-event metadata through the existing tree input path. A new `EventTruthConverter` reads from `gBeamSimDataArray[0]` in `EndOfEventAction` and writes per-event scalar branches `truth_b_phi/D`, `truth_bimp/D`, `truth_phi_np/D` to the output tree.

**Tech Stack:** C++20, ROOT 6.x, Geant4 10.x, GoogleTest, spdlog, CMake (`file(GLOB_RECURSE)` so new `.cc` files auto-pick).

**Spec:** `docs/superpowers/specs/2026-04-28-qmd-g4-io-branches-design.md`

---

## File Structure

**Create:**
- `scripts/event_generator/dpol_eventgen/qmd_rotation.hh` — pure inline helpers `phi_np_from_components`, `apply_xy_rotation`, `wrap_two_pi` (testable without ROOT IO)
- `libs/smg4lib/src/data/include/EventTruthConverter.hh` — new SimDataConverter
- `libs/smg4lib/src/data/src/EventTruthConverter.cc`
- `tests/unit/test_qmd_rotation.cc` — unit test for rotation helpers
- `tests/unit/test_qmd_input_metadata_indices.cc` — guard test for index constants
- `tests/integration/test_geninput_qmdrawdata_roundtrip.sh` — end-to-end smoke (gen → read ROOT → assert)
- `tests/integration/test_geninput_qmdrawdata_roundtrip.C` — ROOT macro called by the .sh

**Modify:**
- `libs/smg4lib/include/QMDInputMetadata.hh` — add `kBPhiIndex=1`, `kPhiNpTruthIndex=2`
- `scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc` — drop alignment, add random rotation, 9-col sniff, CLI rename, RNG seed, stamp metadata
- `apps/sim_deuteron/main.cc:105-106` — register `EventTruthConverter`
- `libs/smg4lib/src/action/src/PrimaryGeneratorActionBasic.cc::BeamTypeTree` — (optional) log warn if `fUserDouble.size() < 3`
- `scripts/analysis/run_polarization_validation_sn124_zpol_g060.sh:44` — rename CLI flag
- `scripts/analysis/run_target_momentum_reco_pipeline.sh:127,138` — rename CLI flags
- `tests/unit/CMakeLists.txt` — register the two new gtest targets
- `tests/integration/CMakeLists.txt` — register the new shell test

---

## Setup

You are in worktree `/home/tian/workspace/dpol/smsimulator5.5/.worktrees/qmd-g4-io-branches` on branch `feature/qmd-g4-io-branches`. Build commands assume this worktree.

Activate the env once at the start of work:
```bash
micromamba activate anaroot-env
```

Initial baseline build (one-time):
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_APPS=ON -DBUILD_ANALYSIS=ON -DWITH_ANAROOT=ON
make -j$(nproc)
cd ..
```

After every code-changing task, rebuild only the affected targets (e.g. `make -j$(nproc) GenInputRoot_qmdrawdata sim_deuteron smdata`) before running tests.

---

## Task 1: Extend `QMDInputMetadata` with new indices

**Files:**
- Modify: `libs/smg4lib/include/QMDInputMetadata.hh`
- Test: `tests/unit/test_qmd_input_metadata_indices.cc` (new)
- Test reg: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/unit/test_qmd_input_metadata_indices.cc`:
```cpp
#include <gtest/gtest.h>
#include "QMDInputMetadata.hh"

TEST(QMDInputMetadataIndices, FUserDoubleLayoutIsStable) {
    using namespace qmd_input_metadata;
    EXPECT_EQ(kBimpIndex,        0);
    EXPECT_EQ(kBPhiIndex,        1);
    EXPECT_EQ(kPhiNpTruthIndex,  2);
}

TEST(QMDInputMetadataIndices, FUserIntLayoutIsStable) {
    using namespace qmd_input_metadata;
    EXPECT_EQ(kSourceKindIndex,        0);
    EXPECT_EQ(kOriginalEventIdIndex,   1);
    EXPECT_EQ(kSourceFileIndex,        2);
    EXPECT_EQ(kPolarizationKindIndex,  3);
}
```

- [ ] **Step 2: Register the test in CMake**

Append to `tests/unit/CMakeLists.txt`:
```cmake
add_executable(test_qmd_input_metadata_indices
    test_qmd_input_metadata_indices.cc
)
target_link_libraries(test_qmd_input_metadata_indices PRIVATE
    smg4lib
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(test_qmd_input_metadata_indices PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/smg4lib/include
)
gtest_discover_tests(test_qmd_input_metadata_indices
    PROPERTIES LABELS "unit"
)
```

- [ ] **Step 3: Run test to verify it fails**

```bash
cd build && cmake .. && make -j$(nproc) test_qmd_input_metadata_indices 2>&1 | tail -20
```
Expected: compile error `kBPhiIndex was not declared in this scope`.

- [ ] **Step 4: Add new indices**

Edit `libs/smg4lib/include/QMDInputMetadata.hh`. Replace the existing `constexpr int kBimpIndex = 0;` line with:
```cpp
constexpr int kBimpIndex        = 0;
constexpr int kBPhiIndex        = 1;
constexpr int kPhiNpTruthIndex  = 2;
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cd build && make -j$(nproc) test_qmd_input_metadata_indices && ctest -R test_qmd_input_metadata_indices --output-on-failure
```
Expected: 2 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/smg4lib/include/QMDInputMetadata.hh \
        tests/unit/test_qmd_input_metadata_indices.cc \
        tests/unit/CMakeLists.txt
git commit -m "metadata: add kBPhiIndex and kPhiNpTruthIndex to fUserDouble"
```

---

## Task 2: Extract rotation algebra into testable helpers

**Files:**
- Create: `scripts/event_generator/dpol_eventgen/qmd_rotation.hh`
- Test: `tests/unit/test_qmd_rotation.cc`
- Test reg: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/unit/test_qmd_rotation.cc`:
```cpp
#include <gtest/gtest.h>
#include "qmd_rotation.hh"
#include <cmath>

constexpr double kTwoPi = 2.0 * M_PI;
constexpr double kPi    = M_PI;

TEST(QMDRotation, PhiNpFromComponentsIdentity) {
    EXPECT_NEAR(qmd::phi_np(1.0, 0.0), 0.0, 1e-12);
    EXPECT_NEAR(qmd::phi_np(0.0, 1.0), kPi / 2.0, 1e-12);
    EXPECT_NEAR(qmd::phi_np(-1.0, 0.0), kPi, 1e-12);
}

TEST(QMDRotation, ApplyXYRotationZeroIsIdentity) {
    double x = 100.0, y = -50.0;
    qmd::rotate_xy(x, y, 0.0);
    EXPECT_NEAR(x, 100.0, 1e-9);
    EXPECT_NEAR(y, -50.0, 1e-9);
}

TEST(QMDRotation, ApplyXYRotationHalfPi) {
    // [EN] (1,0) rotated by +π/2 => (0,1).
    double x = 1.0, y = 0.0;
    qmd::rotate_xy(x, y, kPi / 2.0);
    EXPECT_NEAR(x, 0.0, 1e-9);
    EXPECT_NEAR(y, 1.0, 1e-9);
}

TEST(QMDRotation, WrapTwoPiInRange) {
    EXPECT_NEAR(qmd::wrap_two_pi(0.0), 0.0, 1e-12);
    EXPECT_NEAR(qmd::wrap_two_pi(kTwoPi), 0.0, 1e-12);
    EXPECT_NEAR(qmd::wrap_two_pi(-0.1), kTwoPi - 0.1, 1e-9);
    EXPECT_NEAR(qmd::wrap_two_pi(3.0 * kPi), kPi, 1e-9);
}
```

- [ ] **Step 2: Register the test in CMake**

Append to `tests/unit/CMakeLists.txt`:
```cmake
add_executable(test_qmd_rotation
    test_qmd_rotation.cc
)
target_include_directories(test_qmd_rotation PRIVATE
    ${CMAKE_SOURCE_DIR}/scripts/event_generator/dpol_eventgen
)
target_link_libraries(test_qmd_rotation PRIVATE
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(test_qmd_rotation
    PROPERTIES LABELS "unit"
)
```

- [ ] **Step 3: Run test to verify it fails**

```bash
cd build && cmake .. && make -j$(nproc) test_qmd_rotation 2>&1 | tail -10
```
Expected: `fatal error: qmd_rotation.hh: No such file or directory`.

- [ ] **Step 4: Implement the helpers**

Create `scripts/event_generator/dpol_eventgen/qmd_rotation.hh`:
```cpp
#ifndef QMD_ROTATION_HH
#define QMD_ROTATION_HH

#include <cmath>

namespace qmd {

inline double phi_np(double sum_px, double sum_py) {
    return std::atan2(sum_py, sum_px);
}

inline void rotate_xy(double& x, double& y, double angle) {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const double xr = c * x - s * y;
    const double yr = s * x + c * y;
    x = xr;
    y = yr;
}

inline double wrap_two_pi(double angle) {
    constexpr double two_pi = 2.0 * M_PI;
    const double r = std::fmod(angle, two_pi);
    return (r < 0.0) ? r + two_pi : r;
}

}  // namespace qmd

#endif
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cd build && make -j$(nproc) test_qmd_rotation && ctest -R test_qmd_rotation --output-on-failure
```
Expected: 4 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add scripts/event_generator/dpol_eventgen/qmd_rotation.hh \
        tests/unit/test_qmd_rotation.cc \
        tests/unit/CMakeLists.txt
git commit -m "geninput: extract rotation algebra into qmd_rotation.hh + tests"
```

---

## Task 3: Replace alignment with random rotation in `ConvertElasticFile`

**Files:**
- Modify: `scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc`

This task only swaps the rotation block at lines 528-549 (alignment) for random rotation using `qmd::*` helpers. CLI rename and seed wiring come in Task 5. For now, use a hardcoded `TRandom3(0)` instance scoped to `ConvertElasticFile`; Task 5 will replace it with one passed via Options.

- [ ] **Step 1: Add include for qmd_rotation.hh**

Edit `scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc`. After the existing `#include "TBeamSimData.hh"` (~line 11), add:
```cpp
#include "qmd_rotation.hh"
#include "TRandom3.h"
```

- [ ] **Step 2: Replace the alignment block**

In `ConvertElasticFile` find the existing block (currently around lines 528-549):
```cpp
        const bool do_rotate = (pol == qmd_input_metadata::PolarizationKind::kY && opts.rotate_ypol)
            || (pol == qmd_input_metadata::PolarizationKind::kZ && opts.rotate_zpol);
        if (do_rotate) {
            const double sum_px = pxp + pxn;
            const double sum_py = pyp + pyn;
            const double phi = std::atan2(sum_py, sum_px);
            const double angle = -phi;
            const double cos_a = std::cos(angle);
            const double sin_a = std::sin(angle);

            const double rot_pxp = cos_a * pxp - sin_a * pyp;
            const double rot_pyp = sin_a * pxp + cos_a * pyp;
            const double rot_pxn = cos_a * pxn - sin_a * pyn;
            const double rot_pyn = sin_a * pxn + cos_a * pyn;

            pxp = rot_pxp;
            pyp = rot_pyp;
            pxn = rot_pxn;
            pyn = rot_pyn;
            ++rotated_event_count;
        }
```

Replace with:
```cpp
        const bool randomize = (pol == qmd_input_metadata::PolarizationKind::kY && opts.rotate_ypol)
            || (pol == qmd_input_metadata::PolarizationKind::kZ && opts.rotate_zpol);
        const double b_phi_raw_rad = 0.0;  // [EN] Task 4 fills this from the optional rpphi column / [CN] Task 4 改为从 rpphi 列读取
        double delta = 0.0;
        if (randomize) {
            // [EN] gRandom seeded in main(); Task 5 wires --rotation-seed.
            delta = 2.0 * M_PI * gRandom->Uniform();
            qmd::rotate_xy(pxp, pyp, delta);
            qmd::rotate_xy(pxn, pyn, delta);
            ++rotated_event_count;
        }
        const double b_phi = qmd::wrap_two_pi(b_phi_raw_rad + delta);
        // [EN] qmd::phi_np takes (sum_px, sum_py) — see qmd_rotation.hh.
        const double phi_np_truth = qmd::phi_np(pxp + pxn, pyp + pyn);
```

- [ ] **Step 3: Pass b_phi & phi_np_truth to WriteElasticEvent**

Find the existing `WriteElasticEvent(no, pxp, pyp, pzp, pxn, pyn, pzn, pol, bimp, source_file_index, tree, position);` call (~line 551).

Modify the signature of `WriteElasticEvent` (declaration ~line 344) to take two new params at the end:
```cpp
void WriteElasticEvent(
    int event_no,
    double pxp, double pyp, double pzp,
    double pxn, double pyn, double pzn,
    qmd_input_metadata::PolarizationKind pol,
    double bimp,
    int source_file_index,
    TTree& tree,
    const TVector3& position,
    double b_phi,
    double phi_np_truth
)
```

Update the call site:
```cpp
        WriteElasticEvent(no, pxp, pyp, pzp, pxn, pyn, pzn, pol, bimp,
                          source_file_index, tree, position,
                          b_phi, phi_np_truth);
```

`StampMetadata` is updated in Task 6 to actually use `b_phi` and `phi_np_truth`. For now `WriteElasticEvent` accepts them but ignores. Add a one-line `(void)b_phi; (void)phi_np_truth;` inside `WriteElasticEvent` to silence -Wunused.

- [ ] **Step 4: Build and verify nothing else breaks**

```bash
cd build && make -j$(nproc) GenInputRoot_qmdrawdata 2>&1 | tail -20
```
Expected: success. (Existing tests not affected yet.)

- [ ] **Step 5: Commit**

```bash
git add scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc
git commit -m "geninput: replace zpol alignment with per-event random phi rotation"
```

---

## Task 4: Add 9-column sniffing to extract rpphi

**Files:**
- Modify: `scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc`
- Test: extend `tests/integration/test_geninput_qmdrawdata_roundtrip.sh` (created in Task 12)

This task only changes parsing — the `b_phi_raw_rad = 0.0` placeholder from Task 3 becomes filled from the file when present.

- [ ] **Step 1: Modify the elastic parsing loop**

In `ConvertElasticFile` find the read in the `while (std::getline(fin, line))` loop (~line 506):
```cpp
        if (!(iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn)) {
            continue;
        }
```

Replace with:
```cpp
        if (!(iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn)) {
            continue;
        }
        // [EN] phi_random datasets append two extra columns: per-event b (fm) and rpphi (deg). / [CN] phi_random 数据集多两列: 每事件 b (fm) 与 rpphi (°)
        double b_per_event = std::numeric_limits<double>::quiet_NaN();
        double rpphi_deg   = 0.0;
        const bool has_rpphi = static_cast<bool>(iss >> b_per_event >> rpphi_deg);
        const double b_phi_raw_rad = has_rpphi ? rpphi_deg * (M_PI / 180.0) : 0.0;
```

- [ ] **Step 2: Wire `b_phi_raw_rad` into the rotation block**

In the same function, replace the `const double b_phi_raw_rad = 0.0;` line that Task 3 added with a comment noting it's now defined upstream. The variable is already named `b_phi_raw_rad`, so just delete the `= 0.0;` placeholder line. The rest of the rotation block already references `b_phi_raw_rad`.

- [ ] **Step 3: Prefer per-event `b` whenever 9-col present**

In phi_random files the header reports `b= 0.000fm` (a placeholder) while column 8 carries the real per-event impact parameter. Override `bimp` to the per-event value whenever the row has 9 columns.

Promote `bimp` from `const` to mutable. Change line ~474 from:
```cpp
    const double bimp = ExtractB(header1).value_or(kUnknownBimp);
```
to:
```cpp
    const double bimp_header = ExtractB(header1).value_or(kUnknownBimp);
```

Inside the per-row loop, just below the `b_phi_raw_rad` line, declare the per-row bimp:
```cpp
        const double bimp = (has_rpphi && std::isfinite(b_per_event))
            ? b_per_event
            : bimp_header;
```

This gives 9-col rows their true per-event b, and 7-col rows the header b (current behavior).

- [ ] **Step 4: Build**

```bash
cd build && make -j$(nproc) GenInputRoot_qmdrawdata
```
Expected: success.

- [ ] **Step 5: Manual sniff test (will be automated in Task 12)**

Create a tiny test input:
```bash
cat > /tmp/dbreak_test_9col.dat <<'EOF'
ynp d+124Sn   190.00MeV/u b= 0.000fm   2events
  No.  pxp(MeV/c)  pyp(MeV/c)  pzp(MeV/c)  pxn(MeV/c)  pyn(MeV/c)  pzn(MeV/c)           b       rpphi
       1    100.0    0.0    600.0    0.0    0.0    600.0   5.0    90.0
       2    0.0    100.0    600.0    0.0    0.0    600.0   5.0    180.0
EOF
mkdir -p /tmp/qmd_test_in/y_pol/phi_random/d+Sn124E190g050ynp
cp /tmp/dbreak_test_9col.dat /tmp/qmd_test_in/y_pol/phi_random/d+Sn124E190g050ynp/dbreak.dat
mkdir -p /tmp/qmd_test_out
./build/bin/GenInputRoot_qmdrawdata --mode ypol --source elastic \
    --input-base /tmp/qmd_test_in \
    --output-base /tmp/qmd_test_out \
    --rotate-ypol off
ls /tmp/qmd_test_out/y_pol/phi_random/d+Sn124E190g050ynp/dbreak.root
```
Expected: ROOT file produced; no parser crash on the 9-column input.

- [ ] **Step 6: Commit**

```bash
git add scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc
git commit -m "geninput: parse optional rpphi/b 9-column format from phi_random files"
```

---

## Task 5: CLI rename + `--rotation-seed`

**Files:**
- Modify: `scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc`
- Modify: `scripts/analysis/run_polarization_validation_sn124_zpol_g060.sh`
- Modify: `scripts/analysis/run_target_momentum_reco_pipeline.sh`

- [ ] **Step 1: Rename Options fields and add seed**

In `GenInputRoot_qmdrawdata.cc` find the `struct Options` (~line 52):
```cpp
struct Options {
    fs::path input_base;
    fs::path output_base;
    Mode mode = Mode::kBoth;
    SourceMode source = SourceMode::kElastic;
    std::string target_filter;
    bool cut_unphysical = true;
    double cut_ypol_axis_limit = 150.0;
    double cut_zpol_axis_limit = 150.0;
    bool rotate_ypol = false;
    bool rotate_zpol = true;
};
```

Replace with:
```cpp
struct Options {
    fs::path input_base;
    fs::path output_base;
    Mode mode = Mode::kBoth;
    SourceMode source = SourceMode::kElastic;
    std::string target_filter;
    bool cut_unphysical = true;
    double cut_ypol_axis_limit = 150.0;
    double cut_zpol_axis_limit = 150.0;
    bool randomize_ypol = false;
    bool randomize_zpol = true;
    long rotation_seed = 0;  // 0 → use system-time-derived seed
};
```

Replace all references to `opts.rotate_ypol` / `opts.rotate_zpol` with `opts.randomize_ypol` / `opts.randomize_zpol`. Use grep to find them:
```bash
grep -n "rotate_ypol\|rotate_zpol" scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc
```

- [ ] **Step 2: Update the CLI parser**

Find `ParseArgs` (~line 669). Replace:
```cpp
        } else if (arg == "--rotate-ypol" && i + 1 < argc) {
            opts.rotate_ypol = ParseBoolOption(argv[++i]);
        } else if (arg == "--rotate-zpol" && i + 1 < argc) {
            opts.rotate_zpol = ParseBoolOption(argv[++i]);
        }
```

With:
```cpp
        } else if (arg == "--randomize-ypol" && i + 1 < argc) {
            opts.randomize_ypol = ParseBoolOption(argv[++i]);
        } else if (arg == "--randomize-zpol" && i + 1 < argc) {
            opts.randomize_zpol = ParseBoolOption(argv[++i]);
        } else if (arg == "--rotation-seed" && i + 1 < argc) {
            opts.rotation_seed = std::stol(argv[++i]);
        }
```

Update the `--help` text (around line 702-710):
```cpp
            spdlog::info(
                "Usage: GenInputRoot_qmdrawdata --mode [ypol|zpol|both] "
                "[--source elastic|allevent|both] "
                "--input-base PATH --output-base PATH "
                "[--target-filter Sn124] "
                "[--cut-unphysical on|off] [--cut-ypol-axis-limit 150] [--cut-zpol-axis-limit 150] "
                "[--randomize-ypol on|off] [--randomize-zpol on|off] "
                "[--rotation-seed N]"
            );
```

- [ ] **Step 3: Initialize gRandom in main**

Find `int main(int argc, char* argv[])` (~line 910). After `const auto opts = ParseArgs(argc, argv);` and before the calls to `ProcessElastic`/`ProcessAllevent`, add:
```cpp
        if (opts.rotation_seed != 0) {
            gRandom->SetSeed(static_cast<UInt_t>(opts.rotation_seed));
            spdlog::info("Using --rotation-seed {}", opts.rotation_seed);
        } else {
            gRandom->SetSeed(0);  // 0 = TRandom3 uses UUID-based seed
            spdlog::info("Using system-derived seed (--rotation-seed 0)");
        }
```

Add `#include <TRandom.h>` near the top includes if not already present (TRandom3 includes TRandom).

- [ ] **Step 4: Update `ProcessZpolElasticFiles` warning**

Find around line 809:
```cpp
    if (opts.rotate_zpol) {
        spdlog::warn(
            "rotate-zpol is ON: generated files under {} will contain rotated z_pol momenta.",
            output_base.string().c_str()
        );
    }
```

Replace with:
```cpp
    if (opts.randomize_zpol) {
        spdlog::info(
            "randomize-zpol is ON: each z_pol event will be rotated by a uniform random phi in [0, 2π). Output: {}",
            output_base.string().c_str()
        );
    }
```

- [ ] **Step 5: Update consumer scripts**

Edit `scripts/analysis/run_polarization_validation_sn124_zpol_g060.sh` line 44:
```bash
    --rotate-zpol on \
```
to:
```bash
    --randomize-zpol on \
```

Edit `scripts/analysis/run_target_momentum_reco_pipeline.sh` lines 127 & 138:
```bash
        --rotate-zpol on
```
to:
```bash
        --randomize-zpol on
```
And:
```bash
        --rotate-ypol off
```
to:
```bash
        --randomize-ypol off
```

- [ ] **Step 6: Build and smoke-test CLI**

```bash
cd build && make -j$(nproc) GenInputRoot_qmdrawdata
./bin/GenInputRoot_qmdrawdata --help 2>&1 | grep randomize
```
Expected: help text contains `--randomize-ypol` and `--randomize-zpol` and `--rotation-seed`.

```bash
./bin/GenInputRoot_qmdrawdata --mode ypol --source elastic \
    --input-base /tmp/qmd_test_in --output-base /tmp/qmd_test_out2 \
    --randomize-ypol off --rotation-seed 42
```
Expected: success.

- [ ] **Step 7: Commit**

```bash
git add scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc \
        scripts/analysis/run_polarization_validation_sn124_zpol_g060.sh \
        scripts/analysis/run_target_momentum_reco_pipeline.sh
git commit -m "geninput: rename --rotate-* to --randomize-*; add --rotation-seed"
```

---

## Task 6: Stamp `b_phi` and `phi_np_truth` into `fUserDouble`

**Files:**
- Modify: `scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc`

- [ ] **Step 1: Extend `StampMetadata` signature**

Find `StampMetadata` (~line 311):
```cpp
void StampMetadata(
    TBeamSimData& beam,
    qmd_input_metadata::SourceKind source,
    int original_event_id,
    int source_file_index,
    qmd_input_metadata::PolarizationKind pol,
    double bimp
) {
    beam.fUserInt.assign(qmd_input_metadata::kPolarizationKindIndex + 1, 0);
    beam.fUserDouble.assign(qmd_input_metadata::kBimpIndex + 1, kUnknownBimp);
    beam.fUserInt[qmd_input_metadata::kSourceKindIndex] = static_cast<int>(source);
    beam.fUserInt[qmd_input_metadata::kOriginalEventIdIndex] = original_event_id;
    beam.fUserInt[qmd_input_metadata::kSourceFileIndex] = source_file_index;
    beam.fUserInt[qmd_input_metadata::kPolarizationKindIndex] = static_cast<int>(pol);
    beam.fUserDouble[qmd_input_metadata::kBimpIndex] = bimp;
}
```

Replace with:
```cpp
void StampMetadata(
    TBeamSimData& beam,
    qmd_input_metadata::SourceKind source,
    int original_event_id,
    int source_file_index,
    qmd_input_metadata::PolarizationKind pol,
    double bimp,
    double b_phi,
    double phi_np_truth
) {
    beam.fUserInt.assign(qmd_input_metadata::kPolarizationKindIndex + 1, 0);
    beam.fUserDouble.assign(qmd_input_metadata::kPhiNpTruthIndex + 1,
                            std::numeric_limits<double>::quiet_NaN());
    beam.fUserInt[qmd_input_metadata::kSourceKindIndex] = static_cast<int>(source);
    beam.fUserInt[qmd_input_metadata::kOriginalEventIdIndex] = original_event_id;
    beam.fUserInt[qmd_input_metadata::kSourceFileIndex] = source_file_index;
    beam.fUserInt[qmd_input_metadata::kPolarizationKindIndex] = static_cast<int>(pol);
    beam.fUserDouble[qmd_input_metadata::kBimpIndex]       = bimp;
    beam.fUserDouble[qmd_input_metadata::kBPhiIndex]       = b_phi;
    beam.fUserDouble[qmd_input_metadata::kPhiNpTruthIndex] = phi_np_truth;
}
```

- [ ] **Step 2: Pass new args from `WriteElasticEvent`**

Find both `StampMetadata(...)` call sites in `WriteElasticEvent` (proton block ~line 366, neutron block ~line 382). Replace each call's arg list:
```cpp
    StampMetadata(
        proton,
        qmd_input_metadata::SourceKind::kElastic,
        event_no,
        source_file_index,
        pol,
        bimp,
        b_phi,
        phi_np_truth
    );
```
(Same for neutron.)

Remove the `(void)b_phi; (void)phi_np_truth;` silencer added in Task 3.

- [ ] **Step 3: Pass new args from `FlushAlleventGroup`**

Find `FlushAlleventGroup` (~line 395). The all-event path doesn't have well-defined `b_phi` (see spec §8). Update its `StampMetadata` call to pass NaN for both:
```cpp
      StampMetadata(
          primaries[i],
          qmd_input_metadata::SourceKind::kAllevent,
          current_isim,
          source_file_index,
          pol,
          current_bimp,
          std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::quiet_NaN()
      );
```

- [ ] **Step 4: Build**

```bash
cd build && make -j$(nproc) GenInputRoot_qmdrawdata
```
Expected: success.

- [ ] **Step 5: Smoke-test that fUserDouble has 3 entries**

Run on the tiny test input from Task 4 step 5, then:
```bash
root -b -l -q -e '
    TFile* f = TFile::Open("/tmp/qmd_test_out2/y_pol/phi_random/d+Sn124E190g050ynp/dbreak.root");
    TTree* t = (TTree*)f->Get("tree");
    TBeamSimDataArray* arr = nullptr;
    t->SetBranchAddress("TBeamSimData", &arr);
    t->GetEntry(0);
    std::cout << "fUserDouble.size()=" << arr->at(0).fUserDouble.size()
              << " bimp=" << arr->at(0).fUserDouble[0]
              << " b_phi=" << arr->at(0).fUserDouble[1]
              << " phi_np=" << arr->at(0).fUserDouble[2] << std::endl;
'
```
Expected: `fUserDouble.size()=3 bimp=5 b_phi=1.5708 phi_np=0` (event 1 had px=100, py=0 → phi_np=0; b_phi=π/2 since rpphi was 90°).

- [ ] **Step 6: Commit**

```bash
git add scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc
git commit -m "geninput: stamp b_phi and phi_np_truth into fUserDouble[1,2]"
```

---

## Task 7: Create `EventTruthConverter`

**Files:**
- Create: `libs/smg4lib/src/data/include/EventTruthConverter.hh`
- Create: `libs/smg4lib/src/data/src/EventTruthConverter.cc`

The CMake `file(GLOB_RECURSE)` auto-picks the new files; no CMakeLists change required.

- [ ] **Step 1: Create header**

Write `libs/smg4lib/src/data/include/EventTruthConverter.hh`:
```cpp
#ifndef _EVENTTRUTHCONVERTER_HH_
#define _EVENTTRUTHCONVERTER_HH_

#include "TObject.h"
#include "SimDataConverter.hh"

class TTree;

// [EN] Reads per-event truth metadata (b_phi, bimp, phi_np_truth) stamped by
//      GenInputRoot_qmdrawdata into TBeamSimData::fUserDouble[0..2] and writes
//      them as scalar branches truth_bimp/truth_b_phi/truth_phi_np in the
//      output ROOT tree. Reaction-plane reconstruction validation downstream
//      compares reco phi_RP against truth_b_phi.
// [CN] 把 GenInputRoot_qmdrawdata 在 fUserDouble[0..2] 写入的 per-event truth
//      元数据透传到输出 ROOT tree 的标量 branch,供下游反应平面 reco 验证。
class EventTruthConverter : public SimDataConverter
{
public:
  EventTruthConverter(TString name = "EventTruthConverter");
  ~EventTruthConverter() override;

  int Initialize() override;
  int DefineBranch(TTree* tree) override;
  int ConvertSimData() override;
  int ClearBuffer() override;

private:
  Double_t fBimp;
  Double_t fBPhi;
  Double_t fPhiNpTruth;
};

#endif
```

- [ ] **Step 2: Create implementation**

Write `libs/smg4lib/src/data/src/EventTruthConverter.cc`:
```cpp
#include "EventTruthConverter.hh"
#include "TBeamSimData.hh"
#include "QMDInputMetadata.hh"

#include "TTree.h"

#include <cmath>
#include <iostream>
#include <limits>

EventTruthConverter::EventTruthConverter(TString name)
  : SimDataConverter(name)
{
  ClearBuffer();
}

EventTruthConverter::~EventTruthConverter() = default;

int EventTruthConverter::Initialize()
{
  return 0;
}

int EventTruthConverter::DefineBranch(TTree* tree)
{
  if (fDataStore && tree) {
    tree->Branch("truth_bimp",   &fBimp,       "truth_bimp/D");
    tree->Branch("truth_b_phi",  &fBPhi,       "truth_b_phi/D");
    tree->Branch("truth_phi_np", &fPhiNpTruth, "truth_phi_np/D");
  }
  return 0;
}

int EventTruthConverter::ConvertSimData()
{
  // [EN] gBeamSimDataArray is filled by PrimaryGeneratorActionBasic::BeamTypeTree
  //      at the start of each event; per-event metadata is replicated on every
  //      particle's fUserDouble. Read from the first particle.
  // [CN] gBeamSimDataArray 在 BeamTypeTree 中填充;每个粒子 fUserDouble 携带相同
  //      per-event 元数据,从第 0 号粒子读取即可。
  if (!gBeamSimDataArray || gBeamSimDataArray->empty()) {
    ClearBuffer();
    return 0;
  }

  const auto& first = gBeamSimDataArray->at(0);
  if (first.fUserDouble.size() <= qmd_input_metadata::kPhiNpTruthIndex) {
    static bool warned = false;
    if (!warned) {
      std::cerr << "[EventTruthConverter] WARN: input fUserDouble too short ("
                << first.fUserDouble.size()
                << "); writing NaN truth metadata. Regenerate g4 input with newer GenInputRoot_qmdrawdata."
                << std::endl;
      warned = true;
    }
    ClearBuffer();
    return 0;
  }

  fBimp       = first.fUserDouble[qmd_input_metadata::kBimpIndex];
  fBPhi       = first.fUserDouble[qmd_input_metadata::kBPhiIndex];
  fPhiNpTruth = first.fUserDouble[qmd_input_metadata::kPhiNpTruthIndex];
  return 0;
}

int EventTruthConverter::ClearBuffer()
{
  fBimp       = std::numeric_limits<double>::quiet_NaN();
  fBPhi       = std::numeric_limits<double>::quiet_NaN();
  fPhiNpTruth = std::numeric_limits<double>::quiet_NaN();
  return 0;
}
```

- [ ] **Step 3: Build and confirm CMake glob picks it up**

```bash
cd build && cmake .. && make -j$(nproc) smdata 2>&1 | tail -10
```
Expected: `EventTruthConverter.cc.o` mentioned in the build output.

- [ ] **Step 4: Commit**

```bash
git add libs/smg4lib/src/data/include/EventTruthConverter.hh \
        libs/smg4lib/src/data/src/EventTruthConverter.cc
git commit -m "smdata: add EventTruthConverter for truth metadata passthrough"
```

---

## Task 8: Register `EventTruthConverter` in `sim_deuteron`

**Files:**
- Modify: `apps/sim_deuteron/main.cc`

- [ ] **Step 1: Add include**

Edit `apps/sim_deuteron/main.cc`. After the existing `#include "FragSimDataConverter_Basic.hh"` (line 45), add:
```cpp
#include "EventTruthConverter.hh"
```

- [ ] **Step 2: Register the converter**

Find the converter registration block (~line 105):
```cpp
  simDataManager->RegistConverter(new FragSimDataConverter_Basic);
  simDataManager->RegistConverter(new NEBULASimDataConverter_TArtNEBULAPla);
```

Change to:
```cpp
  simDataManager->RegistConverter(new FragSimDataConverter_Basic);
  simDataManager->RegistConverter(new NEBULASimDataConverter_TArtNEBULAPla);
  simDataManager->RegistConverter(new EventTruthConverter);
```

- [ ] **Step 3: Build sim_deuteron**

```bash
cd build && make -j$(nproc) sim_deuteron 2>&1 | tail -20
```
Expected: success.

- [ ] **Step 4: Commit**

```bash
git add apps/sim_deuteron/main.cc
git commit -m "sim_deuteron: register EventTruthConverter for truth metadata output"
```

---

## Task 9: Add a smoke integration test (input-side roundtrip)

**Files:**
- Create: `tests/integration/test_geninput_qmdrawdata_roundtrip.sh`
- Create: `tests/integration/test_geninput_qmdrawdata_roundtrip.C`
- Modify: `tests/integration/CMakeLists.txt`

This test only exercises GenInputRoot_qmdrawdata + ROOT readback. The g4 end-to-end test is left as future work (it requires geometry + macros, much heavier).

- [ ] **Step 1: Write the failing shell harness**

Create `tests/integration/test_geninput_qmdrawdata_roundtrip.sh`:
```bash
#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
GEN_BIN="${REPO_DIR}/build/bin/GenInputRoot_qmdrawdata"

if [[ ! -x "$GEN_BIN" ]]; then
    echo "[SKIP] $GEN_BIN not built"
    exit 0
fi

WORK_DIR="$(mktemp -d)"
trap "rm -rf $WORK_DIR" EXIT

# [EN] Build a synthetic 9-column input with known b_phi values.
mkdir -p "$WORK_DIR/in/y_pol/phi_random/d+Sn124E190g050ynp"
cat > "$WORK_DIR/in/y_pol/phi_random/d+Sn124E190g050ynp/dbreak.dat" <<EOF
ynp d+124Sn   190.00MeV/u b= 0.000fm   3events
  No.  pxp(MeV/c)  pyp(MeV/c)  pzp(MeV/c)  pxn(MeV/c)  pyn(MeV/c)  pzn(MeV/c)           b       rpphi
       1    100.0    0.0    600.0    -100.0    0.0    600.0   5.0      0.0
       2    100.0    0.0    600.0    -100.0    0.0    600.0   5.0     90.0
       3    100.0    0.0    600.0    -100.0    0.0    600.0   5.0    180.0
EOF

# [EN] Build a synthetic 7-column z_pol input.
mkdir -p "$WORK_DIR/in/z_pol/b_discrete/d+Sn124E190g050znp"
cat > "$WORK_DIR/in/z_pol/b_discrete/d+Sn124E190g050znp/dbreakb05.dat" <<EOF
znp d+124Sn   190.00MeV/u b= 5.000fm  3events
  No.  pxp(MeV/c)  pyp(MeV/c)  pzp(MeV/c)  pxn(MeV/c)  pyn(MeV/c)  pzn(MeV/c)
       1    100.0    0.0    600.0    -100.0    0.0    600.0
       2    100.0    0.0    600.0    -100.0    0.0    600.0
       3    100.0    0.0    600.0    -100.0    0.0    600.0
EOF

mkdir -p "$WORK_DIR/out"

# [EN] y_pol: rotation OFF. We expect b_phi = rpphi (mod 2π).
"$GEN_BIN" --mode ypol --source elastic \
    --input-base "$WORK_DIR/in" --output-base "$WORK_DIR/out" \
    --randomize-ypol off

# [EN] z_pol: rotation ON, fixed seed. We expect b_phi to be uniform; just check size.
"$GEN_BIN" --mode zpol --source elastic \
    --input-base "$WORK_DIR/in" --output-base "$WORK_DIR/out" \
    --randomize-zpol on --rotation-seed 42

# [EN] Hand the work dir to the ROOT macro for assertions.
root -b -l -q -e ".L $REPO_DIR/tests/integration/test_geninput_qmdrawdata_roundtrip.C+" \
              -e "test_geninput_qmdrawdata_roundtrip(\"$WORK_DIR\")"
```

Make it executable:
```bash
chmod +x tests/integration/test_geninput_qmdrawdata_roundtrip.sh
```

- [ ] **Step 2: Write the ROOT verification macro**

Create `tests/integration/test_geninput_qmdrawdata_roundtrip.C`:
```cpp
#include <TFile.h>
#include <TTree.h>
#include "TBeamSimData.hh"
#include "QMDInputMetadata.hh"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
constexpr double kPi = 3.14159265358979323846;

void check(bool cond, const std::string& msg) {
    if (!cond) {
        std::cerr << "[FAIL] " << msg << std::endl;
        std::exit(1);
    }
    std::cout << "[OK] " << msg << std::endl;
}
}

void test_geninput_qmdrawdata_roundtrip(const char* work_dir) {
    using namespace qmd_input_metadata;

    // [EN] y_pol: 3 events with rpphi 0/90/180°.
    {
        const std::string path = std::string(work_dir)
            + "/out/y_pol/phi_random/d+Sn124E190g050ynp/dbreak.root";
        TFile* f = TFile::Open(path.c_str(), "READ");
        check(f && !f->IsZombie(), "y_pol file open: " + path);
        TTree* t = (TTree*)f->Get("tree");
        check(t != nullptr, "y_pol tree present");
        TBeamSimDataArray* arr = nullptr;
        t->SetBranchAddress("TBeamSimData", &arr);
        check(t->GetEntries() == 3, "y_pol has 3 entries");

        const double expected[] = {0.0, kPi / 2.0, kPi};
        for (int i = 0; i < 3; ++i) {
            t->GetEntry(i);
            const double bphi = arr->at(0).fUserDouble[kBPhiIndex];
            check(std::abs(bphi - expected[i]) < 1e-6,
                  "y_pol event " + std::to_string(i) + " b_phi");
            const double bimp = arr->at(0).fUserDouble[kBimpIndex];
            check(std::abs(bimp - 5.0) < 1e-6,
                  "y_pol event " + std::to_string(i) + " bimp");
        }
        f->Close();
    }

    // [EN] z_pol: 3 events with rotation. b_phi must be in [0, 2π).
    {
        const std::string path = std::string(work_dir)
            + "/out/z_pol/b_discrete/d+Sn124E190g050znp/dbreakb05.root";
        TFile* f = TFile::Open(path.c_str(), "READ");
        check(f && !f->IsZombie(), "z_pol file open: " + path);
        TTree* t = (TTree*)f->Get("tree");
        check(t != nullptr, "z_pol tree present");
        TBeamSimDataArray* arr = nullptr;
        t->SetBranchAddress("TBeamSimData", &arr);

        for (Long64_t i = 0; i < t->GetEntries(); ++i) {
            t->GetEntry(i);
            const double bphi = arr->at(0).fUserDouble[kBPhiIndex];
            check(bphi >= 0.0 && bphi < 2.0 * kPi,
                  "z_pol event " + std::to_string(i) + " b_phi in [0, 2π)");
        }
        f->Close();
    }

    std::cout << "ALL CHECKS PASSED" << std::endl;
}
```

- [ ] **Step 3: Register in CMake**

Append to `tests/integration/CMakeLists.txt`:
```cmake
add_test(NAME test_geninput_qmdrawdata_roundtrip
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_geninput_qmdrawdata_roundtrip.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(test_geninput_qmdrawdata_roundtrip PROPERTIES
    LABELS "integration"
)
```

- [ ] **Step 4: Build and run**

```bash
cd build && cmake .. && make -j$(nproc) GenInputRoot_qmdrawdata smdata
ctest -R test_geninput_qmdrawdata_roundtrip --output-on-failure
```
Expected: PASS, output ends with `ALL CHECKS PASSED`.

- [ ] **Step 5: Commit**

```bash
git add tests/integration/test_geninput_qmdrawdata_roundtrip.sh \
        tests/integration/test_geninput_qmdrawdata_roundtrip.C \
        tests/integration/CMakeLists.txt
git commit -m "test: add geninput_qmdrawdata roundtrip integration smoke"
```

---

## Task 10: Final regression check (run all unit + integration)

**Files:** none

- [ ] **Step 1: Full rebuild**

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_APPS=ON \
                     -DBUILD_ANALYSIS=ON -DWITH_ANAROOT=ON
make -j$(nproc)
```

- [ ] **Step 2: Run unit + integration test labels**

```bash
ctest -L "unit" --output-on-failure
ctest -L "integration" --output-on-failure
```
Expected: all PASS. Pre-existing test failures (visualization, performance, realdata labels excluded) are unrelated to this work — note them but do not block.

- [ ] **Step 3: Run smsim-geant4-io skill check**

```bash
./skills/smsim-geant4-io/scripts/check_sync.sh
```
Expected: all anchors found.

- [ ] **Step 4: Commit any incidental fixes**

If any test failed because of a path/include drift, fix and commit separately:
```bash
git commit -m "fix: address regression from <task#>"
```

---

## Self-Review Checklist (engineer before requesting review)

After completing all tasks, verify:

- [ ] `git log --oneline feature/qmd-g4-io-branches ^main` shows ~10 small commits, each one logically isolated
- [ ] `grep -rn "rotate-zpol\|rotate-ypol\|rotate_zpol\|rotate_ypol" scripts/ libs/ apps/ tests/` returns NO matches in code (only in spec/plan docs is OK)
- [ ] `grep -n "kBimpIndex\b" libs/smg4lib/include/QMDInputMetadata.hh` shows `kBimpIndex = 0`
- [ ] `./bin/GenInputRoot_qmdrawdata --help` shows `--randomize-zpol`, `--randomize-ypol`, `--rotation-seed`
- [ ] On a real y_pol/phi_random input, `arr[0].fUserDouble[1]` (b_phi) approximately matches `rpphi * π/180` from the input file (use a Sn124 sample if SSD mounted; otherwise rely on the integration test)
- [ ] On a real z_pol input run twice with the same `--rotation-seed`, output ROOT files have identical `truth_b_phi` per event

## Out of Scope (do NOT do in this plan)

- g4 end-to-end mini test (would need a stripped-down geometry + macro; deferred)
- Updating `extract_phys_observables.cc` to read the new truth branches (user has working-tree changes; will be a separate diff)
- Per-event branch refactor of `TBeamSimData` (per-particle replication is wasteful but compatible)
- Helicity / gamma_label / source_kind passthrough to output branches
- allevent (geminiout) path: `b_phi`/`phi_np_truth` left as NaN (spec §8)
