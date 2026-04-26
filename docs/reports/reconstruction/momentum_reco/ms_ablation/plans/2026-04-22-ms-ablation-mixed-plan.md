# MS Ablation Stage A′ (Beam-line Vacuum) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an orthogonal Geant4 geometry switch `/samurai/geometry/BeamLineVacuum` that puts the upstream target→dipole region and the dipole yoke interior into vacuum (`G4_Galactic`), while keeping the world air. Run three MS ablation scenarios (`all_air` / `beamline_vacuum` / `all_vacuum`) and publish a stage A′ memo that decomposes stage A's 94% "air contribution" into fake-upstream vs. real-downstream air.

**Architecture:** One new UI command wired into `DeutDetectorConstructionMessenger`. One new logical volume (`vchamber_cavity`) added inside `DipoleConstruction::ConstructSub()`, placed inside the yoke. Resurrect the already-implemented `VacuumUpstreamConstruction` (commented-out member in `DeutDetectorConstruction.hh:101`) by instantiating it in the ctor and placing it in `Construct()` under the new flag. Default `false` → zero behavior change for every existing test / macro. Extend `run_ablation.sh` and `compare_ablation.py` from 2-way to 3-way comparisons.

**Tech Stack:** C++20 (Geant4 10.x), Python 3 (pandas, matplotlib, pytest), bash, CMake, CTest + GoogleTest. Micromamba env `anaroot-env`.

**Spec:** [`../specs/2026-04-22-ms-ablation-mixed-design.md`](../specs/2026-04-22-ms-ablation-mixed-design.md)

---

## Scene-Setting Context (for every implementer)

Source-of-truth file paths (read these before starting if unfamiliar):

- `libs/sim_deuteron_core/include/DeutDetectorConstruction.hh` — line 13 has `class VacuumDownstreamConstruction;`; line 98 has `fVacuumDownstreamConstruction`; line 101 has a commented-out `// VacuumUpstreamConstruction *fVacuumUpstreamConstruction;` — we resurrect this.
- `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc` — line 52 includes `VacuumDownstreamConstruction.hh`; lines 73, 85 instantiate/delete `fVacuumDownstreamConstruction`; lines 190–199 place `vacuum_downstream_log`. We will mirror this pattern for VacuumUpstream.
- `libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc` — lines 26–29 define `fFillAirCmd`; lines 173–174 dispatch it. We copy that pattern for `fBeamLineVacuumCmd`.
- `libs/sim_deuteron_core/include/DeutDetectorConstructionMessenger.hh` — line 28 declares `fFillAirCmd`; we add `fBeamLineVacuumCmd` nearby.
- `libs/smg4lib/src/construction/include/DipoleConstruction.hh` — add `fBeamLineVacuum` + `SetBeamLineVacuum`.
- `libs/smg4lib/src/construction/src/DipoleConstruction.cc` — lines 99–116 build `vchamber_box`, `vchamber_trap`, and the yoke `G4SubtractionSolid`. We add a new `vchamber_cavity` union solid + logical volume + placement inside the yoke, using the same `vct_rm`/`vct_trans`.
- `libs/smg4lib/src/construction/include/VacuumUpstreamConstruction.hh` / `src/VacuumUpstreamConstruction.cc` — the class already exists; `ConstructSub(G4VPhysicalVolume* dipole_phys)` is implemented (line 79 of the `.cc`).
- `scripts/analysis/ms_ablation/run_ablation.sh` — currently runs 2 conditions via `run_condition()`. We change to 3 conditions.
- `scripts/analysis/ms_ablation/compare_ablation.py` / `tests/test_compare_ablation.py` — currently 2-way. We extend to 3-way.
- `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac` — stage A baseline macro.
- `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_noair.mac` — stage A no_air macro.

Build commands:
```bash
cd /home/tian/workspace/dpol/smsimulator5.5
micromamba activate anaroot-env    # environment
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_APPS=ON -DBUILD_ANALYSIS=ON -DWITH_ANAROOT=ON
make -j$(nproc)
cd ..
ctest --test-dir build --output-on-failure  # regression check
```

Critical invariant: `BeamLineVacuum` default = `false` ⇒ new code has **zero observable effect** (no extra logical volume placed, dipole cavity logical volume material equals world material). Every existing CTest must stay green without modification.

---

## Task 1: Add `BeamLineVacuum` UI command + setter plumbing

**Files:**
- Modify: `libs/sim_deuteron_core/include/DeutDetectorConstructionMessenger.hh`
- Modify: `libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc`
- Modify: `libs/sim_deuteron_core/include/DeutDetectorConstruction.hh`
- Modify: `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`

- [ ] **Step 1: Add `fBeamLineVacuumCmd` to Messenger header**

In `libs/sim_deuteron_core/include/DeutDetectorConstructionMessenger.hh`, after line 28 (`G4UIcmdWithABool* fFillAirCmd;`), add:

```cpp
  G4UIcmdWithABool* fBeamLineVacuumCmd;
```

- [ ] **Step 2: Construct the UI command in Messenger ctor**

In `libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc`, after the `fFillAirCmd` block (ends at line 29), add:

```cpp
  fBeamLineVacuumCmd = new G4UIcmdWithABool("/samurai/geometry/BeamLineVacuum",this);
  fBeamLineVacuumCmd->SetGuidance("Put target-to-dipole and dipole yoke interior in vacuum (G4_Galactic).");
  fBeamLineVacuumCmd->SetGuidance("Orthogonal to FillAir which controls the world material.");
  fBeamLineVacuumCmd->SetParameterName("BeamLineVacuum",true);
  fBeamLineVacuumCmd->SetDefaultValue(false);
```

- [ ] **Step 3: Delete the UI command in Messenger dtor**

In `libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc`, after `delete fFillAirCmd;` (line 141), add:

```cpp
  delete fBeamLineVacuumCmd;
```

- [ ] **Step 4: Dispatch the command in Messenger::SetNewValue**

In `libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc`, after the `fFillAirCmd` dispatch block (lines 173–175), add another `else if` branch:

```cpp
  }else if ( command == fBeamLineVacuumCmd ){
    fDetectorConstruction->SetBeamLineVacuum(fBeamLineVacuumCmd->GetNewBoolValue(newValue));
```

- [ ] **Step 5: Add `fBeamLineVacuum` member + setter/getter to DeutDetectorConstruction header**

In `libs/sim_deuteron_core/include/DeutDetectorConstruction.hh`:

1. Change line 13 from a comment-only `// VacuumDownstreamConstruction; 添加前向声明` context to also declare VacuumUpstream. After line 13 (`class VacuumDownstreamConstruction;`), add:

```cpp
class VacuumUpstreamConstruction;
```

2. After line 50 (`void SetFillAir(G4bool tf){fFillAir = tf;}`), add:

```cpp
  void SetBeamLineVacuum(G4bool tf){fBeamLineVacuum = tf;}
  G4bool IsBeamLineVacuum() const { return fBeamLineVacuum; }
```

3. After line 60 (`G4bool fFillAir;`), add:

```cpp
  G4bool fBeamLineVacuum;
```

4. Replace line 101 (currently `  // VacuumUpstreamConstruction *fVacuumUpstreamConstruction;`) with the uncommented version:

```cpp
  VacuumUpstreamConstruction *fVacuumUpstreamConstruction;
```

Also remove line 102 (`  // VacuumDownstreamConstruction *fVacuumDownstreamConstruction;` — duplicate commented-out line).

- [ ] **Step 6: Initialize + allocate + delete `fVacuumUpstreamConstruction` + `fBeamLineVacuum` in DeutDetectorConstruction**

In `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`:

1. After the include for `VacuumDownstreamConstruction.hh` (line 52), add:

```cpp
#include "VacuumUpstreamConstruction.hh"
```

2. Update the ctor init list (line 58). Change:

```cpp
  fFillAir{false}, fSetTarget{false}, fSetDump{true}, fSetIPS{false}, fTargetMat{"Sn"},
```

to:

```cpp
  fFillAir{false}, fBeamLineVacuum{false}, fSetTarget{false}, fSetDump{true}, fSetIPS{false}, fTargetMat{"Sn"},
```

3. After line 73 (`fVacuumDownstreamConstruction = new VacuumDownstreamConstruction();`), add:

```cpp
  fVacuumUpstreamConstruction = new VacuumUpstreamConstruction();
```

4. After line 85 (`delete fVacuumDownstreamConstruction;`), add:

```cpp
  delete fVacuumUpstreamConstruction;
```

- [ ] **Step 7: Build + smoke test**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
micromamba activate anaroot-env
cmake --build build -j$(nproc) 2>&1 | tail -20
```

Expected: compiles cleanly, zero warnings about unused variables. If any "member initializer list" warnings appear, verify the ordering in Step 6.2 matches the header declaration order from Step 5.3.

- [ ] **Step 8: Regression — existing CTests still pass**

```bash
ctest --test-dir build --output-on-failure -j$(nproc) 2>&1 | tail -40
```

Expected: **100% of previously-passing tests still pass**. Zero new failures. If a test fails, it means the default-false behavior is not bit-for-bit preserved — stop and investigate.

- [ ] **Step 9: Commit**

```bash
git add libs/sim_deuteron_core/include/DeutDetectorConstruction.hh \
        libs/sim_deuteron_core/src/DeutDetectorConstruction.cc \
        libs/sim_deuteron_core/include/DeutDetectorConstructionMessenger.hh \
        libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc
git commit -m "feat: add BeamLineVacuum geometry flag + resurrect VacuumUpstream member

Introduces /samurai/geometry/BeamLineVacuum (default false). When true,
upstream target-to-dipole region and dipole yoke interior will be filled
with G4_Galactic instead of world material. Default false preserves
bit-for-bit behavior; no physics change yet until subsequent tasks wire
up the actual volumes."
```

---

## Task 2: Add `fBeamLineVacuum` to `DipoleConstruction` + build `vchamber_cavity` logical volume

**Files:**
- Modify: `libs/smg4lib/src/construction/include/DipoleConstruction.hh`
- Modify: `libs/smg4lib/src/construction/src/DipoleConstruction.cc`

- [ ] **Step 1: Add `fBeamLineVacuum` member + setter to DipoleConstruction header**

In `libs/smg4lib/src/construction/include/DipoleConstruction.hh`, after line 33 (`Double_t GetFieldFactor(){return fMagField->GetFieldFactor();}`), add:

```cpp
  void SetBeamLineVacuum(G4bool tf){fBeamLineVacuum = tf;}
```

After line 36 (`G4double fAngle;`), add:

```cpp
  G4bool fBeamLineVacuum;
```

- [ ] **Step 2: Initialize `fBeamLineVacuum` to `false` in ctor**

In `libs/smg4lib/src/construction/src/DipoleConstruction.cc`, change line 34 from:

```cpp
  : fAngle(0), fLogicDipole(0),
    fMagField(0), fMagFieldFactor(1.0)
```

to:

```cpp
  : fAngle(0), fBeamLineVacuum(false), fLogicDipole(0),
    fMagField(0), fMagFieldFactor(1.0)
```

- [ ] **Step 3: Add `G4PVPlacement.hh` and `G4UnionSolid.hh` includes if not present**

In `libs/smg4lib/src/construction/src/DipoleConstruction.cc`, verify lines 6–13 already include `G4PVPlacement.hh` (line 12) and `G4UnionSolid.hh` (line 8). If either missing, add.

- [ ] **Step 4: Build the `vchamber_cavity` union solid + logical volume + placement**

In `libs/smg4lib/src/construction/src/DipoleConstruction.cc`, at the end of `ConstructSub()` — right before `return fLogicDipole;` (currently line 121) — insert:

```cpp
  //------------------------------- vchamber cavity volume
  // [EN] Place a logical volume that fills the void carved out of the yoke by
  // vchamber_box + vchamber_trap. Without this, the cavity inherits world
  // material (air when FillAir=true). When fBeamLineVacuum=true the cavity is
  // explicitly G4_Galactic; when false it tracks world material so behavior
  // is bit-for-bit identical to the pre-change geometry.
  // [CN] 给 yoke 内被挖掉的 void 安一个显式 logical volume；
  // fBeamLineVacuum=true 时为真空，false 时跟随世界材质保持既有行为。
  G4VSolid* vchamber_cavity_sol = new G4UnionSolid(
      "vchamber_cavity", vchamber_box, vchamber_trap, vct_rm, *vct_trans);
  G4Material* cavity_mat = fBeamLineVacuum ? fWorldMaterial  // placeholder; fWorldMaterial is G4_Galactic in ctor
                                           : fWorldMaterial;
  // fWorldMaterial in this class's ctor is G4_Galactic regardless. We need the
  // actual world (expHall) material, which matches what the Geant4 world box
  // is made of. Use the G4Material of expHall_log via a lookup.
  if (fBeamLineVacuum) {
    cavity_mat = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  } else {
    // Default-false branch: match world material so bit-for-bit behavior is preserved.
    // Look up expHall's current material via the logical-volume store.
    auto* lvs = G4LogicalVolumeStore::GetInstance();
    auto* expHall_lv = lvs->GetVolume("expHall_log", false);
    cavity_mat = expHall_lv ? expHall_lv->GetMaterial()
                            : G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  }
  G4LogicalVolume* cavity_log = new G4LogicalVolume(
      vchamber_cavity_sol, cavity_mat, "vchamber_cavity_log");
  cavity_log->SetVisAttributes(new G4VisAttributes(G4Colour(0.8, 0.8, 1.0, 0.2)));
  new G4PVPlacement(nullptr, {0,0,0}, cavity_log, "vchamber_cavity",
                    fLogicDipole, false, 0, true);  // checkOverlaps=true
```

**Important:** Simplify the above to the final clean version:

```cpp
  //------------------------------- vchamber cavity volume
  // [EN] Place a logical volume inside the yoke that fills the void carved
  // out by vchamber_box + vchamber_trap. Without it the cavity inherits world
  // material. fBeamLineVacuum=true: vacuum; false: match world material
  // (bit-for-bit compatible with the pre-change geometry).
  // [CN] 给 yoke 内被挖掉的 void 安一个显式 logical volume。
  G4VSolid* vchamber_cavity_sol = new G4UnionSolid(
      "vchamber_cavity", vchamber_box, vchamber_trap, vct_rm, *vct_trans);
  G4Material* cavity_mat = nullptr;
  if (fBeamLineVacuum) {
    cavity_mat = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  } else {
    auto* lvs = G4LogicalVolumeStore::GetInstance();
    auto* expHall_lv = lvs->GetVolume("expHall_log", false);
    cavity_mat = expHall_lv ? expHall_lv->GetMaterial()
                            : G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  }
  G4LogicalVolume* cavity_log = new G4LogicalVolume(
      vchamber_cavity_sol, cavity_mat, "vchamber_cavity_log");
  cavity_log->SetVisAttributes(new G4VisAttributes(G4Colour(0.8, 0.8, 1.0, 0.2)));
  new G4PVPlacement(nullptr, {0,0,0}, cavity_log, "vchamber_cavity",
                    fLogicDipole, false, 0, true);  // checkOverlaps=true
```

(The `if-else` with simplify reasoning is what goes in. Delete the earlier "placeholder" version — it was for explanation only.)

- [ ] **Step 5: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -20
```

Expected: compiles, no warnings. If "undefined reference" to `G4LogicalVolumeStore::GetInstance`, add `#include "G4LogicalVolumeStore.hh"` near the top of the `.cc` file (it is already there at line 16 of the original).

- [ ] **Step 6: Verify no CheckOverlaps violation via 1-event smoke**

Create a minimal 1-event macro `build/ms_ablation_smoke.mac`:

```bash
cat > build/ms_ablation_smoke.mac <<'EOF'
/control/execute configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
/run/initialize
/run/verbose 0
/event/verbose 0
/tracking/verbose 0
/run/beamOn 1
EOF
```

Wait — actually `sim_deuteron` needs a full input macro including particle gun setup. The cleanest smoke is the existing CTest suite which already calls `Construct()`:

```bash
ctest --test-dir build --output-on-failure -R "Geometry|Geant4" 2>&1 | tail -30
```

Expected: 0 failures. If Geant4 reports "overlap" during geometry construction, the union solid or placement is wrong — investigate before proceeding.

- [ ] **Step 7: Full regression**

```bash
ctest --test-dir build --output-on-failure -j$(nproc) 2>&1 | tail -40
```

Expected: all previously-passing tests still pass.

- [ ] **Step 8: Commit**

```bash
git add libs/smg4lib/src/construction/include/DipoleConstruction.hh \
        libs/smg4lib/src/construction/src/DipoleConstruction.cc
git commit -m "feat: add vchamber_cavity logical volume inside Dipole yoke

The yoke G4SubtractionSolid carves out vchamber_box + vchamber_trap but
leaves no explicit volume for the cavity; until now the cavity inherited
world material. Add an explicit vchamber_cavity logical volume whose
material is G4_Galactic when fBeamLineVacuum=true, or world material
when false. Default false preserves bit-for-bit pre-change behavior.
Placement checkOverlaps=true catches any misalignment with the yoke's
subtraction solid."
```

---

## Task 3: Wire DeutDetectorConstruction to forward `fBeamLineVacuum` to Dipole + place VacuumUpstream

**Files:**
- Modify: `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`

- [ ] **Step 1: Forward `fBeamLineVacuum` to DipoleConstruction before ConstructSub**

In `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`, change line 147 from:

```cpp
  fDipoleConstruction->ConstructSub();
```

to:

```cpp
  fDipoleConstruction->SetBeamLineVacuum(fBeamLineVacuum);
  fDipoleConstruction->ConstructSub();
```

- [ ] **Step 2: Add VacuumUpstream placement under `fBeamLineVacuum` guard**

In `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`, after line 199 (the closing `new G4PVPlacement(...)` for vacuum_downstream) and before `// ...existing code...` at line 201, insert:

```cpp

  //------------------------------ Vacuum Upstream (target → dipole)
  // [EN] Only place when fBeamLineVacuum=true; keeps default-false bit-for-bit
  // identical to pre-change geometry (no extra LV in the world).
  // [CN] 仅 fBeamLineVacuum=true 时拼装；默认 false 保持 bit-for-bit 兼容。
  if (fBeamLineVacuum) {
    G4LogicalVolume *vacuum_upstream_log =
      fVacuumUpstreamConstruction->ConstructSub(Dipole_phys);
    G4ThreeVector vacuum_upstream_pos = fVacuumUpstreamConstruction->GetPosition();
    new G4PVPlacement(nullptr, vacuum_upstream_pos,
                      vacuum_upstream_log, "vacuum_upstream",
                      expHall_log, false, 0, true);  // checkOverlaps=true
  }
```

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -20
```

Expected: compiles.

- [ ] **Step 4: Run full CTest suite (default BeamLineVacuum=false)**

```bash
ctest --test-dir build --output-on-failure -j$(nproc) 2>&1 | tail -40
```

Expected: **100% previously-passing tests still pass** (default-false = no behavior change).

- [ ] **Step 5: Commit**

```bash
git add libs/sim_deuteron_core/src/DeutDetectorConstruction.cc
git commit -m "feat: wire BeamLineVacuum to Dipole cavity + VacuumUpstream placement

Pass fBeamLineVacuum into DipoleConstruction before ConstructSub so the
new vchamber_cavity gets the right material. Place VacuumUpstream under
an fBeamLineVacuum guard; default false path places no extra volume and
preserves bit-for-bit existing geometry."
```

---

## Task 4: Create `_target3deg_mixed.mac` + macro-diff test

**Files:**
- Create: `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac`
- Create: `scripts/analysis/ms_ablation/tests/test_macro_diff_mixed.sh`

- [ ] **Step 1: Write failing macro-diff test**

Create `scripts/analysis/ms_ablation/tests/test_macro_diff_mixed.sh`:

```bash
#!/bin/bash
# Verify _mixed.mac = _target3deg.mac + exactly one appended BeamLineVacuum line.
set -euo pipefail
cd "$(dirname "$0")/../../../.."    # repo root

BASELINE=configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
MIXED=configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac

if [[ ! -f "$MIXED" ]]; then
    echo "FAIL: $MIXED 不存在" >&2
    exit 1
fi

# Mixed should have exactly one additional line: BeamLineVacuum true
# The Update line is LAST in baseline; BeamLineVacuum must come BEFORE Update.
# Strategy: mixed = baseline with one inserted line right before the Update line.
NDIFF=$(diff "$BASELINE" "$MIXED" | grep -c '^[<>]' || true)
if [[ "$NDIFF" != "1" ]]; then
    echo "FAIL: 期望一条 > 行 (插入 BeamLineVacuum)，实得 $NDIFF" >&2
    diff "$BASELINE" "$MIXED" >&2 || true
    exit 1
fi

if ! grep -q '^/samurai/geometry/BeamLineVacuum true$' "$MIXED"; then
    echo "FAIL: $MIXED 里没有 'BeamLineVacuum true'" >&2
    exit 1
fi

if ! grep -q '^/samurai/geometry/FillAir true$' "$MIXED"; then
    echo "FAIL: $MIXED 里 FillAir 应保持 true（空气世界 + 真空束流管）" >&2
    exit 1
fi

# Update must still be the last non-empty line
LAST_CMD=$(grep -E '^/samurai/' "$MIXED" | tail -1)
if [[ "$LAST_CMD" != "/samurai/geometry/Update" ]]; then
    echo "FAIL: Update 应为最后一条 /samurai 命令，实得 '$LAST_CMD'" >&2
    exit 1
fi

echo "PASS: mixed macro diff 检查通过"
```

Make it executable:

```bash
chmod +x scripts/analysis/ms_ablation/tests/test_macro_diff_mixed.sh
```

- [ ] **Step 2: Run test to verify it fails**

```bash
bash scripts/analysis/ms_ablation/tests/test_macro_diff_mixed.sh
```

Expected: `FAIL: ...mixed.mac 不存在`.

- [ ] **Step 3: Create the mixed macro**

The baseline macro is 22 lines. Copy it and insert `BeamLineVacuum true` right before `/samurai/geometry/Update` (line 21):

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
MAC_BASELINE=configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
MAC_MIXED=configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac
awk '/^\/samurai\/geometry\/Update$/ && !ins { print "/samurai/geometry/BeamLineVacuum true"; ins=1 } { print }' "$MAC_BASELINE" > "$MAC_MIXED"
```

Verify:

```bash
diff "$MAC_BASELINE" "$MAC_MIXED"
```

Expected one diff:

```
21a22
> /samurai/geometry/BeamLineVacuum true
```

Wait — the `awk` inserts *before* the Update line, so diff shows `20a21` with the new line. The exact line number depends on your file. Either position works as long as it's before `Update`.

- [ ] **Step 4: Run test to verify it passes**

```bash
bash scripts/analysis/ms_ablation/tests/test_macro_diff_mixed.sh
```

Expected: `PASS: mixed macro diff 检查通过`.

- [ ] **Step 5: Commit**

```bash
git add configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac \
        scripts/analysis/ms_ablation/tests/test_macro_diff_mixed.sh
git commit -m "feat: add _target3deg_mixed.mac (FillAir=true, BeamLineVacuum=true)

Experimentally-realistic scenario for stage A': world stays air, but
the target-to-dipole and dipole yoke interior are vacuum (matching
the real SAMURAI beam line). Test enforces one-line delta from
baseline + the correct BeamLineVacuum value."
```

---

## Task 5: Extend `compare_ablation.py` to 3-way comparison (TDD)

**Files:**
- Modify: `scripts/analysis/ms_ablation/compare_ablation.py`
- Modify: `scripts/analysis/ms_ablation/tests/test_compare_ablation.py`

- [ ] **Step 1: Write failing tests**

Replace the contents of `scripts/analysis/ms_ablation/tests/test_compare_ablation.py` with:

```python
import sys
from pathlib import Path

import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compare_ablation import build_comparison


def _sample_summary(sigma_y_pdc2, truth_px=0.0, truth_py=0.0):
    return pd.DataFrame([{
        "truth_px": truth_px, "truth_py": truth_py, "N": 50,
        "sigma_x_pdc1_mm": 2.8, "sigma_y_pdc1_mm": 11.2,
        "sigma_x_pdc2_mm": 3.5, "sigma_y_pdc2_mm": sigma_y_pdc2,
        "sigma_theta_x_mrad": 3.5, "sigma_theta_y_mrad": 8.0,
    }])


def test_three_way_all_conditions_present():
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    assert len(out) == 3
    assert set(out["condition"]) == {"all_air", "beamline_vacuum", "all_vacuum"}


def test_three_way_delta_upstream_air():
    """delta_sigma_y_pdc2_mm_upstream_air = all_air - beamline_vacuum."""
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    row = out[out["condition"] == "all_air"].iloc[0]
    assert row["delta_sigma_y_pdc2_mm_upstream_air"] == pytest.approx(15.2 - 10.5)


def test_three_way_delta_downstream_air():
    """delta_sigma_y_pdc2_mm_downstream_air = beamline_vacuum - all_vacuum."""
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    row = out[out["condition"] == "beamline_vacuum"].iloc[0]
    assert row["delta_sigma_y_pdc2_mm_downstream_air"] == pytest.approx(10.5 - 1.0)


def test_three_way_delta_total_air():
    """delta_sigma_y_pdc2_mm_total_air = all_air - all_vacuum."""
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    row = out[out["condition"] == "all_air"].iloc[0]
    assert row["delta_sigma_y_pdc2_mm_total_air"] == pytest.approx(15.2 - 1.0)


def test_three_way_all_vacuum_has_no_delta():
    """all_vacuum is the reference; its delta columns must be NaN."""
    all_air   = _sample_summary(15.2)
    mixed     = _sample_summary(10.5)
    all_vac   = _sample_summary(1.0)
    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    row = out[out["condition"] == "all_vacuum"].iloc[0]
    assert pd.isna(row["delta_sigma_y_pdc2_mm_upstream_air"])
    assert pd.isna(row["delta_sigma_y_pdc2_mm_downstream_air"])
    assert pd.isna(row["delta_sigma_y_pdc2_mm_total_air"])


def test_three_way_multiple_truth_points():
    a1 = _sample_summary(15.2, truth_px=0.0,   truth_py=0.0)
    a2 = _sample_summary(12.3, truth_px=100.0, truth_py=0.0)
    all_air = pd.concat([a1, a2], ignore_index=True)

    m1 = _sample_summary(10.5, truth_px=0.0,   truth_py=0.0)
    m2 = _sample_summary(8.1,  truth_px=100.0, truth_py=0.0)
    mixed = pd.concat([m1, m2], ignore_index=True)

    v1 = _sample_summary(1.0, truth_px=0.0,   truth_py=0.0)
    v2 = _sample_summary(0.9, truth_px=100.0, truth_py=0.0)
    all_vac = pd.concat([v1, v2], ignore_index=True)

    out = build_comparison(all_air=all_air, beamline_vacuum=mixed, all_vacuum=all_vac)
    assert len(out) == 6  # 2 truth × 3 conditions

    # tp (0,0) upstream-air delta on all_air row
    r = out[(out["truth_px"] == 0.0) & (out["condition"] == "all_air")].iloc[0]
    assert r["delta_sigma_y_pdc2_mm_upstream_air"] == pytest.approx(15.2 - 10.5)
    # tp (100,0) downstream-air delta on beamline_vacuum row
    r = out[(out["truth_px"] == 100.0) & (out["condition"] == "beamline_vacuum")].iloc[0]
    assert r["delta_sigma_y_pdc2_mm_downstream_air"] == pytest.approx(8.1 - 0.9)
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
micromamba run -n anaroot-env pytest scripts/analysis/ms_ablation/tests/test_compare_ablation.py -v
```

Expected: multiple failures (the new signature of `build_comparison` doesn't exist yet).

- [ ] **Step 3: Rewrite `build_comparison` to 3-way**

Replace the contents of `scripts/analysis/ms_ablation/compare_ablation.py` with:

```python
#!/usr/bin/env python3
"""Compare three MS ablation conditions and produce:
  - compare.csv  (stacked rows with three delta columns on the all_air and
    beamline_vacuum rows: upstream_air, downstream_air, total_air)
  - figures/dispersion_overlay_<px>_<py>.png  (3-color scatter per truth point)
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd


SIGMA_COLS = [
    "sigma_x_pdc1_mm", "sigma_y_pdc1_mm",
    "sigma_x_pdc2_mm", "sigma_y_pdc2_mm",
    "sigma_theta_x_mrad", "sigma_theta_y_mrad",
]

CONDITION_ORDER = ["all_air", "beamline_vacuum", "all_vacuum"]

# (delta_suffix, minuend_condition, subtrahend_condition)
# "upstream_air"   = all_air - beamline_vacuum (fake upstream air contribution)
# "downstream_air" = beamline_vacuum - all_vacuum (real downstream air contribution)
# "total_air"      = all_air - all_vacuum (stage A's original number)
DELTAS = [
    ("upstream_air",   "all_air",         "beamline_vacuum"),
    ("downstream_air", "beamline_vacuum", "all_vacuum"),
    ("total_air",      "all_air",         "all_vacuum"),
]


def _delta(minuend_row, subtrahend_lookup, col):
    key = (minuend_row["truth_px"], minuend_row["truth_py"])
    if key not in subtrahend_lookup.index:
        return float("nan")
    return minuend_row[col] - subtrahend_lookup.loc[key, col]


def build_comparison(
    all_air: pd.DataFrame,
    beamline_vacuum: pd.DataFrame,
    all_vacuum: pd.DataFrame,
) -> pd.DataFrame:
    frames = {
        "all_air":         all_air.copy(),
        "beamline_vacuum": beamline_vacuum.copy(),
        "all_vacuum":      all_vacuum.copy(),
    }
    for name, df in frames.items():
        df["condition"] = name

    key = ["truth_px", "truth_py"]
    lookups = {name: df.set_index(key) for name, df in frames.items()}

    for suffix, minuend, subtrahend in DELTAS:
        sub_lookup = lookups[subtrahend]
        for col in SIGMA_COLS:
            delta_col = f"delta_{col}_{suffix}"
            for name, df in frames.items():
                if name == minuend:
                    df[delta_col] = df.apply(
                        lambda r: _delta(r, sub_lookup, col), axis=1)
                else:
                    df[delta_col] = float("nan")

    return pd.concat(
        [frames[name] for name in CONDITION_ORDER],
        ignore_index=True, sort=False)


def _render_overlays(hits_by_condition: dict[str, pd.DataFrame],
                     figures_dir: Path, truth_points: list[tuple[float, float]]):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    colors = {
        "all_air":         "tab:gray",
        "beamline_vacuum": "tab:blue",
        "all_vacuum":      "tab:green",
    }
    figures_dir.mkdir(parents=True, exist_ok=True)
    for tpx, tpy in truth_points:
        fig, axes = plt.subplots(1, 2, figsize=(12, 5))
        for ax, which in zip(axes, ("pdc1", "pdc2")):
            for name in CONDITION_ORDER:
                df = hits_by_condition.get(name)
                if df is None:
                    continue
                sub = df[(df["truth_px"] == tpx) & (df["truth_py"] == tpy)]
                ax.scatter(sub[f"{which}_x"], sub[f"{which}_y"],
                           c=colors[name], s=18, alpha=0.6,
                           label=f"{name} (N={len(sub)})")
            ax.set_xlabel(f"{which.upper()} x (mm)")
            ax.set_ylabel(f"{which.upper()} y (mm)")
            ax.set_title(f"{which.upper()} hits @ truth=({tpx:.0f},{tpy:.0f})")
            ax.legend(loc="best", fontsize=9)
            ax.grid(alpha=0.3)
        fig.tight_layout()
        out_path = figures_dir / f"dispersion_overlay_tp{int(tpx)}_{int(tpy)}.png"
        fig.savefig(out_path, dpi=120)
        plt.close(fig)
        print(f"[compare] wrote {out_path}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--all-air-dir",         type=Path, required=True)
    parser.add_argument("--beamline-vacuum-dir", type=Path, required=True)
    parser.add_argument("--all-vacuum-dir",      type=Path, required=True)
    parser.add_argument("--out-dir",             type=Path, required=True)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)

    summaries = {
        "all_air":         pd.read_csv(args.all_air_dir         / "dispersion_summary.csv"),
        "beamline_vacuum": pd.read_csv(args.beamline_vacuum_dir / "dispersion_summary.csv"),
        "all_vacuum":      pd.read_csv(args.all_vacuum_dir      / "dispersion_summary.csv"),
    }

    compare = build_comparison(
        all_air         = summaries["all_air"],
        beamline_vacuum = summaries["beamline_vacuum"],
        all_vacuum      = summaries["all_vacuum"])
    compare_path = args.out_dir / "compare.csv"
    compare.to_csv(compare_path, index=False)
    print(f"[compare] wrote {compare_path}", flush=True)
    print(compare.to_string(index=False))

    hits = {}
    for name, d in [("all_air",         args.all_air_dir),
                    ("beamline_vacuum", args.beamline_vacuum_dir),
                    ("all_vacuum",      args.all_vacuum_dir)]:
        p = d / "pdc_hits.csv"
        if p.exists():
            hits[name] = pd.read_csv(p)
    if hits:
        truth_points = list(zip(summaries["all_air"]["truth_px"],
                                summaries["all_air"]["truth_py"]))
        _render_overlays(hits, args.out_dir / "figures", truth_points)


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
micromamba run -n anaroot-env pytest scripts/analysis/ms_ablation/tests/test_compare_ablation.py -v
```

Expected: all tests green.

- [ ] **Step 5: Commit**

```bash
git add scripts/analysis/ms_ablation/compare_ablation.py \
        scripts/analysis/ms_ablation/tests/test_compare_ablation.py
git commit -m "feat: extend compare_ablation.py to 3-way (all_air/beamline_vacuum/all_vacuum)

Three new delta columns per sigma metric:
- delta_*_upstream_air   = all_air - beamline_vacuum (fake upstream air)
- delta_*_downstream_air = beamline_vacuum - all_vacuum (real downstream air)
- delta_*_total_air      = all_air - all_vacuum (stage A's original)

CLI uses --all-air-dir / --beamline-vacuum-dir / --all-vacuum-dir flags
replacing the prior baseline/no_air pair. Overlay figures get a 3rd
condition in distinct color."
```

---

## Task 6: Update `run_ablation.sh` to 3-way

**Files:**
- Modify: `scripts/analysis/ms_ablation/run_ablation.sh`

- [ ] **Step 1: Rewrite the script**

Replace the contents of `scripts/analysis/ms_ablation/run_ablation.sh` with:

```bash
#!/bin/bash
# Run the MS ablation experiment v2 (stage A').
# Three conditions:
#   - all_air         : FillAir=true,  BeamLineVacuum=false (= stage A baseline)
#   - beamline_vacuum : FillAir=true,  BeamLineVacuum=true  (NEW; experimentally real)
#   - all_vacuum      : FillAir=false                        (= stage A no_air)
# 3 truth points × ENSEMBLE_SIZE seeds per condition.
set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
BUILD_DIR=${BUILD_DIR:-${SMSIM_DIR}/build}
ENSEMBLE_SIZE=${ENSEMBLE_SIZE:-50}
SEED_A=${SEED_A:-20260421}
SEED_B=${SEED_B:-20260422}
OUT_ROOT=${OUT_ROOT:-${BUILD_DIR}/test_output/ms_ablation_air}
RK_FIT_MODE=${RK_FIT_MODE:-fixed-target-pdc-only}

# Normalize to absolute paths.
if [[ "${OUT_ROOT}" != /* ]]; then OUT_ROOT="$(pwd)/${OUT_ROOT}"; fi
if [[ "${BUILD_DIR}" != /* ]]; then BUILD_DIR="$(pwd)/${BUILD_DIR}"; fi

MAC_ALL_AIR=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
MAC_MIXED=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac
MAC_ALL_VAC=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_noair.mac
FIELD_MAP=${SMSIM_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table
NN_MODEL_JSON=${NN_MODEL_JSON:-${SMSIM_DIR}/data/nn_target_momentum/domain_matched_retrain/20260228_002757/model/model_cpp.json}

for f in "$MAC_ALL_AIR" "$MAC_MIXED" "$MAC_ALL_VAC" "$FIELD_MAP" "$NN_MODEL_JSON" \
         "$BUILD_DIR/bin/test_pdc_target_momentum_e2e" \
         "$BUILD_DIR/bin/sim_deuteron" \
         "$BUILD_DIR/bin/reconstruct_target_momentum" \
         "$BUILD_DIR/bin/evaluate_target_momentum_reco"; do
    [[ -e "$f" ]] || { echo "MISSING: $f" >&2; exit 1; }
done

echo "=== MS ablation Air v2 (stage A') ==="
echo "git HEAD:      $(cd "$SMSIM_DIR" && git rev-parse HEAD)"
echo "ENSEMBLE_SIZE: $ENSEMBLE_SIZE"
echo "SEEDS:         A=$SEED_A B=$SEED_B"
echo "OUT_ROOT:      $OUT_ROOT"
echo

run_condition() {
    local label=$1
    local mac=$2
    local outdir=${OUT_ROOT}/${label}
    local simdir=${outdir}/sim

    mkdir -p "$simdir"
    echo "--- [$label] simulating with $(basename "$mac") ---"

    "$BUILD_DIR/bin/test_pdc_target_momentum_e2e" \
        --backend rk \
        --smsimdir "$SMSIM_DIR" \
        --output-dir "$simdir" \
        --sim-bin "$BUILD_DIR/bin/sim_deuteron" \
        --reco-bin "$BUILD_DIR/bin/reconstruct_target_momentum" \
        --eval-bin "$BUILD_DIR/bin/evaluate_target_momentum_reco" \
        --geometry-macro "$mac" \
        --field-map "$FIELD_MAP" \
        --nn-model-json "$NN_MODEL_JSON" \
        --threshold-px 500 --threshold-py 500 --threshold-pz 500 \
        --seed-a "$SEED_A" --seed-b "$SEED_B" \
        --rk-fit-mode "$RK_FIT_MODE" \
        --require-gate-pass 0 --require-min-matched 1 \
        --ensemble-size "$ENSEMBLE_SIZE"

    local reco_root=${simdir}/reco/pdc_truth_grid_rk_${RK_FIT_MODE//-/_}_reco.root
    [[ -f "$reco_root" ]] || { echo "MISSING reco root: $reco_root" >&2; exit 1; }

    echo "--- [$label] computing M1+M2 metrics ---"
    micromamba run -n anaroot-env python3 \
        "$SMSIM_DIR/scripts/analysis/ms_ablation/compute_metrics.py" \
        --reco-root "$reco_root" \
        --out-dir "$outdir"
}

run_condition "all_air"         "$MAC_ALL_AIR"
run_condition "beamline_vacuum" "$MAC_MIXED"
run_condition "all_vacuum"      "$MAC_ALL_VAC"

echo
echo "=== Comparing three conditions ==="
micromamba run -n anaroot-env python3 \
    "$SMSIM_DIR/scripts/analysis/ms_ablation/compare_ablation.py" \
    --all-air-dir         "$OUT_ROOT/all_air" \
    --beamline-vacuum-dir "$OUT_ROOT/beamline_vacuum" \
    --all-vacuum-dir      "$OUT_ROOT/all_vacuum" \
    --out-dir             "$OUT_ROOT"

echo
echo "=== Done. Outputs under: $OUT_ROOT ==="
ls -l "$OUT_ROOT"
```

- [ ] **Step 2: Smoke-run with ENSEMBLE_SIZE=2**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
ENSEMBLE_SIZE=2 bash scripts/analysis/ms_ablation/run_ablation.sh 2>&1 | tail -60
```

Expected: 
- Script completes with exit 0.
- Produces `build/test_output/ms_ablation_air/{all_air,beamline_vacuum,all_vacuum}/dispersion_summary.csv` (each with 3 truth-point rows).
- Produces `build/test_output/ms_ablation_air/compare.csv` with 9 rows (3 conditions × 3 truth points).
- Produces `build/test_output/ms_ablation_air/figures/dispersion_overlay_tp{0_0,100_0,0_20}.png` (3 files).
- No Geant4 `CheckOverlaps` errors in the sim logs.

If Geant4 reports overlaps in the `beamline_vacuum` run, stop and re-examine Tasks 2 and 3.

- [ ] **Step 3: Sanity — compare.csv has 9 rows**

```bash
wc -l build/test_output/ms_ablation_air/compare.csv
```

Expected: 10 lines (1 header + 9 rows).

- [ ] **Step 4: Sanity — Physics monotonicity on smoke data**

Read the σ_y(PDC2) column and confirm: `all_air >= beamline_vacuum >= all_vacuum` per truth point (may have large noise at N=2, but the ordering should hold qualitatively).

```bash
micromamba run -n anaroot-env python3 -c "
import pandas as pd
df = pd.read_csv('build/test_output/ms_ablation_air/compare.csv')
print(df.pivot(index=['truth_px','truth_py'], columns='condition', values='sigma_y_pdc2_mm'))
"
```

If ordering is violated at N=2, it may just be noise — not a blocker, but flag it in the report.

- [ ] **Step 5: Commit**

```bash
git add scripts/analysis/ms_ablation/run_ablation.sh
git commit -m "feat: extend run_ablation.sh to 3-way (all_air / beamline_vacuum / all_vacuum)

Wraps the new _mixed.mac in a third run_condition call. Output directory
layout changes: {baseline,no_air} → {all_air,beamline_vacuum,all_vacuum}.
compare.csv grows from 6 rows to 9 rows."
```

---

## Task 7: Full 50-event ensemble run

**Files:**
- No source changes; execution only.

- [ ] **Step 1: Clean previous outputs**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
rm -rf build/test_output/ms_ablation_air
```

- [ ] **Step 2: Run full ensemble (50 × 3 truth × 3 conditions = 450 sim+reco pairs)**

```bash
time bash scripts/analysis/ms_ablation/run_ablation.sh 2>&1 | tee build/test_output/ms_ablation_air_run.log | tail -80
```

Expected runtime: ~45 minutes to 2 hours depending on CPU and whether Geant4 build is Release. The full log is saved for debugging.

- [ ] **Step 3: Verify outputs**

```bash
ls build/test_output/ms_ablation_air/
cat build/test_output/ms_ablation_air/compare.csv
ls build/test_output/ms_ablation_air/figures/
```

Expected:
- Three subdirs `all_air`, `beamline_vacuum`, `all_vacuum`, each with `dispersion_summary.csv`, `pdc_hits.csv`, `sim/`.
- `compare.csv` with 9 data rows.
- `figures/dispersion_overlay_tp{0_0,100_0,0_20}.png`.

- [ ] **Step 4: Regression — `all_air` close to stage A baseline, `all_vacuum` close to stage A no_air**

```bash
micromamba run -n anaroot-env python3 <<'EOF'
import pandas as pd
new = pd.read_csv('build/test_output/ms_ablation_air/compare.csv')
stageA_base = pd.read_csv('docs/reports/reconstruction/ms_ablation/data/baseline_dispersion_summary.csv')
stageA_noair = pd.read_csv('docs/reports/reconstruction/ms_ablation/data/no_air_dispersion_summary.csv')

def check(new_cond, stageA_df, label):
    new_rows = new[new['condition'] == new_cond][['truth_px','truth_py','sigma_y_pdc2_mm']]
    merged = new_rows.merge(stageA_df[['truth_px','truth_py','sigma_y_pdc2_mm']],
                            on=['truth_px','truth_py'], suffixes=('_new','_stageA'))
    merged['pct_diff'] = (merged['sigma_y_pdc2_mm_new'] - merged['sigma_y_pdc2_mm_stageA']).abs() \
                        / merged['sigma_y_pdc2_mm_stageA'] * 100
    print(f'--- {label} regression (new vs stage A) ---')
    print(merged.to_string(index=False))
    ok = (merged['pct_diff'] < 5.0).all()
    print(f'All within ±5%? {ok}')
    return ok

ok_air = check('all_air',    stageA_base,  'all_air vs stage A baseline')
ok_vac = check('all_vacuum', stageA_noair, 'all_vacuum vs stage A no_air')
assert ok_air, 'all_air regression failed'
assert ok_vac, 'all_vacuum regression failed'
print('\nRegression PASS.')
EOF
```

Expected: output ends in "Regression PASS." If any row is outside ±5%, it means the geometry change (even with default-false flag) has leaked into the all_air condition, OR the build differs enough to shift numbers. Investigate before proceeding.

- [ ] **Step 5: Physics monotonicity check**

```bash
micromamba run -n anaroot-env python3 <<'EOF'
import pandas as pd
df = pd.read_csv('build/test_output/ms_ablation_air/compare.csv')
p = df.pivot(index=['truth_px','truth_py'], columns='condition', values='sigma_y_pdc2_mm')
print(p[['all_air','beamline_vacuum','all_vacuum']])
ok = (p['all_air'] >= p['beamline_vacuum']).all() and (p['beamline_vacuum'] >= p['all_vacuum']).all()
print(f'Monotonic (all_air >= beamline_vacuum >= all_vacuum)? {ok}')
assert ok
EOF
```

Expected: `Monotonic? True`.

- [ ] **Step 6: Save log for git**

```bash
cp build/test_output/ms_ablation_air_run.log docs/reports/reconstruction/ms_ablation/data/run_log_20260422.txt || true
```

(Optional; only if you want to archive. Large log files shouldn't be committed — maybe a tail is enough.)

- [ ] **Step 7: Stop and await review**

Do not commit yet. Data is snapshotted in the next task.

---

## Task 8: Snapshot data to docs + write stage A′ memo + update README + add stage A pointer

**Files:**
- Copy: `build/test_output/ms_ablation_air/*/dispersion_summary.csv` → `docs/reports/reconstruction/ms_ablation/data/`
- Copy: `build/test_output/ms_ablation_air/compare.csv` → `docs/reports/reconstruction/ms_ablation/data/`
- Copy: `build/test_output/ms_ablation_air/figures/*.png` → `docs/reports/reconstruction/ms_ablation/figures/`
- Create: `docs/reports/reconstruction/ms_ablation/ms_ablation_mixed_20260422_zh.md`
- Modify: `docs/reports/reconstruction/ms_ablation/README.md`
- Modify: `docs/reports/reconstruction/ms_ablation/ms_ablation_air_20260421_zh.md` (prepend correction notice)

- [ ] **Step 1: Snapshot data (overwrites stage A snapshot — that's fine, stage A CSVs keep their legacy filenames)**

Use Python (avoid chained cp+ls hook issues):

```bash
micromamba run -n anaroot-env python3 <<'EOF'
import shutil
from pathlib import Path
src = Path('build/test_output/ms_ablation_air')
dst_data = Path('docs/reports/reconstruction/ms_ablation/data')
dst_fig  = Path('docs/reports/reconstruction/ms_ablation/figures')
dst_data.mkdir(parents=True, exist_ok=True)
dst_fig.mkdir(parents=True, exist_ok=True)

for cond in ('all_air', 'beamline_vacuum', 'all_vacuum'):
    shutil.copy(src / cond / 'dispersion_summary.csv',
                dst_data / f'{cond}_dispersion_summary.csv')
    print(f'copied {cond}_dispersion_summary.csv')

shutil.copy(src / 'compare.csv', dst_data / 'compare.csv')
print('copied compare.csv')

for png in (src / 'figures').glob('*.png'):
    shutil.copy(png, dst_fig / png.name)
    print(f'copied {png.name}')
EOF
```

Verify:

```bash
ls docs/reports/reconstruction/ms_ablation/data/
ls docs/reports/reconstruction/ms_ablation/figures/
```

Expected: `{all_air,beamline_vacuum,all_vacuum}_dispersion_summary.csv` + `compare.csv` + 3 PNGs.

- [ ] **Step 2: Write stage A′ memo**

Create `docs/reports/reconstruction/ms_ablation/ms_ablation_mixed_20260422_zh.md`. The memo **must** include the three sections required by spec §7. Template:

```markdown
# MS 消融实验 v2 · Beam-line 真空（stage A′）— 主 memo

- **日期**: 2026-04-22
- **作者**: TBT
- **状态**: stage A′ 完成
- **Spec**: [`specs/2026-04-22-ms-ablation-mixed-design.md`](specs/2026-04-22-ms-ablation-mixed-design.md)
- **Plan**: [`plans/2026-04-22-ms-ablation-mixed-plan.md`](plans/2026-04-22-ms-ablation-mixed-plan.md)
- **数据**: [`data/compare.csv`](data/compare.csv) · [`data/all_air_dispersion_summary.csv`](data/all_air_dispersion_summary.csv) · [`data/beamline_vacuum_dispersion_summary.csv`](data/beamline_vacuum_dispersion_summary.csv) · [`data/all_vacuum_dispersion_summary.csv`](data/all_vacuum_dispersion_summary.csv)
- **图**: [`figures/`](figures/)

---

## TL;DR

Stage A 把 PDC2 σ_y 散度的 94% 归给 "air"，实际上这个 94% 是**虚假上游空气**（靶→dipole 与 dipole 内腔，~1.6 m，应为真空）**加真实下游空气**（EW→PDC2，~1.8 m）的和。新加一个正交开关 `/samurai/geometry/BeamLineVacuum` 把上游 1.6 m 真空化，得到实验物理真实的 `beamline_vacuum` baseline。

**核心数字**（填入实测）：
- Δσ_y(PDC2)_upstream_air（虚假） = ...（3 truth 点平均）
- Δσ_y(PDC2)_downstream_air（真实） = ...
- 比 = ...（上/下）

**决定**：stage D process-noise 修复的 σ_MS 输入从原先 14 mm 修正为 Δσ_y_downstream_air ≈ ...。

---

## 1. 实验配置

| 条目 | 值 |
|---|---|
| git HEAD | `...`（运行时 HEAD）|
| ENSEMBLE_SIZE | 50 |
| Seeds | SEED_A=20260421, SEED_B=20260422 |
| 真值点 | (0,0), (100,0), (0,20) MeV/c proton @ 627 MeV/c |
| 条件 | `all_air` (stage A baseline)、`beamline_vacuum` (新)、`all_vacuum` (stage A no_air) |
| Reco | `test_pdc_target_momentum_e2e --backend rk --rk-fit-mode fixed-target-pdc-only` |

Macros:
- `all_air`: `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
- `beamline_vacuum`: `..._target3deg_mixed.mac`（新）
- `all_vacuum`: `..._target3deg_noair.mac`

---

## 2. 三情形原始数据（回答 Q1）

（此处插入 3 × 3 = 9 行表，从 `data/compare.csv` 读取填入。列：truth (px,py)、N、σ_x/σ_y PDC1/PDC2、σ_θ_x/y）

---

## 3. Δσ_y 三段分解（回答 Q2）

| 分解 | 物理空气路径 | Δσ_y(PDC2) 3 点平均 | Highland(Δθ over 1 m drift) 预期 |
|---|---|---:|---:|
| upstream_air = all_air − beamline_vacuum | ~1.6 m 虚假（应为真空）| ... | ... |
| downstream_air = beamline_vacuum − all_vacuum | ~1.8 m 真实 | ... | ~2.56 mrad × 1 m = 2.56 mm |
| total_air = all_air − all_vacuum（即 stage A 数字）| ~3.4 m | ... | — |

Highland 核对：（……）

---

## 4. 修正 stage D 的噪声幅度输入（回答 Q3）

Stage A memo §4.1 给 stage D 的 σ_y,MS(PDC2) ≈ 14 mm 偏大。正确值（物理真实）= Δσ_y_downstream_air ≈ ... mm。stage D spec 应引用本 memo 这一数字。

---

## 5. 复现

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/analysis/ms_ablation/run_ablation.sh
# 产物: build/test_output/ms_ablation_air/{all_air,beamline_vacuum,all_vacuum}/ + compare.csv + figures/
```

---

## 6. 风险与澄清

（按 spec §9 填）

---

## 7. 关联

- Spec: [`specs/2026-04-22-ms-ablation-mixed-design.md`](specs/2026-04-22-ms-ablation-mixed-design.md)
- Plan: [`plans/2026-04-22-ms-ablation-mixed-plan.md`](plans/2026-04-22-ms-ablation-mixed-plan.md)
- 上游 stage A memo: [`ms_ablation_air_20260421_zh.md`](ms_ablation_air_20260421_zh.md)
- RK 报告: `docs/reports/reconstruction/rk_reconstruction_status_20260416_zh.pdf` §7.5
- 下游: stage D（RK process-noise）独立 spec 待起草
```

Fill in the placeholder values from the actual data. Use the CSVs in `data/`.

- [ ] **Step 3: Prepend correction notice to stage A memo**

Edit `docs/reports/reconstruction/ms_ablation/ms_ablation_air_20260421_zh.md` by adding at the very top (before the existing `# MS 消融实验 v1 · Air 关闭 — 主 memo` title):

```markdown
> **⚠ 修正说明（2026-04-22）**: 本 memo 的 baseline 几何**不完全物理**——靶→dipole 入射面（~0.68 m）与 dipole yoke 内腔（~0.95 m）在 Geant4 里误为空气（应为束流真空）。因此本 memo "air 贡献 94%" 的数字混合了约 1.6 m 虚假上游空气 + 1.8 m 真实下游空气。物理真实的 baseline（只真实下游空气）与完整分解见 [`ms_ablation_mixed_20260422_zh.md`](ms_ablation_mixed_20260422_zh.md)（stage A′）。本 memo 保留作为历史记录与代码 bug 的直接证据，**不再作为 stage D 的输入**。

---

```

- [ ] **Step 4: Update README index**

In `docs/reports/reconstruction/ms_ablation/README.md`, update the 迭代索引 table. Change the stage-A row status annotation and add a stage A′ row. Replace the existing table (around lines 9–14 of the current README) with:

```markdown
| 阶段 | 切换 | 状态 | 文档 |
|---|---|---|---|
| A | 世界空气 → 真空 | ✅ 2026-04-21 完成（几何不物理，见 A′）| [`ms_ablation_air_20260421_zh.md`](ms_ablation_air_20260421_zh.md) |
| A′ | + 靶→dipole/yoke 内腔真空（实验真实 baseline） | ✅ 2026-04-22 完成 | [`ms_ablation_mixed_20260422_zh.md`](ms_ablation_mixed_20260422_zh.md) |
| B | + exit window → 真空 | 规划中 | — |
| C | + PDC 气体 MS 消除 | 规划中 | — |
| D | RK process-noise 修复 | 规划中（输入来自 A′）| — |
```

Also update the bottom "快速复现" block if it references `{baseline,no_air}` to reflect the new `{all_air,beamline_vacuum,all_vacuum}` layout, and add a line: "阶段 A′ 关键结果：下游真实空气（~1.8 m）贡献 PDC2 σ_y 约 ... mm（实测），stage D 的 σ_MS 输入从 stage A 的 14 mm 修正为该值。"

- [ ] **Step 5: Commit**

```bash
git add docs/reports/reconstruction/ms_ablation/data/ \
        docs/reports/reconstruction/ms_ablation/figures/ \
        docs/reports/reconstruction/ms_ablation/ms_ablation_mixed_20260422_zh.md \
        docs/reports/reconstruction/ms_ablation/ms_ablation_air_20260421_zh.md \
        docs/reports/reconstruction/ms_ablation/README.md
# Some of the data/ files may be gitignored — force if needed:
git add -f docs/reports/reconstruction/ms_ablation/data/*.csv
git commit -m "docs: MS ablation stage A' memo + 3-way data snapshot + README

Stage A' with physical baseline (beamline vacuum on). Reports
upstream_air / downstream_air / total_air decomposition. Corrects
stage A's 14 mm stage-D input to the downstream-only value.

Prepends a correction notice on stage A memo pointing to A'."
```

---

## Task 9: Update memory (stage A′ finding)

**Files:**
- Modify: `/home/tian/.claude/projects/-home-tian-workspace-dpol-smsimulator5-5/memory/project_ms_ablation_air_finding.md`
- Modify: `/home/tian/.claude/projects/-home-tian-workspace-dpol-smsimulator5-5/memory/MEMORY.md`

- [ ] **Step 1: Add a stage A′ memory file**

Create `/home/tian/.claude/projects/-home-tian-workspace-dpol-smsimulator5-5/memory/project_ms_ablation_mixed_finding.md`:

```markdown
---
name: MS ablation stage A' finding (2026-04-22)
description: stage A air=94% is 1.6 m fake upstream + 1.8 m real downstream; stage D σ_MS input corrected to downstream-only value
type: project
---
Stage A′ (design: `docs/reports/reconstruction/ms_ablation/specs/2026-04-22-ms-ablation-mixed-design.md`) complete.

**Key finding:** Stage A's baseline geometry was not fully physical — the target→dipole region (~0.68 m) and dipole yoke interior (~0.95 m) inherited world material (air) instead of beam-line vacuum. New `/samurai/geometry/BeamLineVacuum` flag fixes this; default false preserves all pre-change behavior.

**Three-way decomposition (3 truth points × N=50, 627 MeV/c proton):**
- Δσ_y(PDC2)_upstream_air (fake, ~1.6 m)   ≈ ...  (fill after running)
- Δσ_y(PDC2)_downstream_air (real, ~1.8 m) ≈ ...
- Δσ_y(PDC2)_total_air (stage A's 94%)     ≈ ...

**Why:** Stage A's σ_MS input of 14 mm for stage D was inflated by the fake upstream air. Correct input is downstream_air only.

**How to apply:**
- Stage D RK process-noise calibration **must** use Δσ_y_downstream_air from stage A′ memo, not stage A's 14 mm.
- The `BeamLineVacuum` flag is orthogonal to `FillAir`; future simulations where geometric realism matters should set `BeamLineVacuum=true` whenever `FillAir=true`.
- Reproduce: `bash scripts/analysis/ms_ablation/run_ablation.sh`. Produces 3 conditions × 3 truth × 50 seeds.
- Code changes: `VacuumUpstreamConstruction` (was unwired) now instantiated conditionally; `DipoleConstruction::ConstructSub` adds an explicit `vchamber_cavity` logical volume whose material is G4_Galactic under the flag, world material otherwise.

Main memo: `docs/reports/reconstruction/ms_ablation/ms_ablation_mixed_20260422_zh.md`.
```

- [ ] **Step 2: Update MEMORY.md index**

Edit `/home/tian/.claude/projects/-home-tian-workspace-dpol-smsimulator5-5/memory/MEMORY.md` to add one line (keep under 150 chars):

```markdown
- [MS ablation stage A' finding 2026-04-22](project_ms_ablation_mixed_finding.md) — 94% air is ~1.6 m fake upstream + ~1.8 m real; stage D σ_MS input corrected
```

Fill the actual measured values into the memory file once the memo has them.

- [ ] **Step 3: Verify memory is saved (no git tracking needed — memory dir is outside repo)**

```bash
ls -la /home/tian/.claude/projects/-home-tian-workspace-dpol-smsimulator5-5/memory/project_ms_ablation_mixed_finding.md
```

---

## Self-Review Checklist (controller runs this before dispatch)

- [ ] Every task has a specific set of files and exact file paths.
- [ ] Each step has concrete code/commands (no "TBD", no "implement error handling").
- [ ] Method/function names are consistent across tasks (`SetBeamLineVacuum`, `IsBeamLineVacuum`, `fBeamLineVacuum`, `fVacuumUpstreamConstruction`, `vchamber_cavity`, `build_comparison`).
- [ ] Every spec requirement has a task:
  - §5.1 C++ changes (Messenger, DeutDetectorConstruction, Dipole, VacuumUpstream wiring) → Tasks 1–3 ✓
  - §5.1 default-false bit-for-bit guarantee → Task 1 Step 8, Task 2 Step 7, Task 3 Step 4 ✓
  - §6.1 `_target3deg_mixed.mac` + smoke test → Task 4 ✓
  - §6.2 run_ablation.sh 3-way + compare_ablation.py 3-way + test_compare_ablation.py → Tasks 5, 6 ✓
  - §6.3 stage A' memo + data snapshot + README + stage A pointer → Task 8 ✓
  - §7 memo three sections (σ table, Δ decomposition, stage D correction) → Task 8 Step 2 ✓
  - §8 success criteria → covered by Tasks 1 Step 8 (regression), 7 Step 4 (regression), 7 Step 5 (monotonicity), 8 (memo/README) ✓
- [ ] Frequent commits: each major task ends in a commit.
- [ ] Tests precede implementation where applicable (Tasks 4, 5 use TDD; Tasks 1–3 use CTest regression since Geant4 geometry doesn't unit-test well).
