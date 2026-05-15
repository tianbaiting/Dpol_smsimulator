# NEBULA-Plus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the NEBULA-Plus Geant4 sub-detector to smsimulator5.5 with full SimData chain, then refactor the existing `NEBULAReco` into a base/derived hierarchy and add the joint NEBULA + NEBULA-Plus neutron reconstruction.

**Architecture:** New `NEBULAPlus*` module mirrors the existing `NEBULA*` files under `libs/smg4lib/`, produces independent SimData branches `fNEBULAPlusSimData` / `fNEBULAPlusSimParameter`. In `libs/analysis/`, `NEBULAReco` is split into `NEBULABaseReco` (abstract) + `NEBULAReco` (NEBULA-only), and a sibling `NEBULAPlusReco` plus a `NebulaJointReco` are added. The plan is structured in three phases, each ending at a clean commit.

**Tech Stack:** C++17, Geant4 (CMake `find_package`), ROOT (`ROOT_GENERATE_DICTIONARY`), GoogleTest (`gtest_discover_tests`). Source layout: `libs/smg4lib/{src/construction,src/data,include}` for G4 / SimData, `libs/analysis/{include,src}` for reconstruction, `tests/{unit,analysis,integration}` for tests.

**Specification:** `docs/superpowers/specs/2026-05-15-nebula-plus-design.md` (commit `99dfaf2`).

**Authoritative numbers (read once, keep in mind throughout):**
- NEBULA-Plus origin in target frame: **Z = 8089 mm** (Wall-A Sub1 centre)
- 4 sub-layers (Wall-A Sub1/Sub2, Wall-B Sub1/Sub2)
- Bar counts per sub-layer: **22, 22, 23, 23** (total 90 Neut)
- Veto per wall: **12** (total 24)
- Bar pitch X = **120 mm**, leftmost bar centre at **X = −1320 mm**
- Sub-layer Z offsets within NEBULA-Plus frame: **0, 130, 845, 975 mm**
- Veto Z offsets: **−100** (Wall-A), **+745** (Wall-B)
- Veto pitch X = 310 mm, leftmost at **+1705 mm** (symmetric span)
- Bar size 120 × 1800 × 120 mm; Veto 320 × 1900 × 10 mm
- Material `G4_PLASTIC_SC_VINYLTOLUENE`; TimeReso 0.240 ns

---

## Phase 0 — Pre-flight

### Task 0.1: Environment sanity check

**Files:**
- Read: `setup.sh`, `CMakeLists.txt`

- [ ] **Step 1: Activate environment**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
source setup.sh
echo "Geant4 version:"; which geant4-config && geant4-config --version
echo "ROOT version:"; which root-config && root-config --version
echo "TARTSYS=${TARTSYS:-NOT SET}"
```

Expected: all three lines print versions / paths.

- [ ] **Step 2: Verify clean build works before any changes**

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_APPS=ON -DBUILD_ANALYSIS=ON ..
make -j$(nproc) 2>&1 | tee /tmp/baseline_build.log | tail -20
```

Expected: build succeeds. Note: `libnebula_reconstructor` related artefacts in `lib/` and the `analysis` shared lib should exist. If build fails before changes, stop and fix that first.

- [ ] **Step 3: Verify existing test suite passes**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
ctest --output-on-failure -L unit -L analysis 2>&1 | tail -30
```

Expected: All currently-defined tests pass. Note the count for later comparison.

---

## Phase 1 — Geometry + SimData chain

Goal: NEBULA-Plus G4 module compiles, `sim_deuteron` produces an output tree with `fNEBULAPlusSimData` and `fNEBULAPlusSimParameter`, vis macro shows the bars. No reco changes.

### Task 1.1: Add the geometry CSV files

**Files:**
- Create: `configs/simulation/geometry/s021/NEBULAPlus_samurai21.csv`
- Create: `configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv`

- [ ] **Step 1: Write the overall-parameter CSV**

Create `configs/simulation/geometry/s021/NEBULAPlus_samurai21.csv` with exactly:

```
Position, 0, 0, 8089,
NeutSize, 120, 1800, 120,
VetoSize, 320, 1900, 10,
Angle, 0, 0, 0,
////input time reso for one PMT
TimeReso, 0.240,
```

- [ ] **Step 2: Write the per-bar CSV header and Wall-A Sub1 rows**

Create `configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv`. Begin with:

```
ID,DetectorType,Layer,SubLayer,PositionX,PositionY,PositionZ,AngleX,AngleY,AngleZ
```

Then append 22 Wall-A Sub1 rows. For `i` in `0..21`, write a row with `ID=101+i, DetectorType=Neut, Layer=1, SubLayer=1, PositionX=-1320+i*120, PositionY=0, PositionZ=0, AngleX=0, AngleY=0, AngleZ=0`.

You can generate the rows quickly with:

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python3 - <<'EOF' >> configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv
for i in range(22):
    x = -1320 + i*120
    print(f"{101+i},Neut,1,1,{x},0,0,0,0,0")
EOF
```

- [ ] **Step 3: Append Wall-A Sub2 rows (22 bars at Z=130)**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python3 - <<'EOF' >> configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv
for i in range(22):
    x = -1320 + i*120
    print(f"{201+i},Neut,1,2,{x},0,130,0,0,0")
EOF
```

- [ ] **Step 4: Append Wall-B Sub1 + Sub2 rows (23 bars each at Z=845 / 975)**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python3 - <<'EOF' >> configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv
for i in range(23):
    x = -1320 + i*120
    print(f"{301+i},Neut,2,1,{x},0,845,0,0,0")
for i in range(23):
    x = -1320 + i*120
    print(f"{401+i},Neut,2,2,{x},0,975,0,0,0")
EOF
```

- [ ] **Step 5: Append the 24 Veto rows**

Veto: 12 per wall, pitch 310 mm, leftmost at +1705 (decreasing). Wall-A Veto at Z=−100, Wall-B Veto at Z=+745.

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python3 - <<'EOF' >> configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv
for i in range(12):
    x = 1705 - i*310
    print(f"{501+i},Veto,1,0,{x},0,-100,0,0,0")
for i in range(12):
    x = 1705 - i*310
    print(f"{601+i},Veto,2,0,{x},0,745,0,0,0")
EOF
```

- [ ] **Step 6: Sanity-check the CSV contents**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
wc -l configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv
awk -F',' 'NR>1 && $2=="Neut" {n++} NR>1 && $2=="Veto" {v++} END {print "Neut:",n," Veto:",v}' \
    configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv
```

Expected:
- `115 configs/...` (1 header + 114 rows)
- `Neut: 90  Veto: 24`

- [ ] **Step 7: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add configs/simulation/geometry/s021/NEBULAPlus_samurai21.csv \
        configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv
git commit -m "feat(geom): add NEBULA-Plus s021 geometry CSV (90 Neut + 24 Veto)"
```

---

### Task 1.2: Add the TArtNEBULAPlusPla data class

**Files:**
- Create: `libs/smg4lib/include/TArtNEBULAPlusPla.hh`
- Create: `libs/smg4lib/src/data/include/TArtNEBULAPlusPla.hh` (mirror)
- Create: `libs/smg4lib/src/data/src/TArtNEBULAPlusPla.cc`
- Modify: `libs/smg4lib/src/data/smdata_linkdef.hh`

- [ ] **Step 1: Write the header `libs/smg4lib/include/TArtNEBULAPlusPla.hh`**

```cpp
#ifndef TArtNEBULAPlusPla_HH
#define TArtNEBULAPlusPla_HH

#include "TObject.h"
#include "TVector3.h"

// Per-bar hit data for NEBULA-Plus, written to ROOT tree as TClonesArray<TArtNEBULAPlusPla>.
// Field names mirror the anaroot TArtNEBULAPla pattern so user analysis code can be cross-ported,
// but the class is independent (not derived from any anaroot type).
class TArtNEBULAPlusPla : public TObject {
public:
  TArtNEBULAPlusPla();
  virtual ~TArtNEBULAPlusPla();

  Int_t    fID;          // 101..122, 201..222 (Wall-A Neut Sub1/2);
                         // 301..323, 401..423 (Wall-B Neut Sub1/2);
                         // 501..512 (Wall-A Veto); 601..612 (Wall-B Veto)
  Int_t    fLayer;       // 1 = Wall-A, 2 = Wall-B
  Int_t    fSubLayer;    // 0 = Veto, 1 = front sub, 2 = back sub
  Double_t fTime;        // ns, mean of fTU and fTD
  Double_t fEdep;        // MeV, total energy deposit per event in this bar
  Double_t fQU;          // upper PMT integrated charge (arbitrary units)
  Double_t fQD;          // lower PMT integrated charge
  Double_t fTU;          // upper PMT time (ns)
  Double_t fTD;          // lower PMT time (ns)
  TVector3 fPos;         // reconstructed hit position (mm, target frame)
  Bool_t   fIsVeto;

  void Clear(Option_t* opt = "") override;

  ClassDef(TArtNEBULAPlusPla, 1);
};

#endif
```

- [ ] **Step 2: Mirror the same header to the smdata include dir**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
cp libs/smg4lib/include/TArtNEBULAPlusPla.hh \
   libs/smg4lib/src/data/include/TArtNEBULAPlusPla.hh
```

- [ ] **Step 3: Write the .cc file**

Create `libs/smg4lib/src/data/src/TArtNEBULAPlusPla.cc`:

```cpp
#include "TArtNEBULAPlusPla.hh"

ClassImp(TArtNEBULAPlusPla)

TArtNEBULAPlusPla::TArtNEBULAPlusPla()
    : TObject(),
      fID(-1), fLayer(-1), fSubLayer(-1),
      fTime(0), fEdep(0),
      fQU(0), fQD(0), fTU(0), fTD(0),
      fPos(0, 0, 0),
      fIsVeto(false)
{}

TArtNEBULAPlusPla::~TArtNEBULAPlusPla() {}

void TArtNEBULAPlusPla::Clear(Option_t*)
{
  fID = -1; fLayer = -1; fSubLayer = -1;
  fTime = 0; fEdep = 0;
  fQU = 0; fQD = 0; fTU = 0; fTD = 0;
  fPos.SetXYZ(0, 0, 0);
  fIsVeto = false;
}
```

- [ ] **Step 4: Register in smdata_linkdef.hh**

Modify `libs/smg4lib/src/data/smdata_linkdef.hh`. Locate the line `#pragma link C++ class TNEBULASimParameter+;` and insert AFTER it:

```cpp
#pragma link C++ class TNEBULAPlusSimParameter+;
#pragma link C++ class TArtNEBULAPlusPla+;
```

(`TNEBULAPlusSimParameter` will be created in Task 1.3 — registering it here in advance is fine; ROOT dict generation runs on the linkdef.)

- [ ] **Step 5: Build, verify the new data class compiles into smdata**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target smdata -j$(nproc) 2>&1 | tail -10
```

Expected: build succeeds. The `TArtNEBULAPlusPla` symbol is present:

```bash
nm -DC lib/libsmdata.so 2>/dev/null | grep -c TArtNEBULAPlusPla
```

Expected: a count > 0.

- [ ] **Step 6: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/smg4lib/include/TArtNEBULAPlusPla.hh \
        libs/smg4lib/src/data/include/TArtNEBULAPlusPla.hh \
        libs/smg4lib/src/data/src/TArtNEBULAPlusPla.cc \
        libs/smg4lib/src/data/smdata_linkdef.hh
git commit -m "feat(data): add TArtNEBULAPlusPla ROOT data class"
```

---

### Task 1.3: Add TNEBULAPlusSimParameter

**Files:**
- Create: `libs/smg4lib/include/TNEBULAPlusSimParameter.hh`
- Create: `libs/smg4lib/src/data/include/TNEBULAPlusSimParameter.hh` (mirror)
- Create: `libs/smg4lib/src/data/src/TNEBULAPlusSimParameter.cc`

- [ ] **Step 1: Inspect the existing NEBULA equivalent so we mirror it**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
cat libs/smg4lib/include/TNEBULASimParameter.hh
```

Identify the public fields and methods.

- [ ] **Step 2: Create the new header as a copy**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULASimParameter/NEBULAPlusSimParameter/g' \
    -e 's/TNEBULASim/TNEBULAPlusSim/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/fNEBULADetectorParameterMap/fNEBULAPlusDetectorParameterMap/g' \
    libs/smg4lib/include/TNEBULASimParameter.hh \
    > libs/smg4lib/include/TNEBULAPlusSimParameter.hh
cp libs/smg4lib/include/TNEBULAPlusSimParameter.hh \
   libs/smg4lib/src/data/include/TNEBULAPlusSimParameter.hh
```

- [ ] **Step 3: Create the .cc the same way**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULASimParameter/NEBULAPlusSimParameter/g' \
    -e 's/TNEBULASim/TNEBULAPlusSim/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/fNEBULADetectorParameterMap/fNEBULAPlusDetectorParameterMap/g' \
    libs/smg4lib/src/data/src/TNEBULASimParameter.cc \
    > libs/smg4lib/src/data/src/TNEBULAPlusSimParameter.cc
```

- [ ] **Step 4: Diff-check the generated files**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
diff libs/smg4lib/include/TNEBULASimParameter.hh libs/smg4lib/include/TNEBULAPlusSimParameter.hh
```

Expected: only the substitutions above appear in the diff. If any unrelated string changed, manually fix.

- [ ] **Step 5: Build smdata and verify dict is regenerated**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target smdata -j$(nproc) 2>&1 | tail -10
nm -DC lib/libsmdata.so | grep -c TNEBULAPlusSimParameter
```

Expected: build succeeds; symbol count > 0.

- [ ] **Step 6: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/smg4lib/include/TNEBULAPlusSimParameter.hh \
        libs/smg4lib/src/data/include/TNEBULAPlusSimParameter.hh \
        libs/smg4lib/src/data/src/TNEBULAPlusSimParameter.cc
git commit -m "feat(data): add TNEBULAPlusSimParameter mirroring NEBULA"
```

---

### Task 1.4: Write the failing test for `NEBULAPlusSimParameterReader`

**Files:**
- Create: `tests/unit/test_NEBULAPlusSimParameterReader.cc`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/unit/test_NEBULAPlusSimParameterReader.cc
#include <gtest/gtest.h>
#include "TNEBULAPlusSimParameter.hh"
#include "NEBULAPlusSimParameterReader.hh"
#include "SimDataManager.hh"

namespace {

const char* kParamCsv = "configs/simulation/geometry/s021/NEBULAPlus_samurai21.csv";
const char* kBarsCsv  = "configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv";

class NEBULAPlusReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        SimDataManager::GetSimDataManager()->Clear();
        fParam = new TNEBULAPlusSimParameter("NEBULAPlusParameter");
        SimDataManager::GetSimDataManager()->AddParameter(fParam);
    }
    TNEBULAPlusSimParameter* fParam = nullptr;
};

TEST_F(NEBULAPlusReaderTest, ReadsOverallParameters) {
    NEBULAPlusSimParameterReader reader(kParamCsv);
    ASSERT_EQ(0, reader.Read());
    EXPECT_DOUBLE_EQ(8089.0, fParam->fPosition.z());
    EXPECT_DOUBLE_EQ(120.0,  fParam->fNeutSize.x());
    EXPECT_DOUBLE_EQ(1800.0, fParam->fNeutSize.y());
    EXPECT_DOUBLE_EQ(120.0,  fParam->fNeutSize.z());
    EXPECT_DOUBLE_EQ(320.0,  fParam->fVetoSize.x());
    EXPECT_DOUBLE_EQ(10.0,   fParam->fVetoSize.z());
    EXPECT_DOUBLE_EQ(0.240,  fParam->fTimeReso);
}

TEST_F(NEBULAPlusReaderTest, ReadsBarTable) {
    NEBULAPlusSimParameterReader reader(kParamCsv);
    ASSERT_EQ(0, reader.Read());
    NEBULAPlusSimParameterReader bars(kBarsCsv);
    ASSERT_EQ(0, bars.ReadDetectors());

    int n_neut = 0, n_veto = 0;
    for (const auto& kv : fParam->fNEBULAPlusDetectorParameterMap) {
        if (kv.second.fDetectorType == "Neut") ++n_neut;
        if (kv.second.fDetectorType == "Veto") ++n_veto;
    }
    EXPECT_EQ(90, n_neut);
    EXPECT_EQ(24, n_veto);
}

TEST_F(NEBULAPlusReaderTest, WallASubXLayoutIsAsymmetric) {
    NEBULAPlusSimParameterReader reader(kParamCsv);
    NEBULAPlusSimParameterReader bars(kBarsCsv);
    reader.Read();
    bars.ReadDetectors();

    // Wall-A Sub1: 22 bars, X from -1320 to +1200 stepping 120
    double xmin = 1e9, xmax = -1e9;
    int count = 0;
    for (const auto& kv : fParam->fNEBULAPlusDetectorParameterMap) {
        const auto& p = kv.second;
        if (p.fLayer == 1 && p.fSubLayer == 1 && p.fDetectorType == "Neut") {
            xmin = std::min(xmin, p.fPosition.x());
            xmax = std::max(xmax, p.fPosition.x());
            ++count;
        }
    }
    EXPECT_EQ(22, count);
    EXPECT_DOUBLE_EQ(-1320.0, xmin);
    EXPECT_DOUBLE_EQ(+1200.0, xmax);
}

}  // namespace
```

- [ ] **Step 2: Wire the test into CMake**

Open `tests/unit/CMakeLists.txt`. After the existing `add_executable(test_DeutDetectorConstruction_defaults ...)` block, append (use the same library set as the existing block uses):

```cmake
add_executable(test_NEBULAPlusSimParameterReader
    test_NEBULAPlusSimParameterReader.cc
)
target_link_libraries(test_NEBULAPlusSimParameterReader PRIVATE
    sim_deuteron_core
    smg4lib
    GTest::gtest
    GTest::gtest_main
    ${Geant4_LIBRARIES}
    ${ROOT_LIBRARIES}
)
gtest_discover_tests(test_NEBULAPlusSimParameterReader
    PROPERTIES LABELS "unit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

The `WORKING_DIRECTORY` is essential because the test loads CSV via project-relative paths.

- [ ] **Step 3: Build and verify it FAILS to compile (the reader doesn't exist yet)**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake .. 2>&1 | tail -5
cmake --build . --target test_NEBULAPlusSimParameterReader -j$(nproc) 2>&1 | tail -20
```

Expected: compilation FAILS with errors like `'NEBULAPlusSimParameterReader': undeclared identifier` / cannot open `NEBULAPlusSimParameterReader.hh`. This is the RED in TDD.

- [ ] **Step 4: Commit the failing test (test-first)**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add tests/unit/test_NEBULAPlusSimParameterReader.cc tests/unit/CMakeLists.txt
git commit -m "test(unit): add failing test for NEBULAPlusSimParameterReader"
```

---

### Task 1.5: Implement `NEBULAPlusSimParameterReader`

**Files:**
- Create: `libs/smg4lib/include/NEBULAPlusSimParameterReader.hh`
- Create: `libs/smg4lib/src/data/include/NEBULAPlusSimParameterReader.hh` (mirror)
- Create: `libs/smg4lib/src/data/src/NEBULAPlusSimParameterReader.cc`

- [ ] **Step 1: Generate header from NEBULA equivalent via sed**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULASimParameterReader/NEBULAPlusSimParameterReader/g' \
    -e 's/TNEBULASimParameter/TNEBULAPlusSimParameter/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/fNEBULADetectorParameterMap/fNEBULAPlusDetectorParameterMap/g' \
    libs/smg4lib/include/NEBULASimParameterReader.hh \
    > libs/smg4lib/include/NEBULAPlusSimParameterReader.hh
cp libs/smg4lib/include/NEBULAPlusSimParameterReader.hh \
   libs/smg4lib/src/data/include/NEBULAPlusSimParameterReader.hh
```

- [ ] **Step 2: Generate .cc the same way**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULASimParameterReader/NEBULAPlusSimParameterReader/g' \
    -e 's/TNEBULASimParameter/TNEBULAPlusSimParameter/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/fNEBULADetectorParameterMap/fNEBULAPlusDetectorParameterMap/g' \
    libs/smg4lib/src/data/src/NEBULASimParameterReader.cc \
    > libs/smg4lib/src/data/src/NEBULAPlusSimParameterReader.cc
```

- [ ] **Step 3: Build and run the test**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target test_NEBULAPlusSimParameterReader -j$(nproc) 2>&1 | tail -10
ctest -R NEBULAPlusSimParameterReader -V 2>&1 | tail -40
```

Expected: build succeeds and all three test cases pass.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/smg4lib/include/NEBULAPlusSimParameterReader.hh \
        libs/smg4lib/src/data/include/NEBULAPlusSimParameterReader.hh \
        libs/smg4lib/src/data/src/NEBULAPlusSimParameterReader.cc
git commit -m "feat(data): NEBULAPlusSimParameterReader loads NPL CSVs"
```

---

### Task 1.6: Clone `NEBULAPlusSimDataInitializer`

**Files:**
- Create: `libs/smg4lib/include/NEBULAPlusSimDataInitializer.hh`
- Create: `libs/smg4lib/src/data/include/NEBULAPlusSimDataInitializer.hh` (mirror)
- Create: `libs/smg4lib/src/data/src/NEBULAPlusSimDataInitializer.cc`

- [ ] **Step 1: Read the existing NEBULA Initializer to know what it does**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
cat libs/smg4lib/src/data/src/NEBULASimDataInitializer.cc
```

Identify the branch names registered (`fNEBULASimData`, `fNEBULASimParameter`) and how `TArtNEBULAPla` is used.

- [ ] **Step 2: Generate the header**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULASimDataInitializer/NEBULAPlusSimDataInitializer/g' \
    -e 's/TNEBULASim/TNEBULAPlusSim/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    libs/smg4lib/include/NEBULASimDataInitializer.hh \
    > libs/smg4lib/include/NEBULAPlusSimDataInitializer.hh
cp libs/smg4lib/include/NEBULAPlusSimDataInitializer.hh \
   libs/smg4lib/src/data/include/NEBULAPlusSimDataInitializer.hh
```

- [ ] **Step 3: Generate the .cc, with the branch-name substitutions**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULASimDataInitializer/NEBULAPlusSimDataInitializer/g' \
    -e 's/TNEBULASimParameter/TNEBULAPlusSimParameter/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/fNEBULASimData/fNEBULAPlusSimData/g' \
    -e 's/"NEBULA"/"NEBULAPlus"/g' \
    -e 's/TArtNEBULAPla/TArtNEBULAPlusPla/g' \
    libs/smg4lib/src/data/src/NEBULASimDataInitializer.cc \
    > libs/smg4lib/src/data/src/NEBULAPlusSimDataInitializer.cc
```

- [ ] **Step 4: Inspect for any leftover NEBULA-only references**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
grep -nE "NEBULA[^P]" libs/smg4lib/src/data/src/NEBULAPlusSimDataInitializer.cc | grep -v Plus
```

Expected: no output. If there's a residual `NEBULA` that doesn't refer to NEBULA-Plus (e.g., an old comment), fix manually.

- [ ] **Step 5: Build smdata**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target smdata -j$(nproc) 2>&1 | tail -10
```

Expected: build succeeds.

- [ ] **Step 6: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/smg4lib/include/NEBULAPlusSimDataInitializer.hh \
        libs/smg4lib/src/data/include/NEBULAPlusSimDataInitializer.hh \
        libs/smg4lib/src/data/src/NEBULAPlusSimDataInitializer.cc
git commit -m "feat(data): clone NEBULASimDataInitializer for NEBULA-Plus"
```

---

### Task 1.7: Add NEBULAPlusSD

**Files:**
- Create: `libs/smg4lib/include/NEBULAPlusSD.hh`
- Create: `libs/smg4lib/src/construction/include/NEBULAPlusSD.hh` (mirror)
- Create: `libs/smg4lib/src/construction/src/NEBULAPlusSD.cc`

- [ ] **Step 1: Generate via sed**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULASD/NEBULAPlusSD/g' \
    -e 's/fNEBULASimData/fNEBULAPlusSimData/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/TNEBULASimParameter/TNEBULAPlusSimParameter/g' \
    libs/smg4lib/include/NEBULASD.hh \
    > libs/smg4lib/include/NEBULAPlusSD.hh
cp libs/smg4lib/include/NEBULAPlusSD.hh \
   libs/smg4lib/src/construction/include/NEBULAPlusSD.hh

sed -e 's/NEBULASD/NEBULAPlusSD/g' \
    -e 's/fNEBULASimData/fNEBULAPlusSimData/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/TNEBULASimParameter/TNEBULAPlusSimParameter/g' \
    libs/smg4lib/src/construction/src/NEBULASD.cc \
    > libs/smg4lib/src/construction/src/NEBULAPlusSD.cc
```

- [ ] **Step 2: Grep for stray NEBULA names**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
grep -nE "NEBULA[^P]" libs/smg4lib/src/construction/src/NEBULAPlusSD.cc | grep -v Plus
```

Expected: no output.

- [ ] **Step 3: Build construction lib**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target smconstruction -j$(nproc) 2>&1 | tail -10
```

Expected: success.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/smg4lib/include/NEBULAPlusSD.hh \
        libs/smg4lib/src/construction/include/NEBULAPlusSD.hh \
        libs/smg4lib/src/construction/src/NEBULAPlusSD.cc
git commit -m "feat(g4): clone NEBULASD as NEBULAPlusSD"
```

---

### Task 1.8: Add NEBULAPlusConstructionMessenger

**Files:**
- Create: `libs/smg4lib/include/NEBULAPlusConstructionMessenger.hh`
- Create: `libs/smg4lib/src/construction/include/NEBULAPlusConstructionMessenger.hh` (mirror)
- Create: `libs/smg4lib/src/construction/src/NEBULAPlusConstructionMessenger.cc`

- [ ] **Step 1: Generate via sed**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULAConstructionMessenger/NEBULAPlusConstructionMessenger/g' \
    -e 's/NEBULAConstruction/NEBULAPlusConstruction/g' \
    -e 's|/samurai/geometry/NEBULA|/samurai/geometry/NEBULAPlus|g' \
    libs/smg4lib/include/NEBULAConstructionMessenger.hh \
    > libs/smg4lib/include/NEBULAPlusConstructionMessenger.hh
cp libs/smg4lib/include/NEBULAPlusConstructionMessenger.hh \
   libs/smg4lib/src/construction/include/NEBULAPlusConstructionMessenger.hh

sed -e 's/NEBULAConstructionMessenger/NEBULAPlusConstructionMessenger/g' \
    -e 's/NEBULAConstruction/NEBULAPlusConstruction/g' \
    -e 's|/samurai/geometry/NEBULA|/samurai/geometry/NEBULAPlus|g' \
    libs/smg4lib/src/construction/src/NEBULAConstructionMessenger.cc \
    > libs/smg4lib/src/construction/src/NEBULAPlusConstructionMessenger.cc
```

- [ ] **Step 2: Grep guard**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
grep -nE "/samurai/geometry/NEBULA[^P]|/samurai/geometry/NEBULA$" \
    libs/smg4lib/src/construction/src/NEBULAPlusConstructionMessenger.cc
```

Expected: no output.

- [ ] **Step 3: Build will fail (NEBULAPlusConstruction class not yet defined) — that is expected; only the headers need to parse for now**

Don't build this in isolation; it will be linked when Task 1.9 lands the Construction class.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/smg4lib/include/NEBULAPlusConstructionMessenger.hh \
        libs/smg4lib/src/construction/include/NEBULAPlusConstructionMessenger.hh \
        libs/smg4lib/src/construction/src/NEBULAPlusConstructionMessenger.cc
git commit -m "feat(g4): clone NEBULAConstructionMessenger for NPL"
```

---

### Task 1.9: Add NEBULAPlusConstruction

**Files:**
- Create: `libs/smg4lib/include/NEBULAPlusConstruction.hh`
- Create: `libs/smg4lib/src/construction/include/NEBULAPlusConstruction.hh` (mirror)
- Create: `libs/smg4lib/src/construction/src/NEBULAPlusConstruction.cc`

- [ ] **Step 1: Generate via sed**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULAConstruction/NEBULAPlusConstruction/g' \
    -e 's/NEBULAConstructionMessenger/NEBULAPlusConstructionMessenger/g' \
    -e 's/NEBULASD/NEBULAPlusSD/g' \
    -e 's/TNEBULASimParameter/TNEBULAPlusSimParameter/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/fNEBULAConstruction/fNEBULAPlusConstruction/g' \
    -e 's/PutNEBULA/PutNEBULAPlus/g' \
    -e 's/SetNEBULASD/SetNEBULAPlusSD/g' \
    -e 's/fNEBULASD/fNEBULAPlusSD/g' \
    -e 's/Creating NEBULA/Creating NEBULAPlus/g' \
    libs/smg4lib/include/NEBULAConstruction.hh \
    > libs/smg4lib/include/NEBULAPlusConstruction.hh
cp libs/smg4lib/include/NEBULAPlusConstruction.hh \
   libs/smg4lib/src/construction/include/NEBULAPlusConstruction.hh

sed -e 's/NEBULAConstruction/NEBULAPlusConstruction/g' \
    -e 's/NEBULAConstructionMessenger/NEBULAPlusConstructionMessenger/g' \
    -e 's/NEBULASD/NEBULAPlusSD/g' \
    -e 's/TNEBULASimParameter/TNEBULAPlusSimParameter/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/fNEBULAConstruction/fNEBULAPlusConstruction/g' \
    -e 's/PutNEBULA/PutNEBULAPlus/g' \
    -e 's/SetNEBULASD/SetNEBULAPlusSD/g' \
    -e 's/fNEBULASD/fNEBULAPlusSD/g' \
    -e 's/Creating NEBULA/Creating NEBULAPlus/g' \
    -e 's/fNEBULAParameterMap/fNEBULAPlusParameterMap/g' \
    -e 's/fNEBULADetectorParameterMap/fNEBULAPlusDetectorParameterMap/g' \
    libs/smg4lib/src/construction/src/NEBULAConstruction.cc \
    > libs/smg4lib/src/construction/src/NEBULAPlusConstruction.cc
```

- [ ] **Step 2: Grep for stale NEBULA references**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
grep -nE "\\bNEBULA\\b" libs/smg4lib/src/construction/src/NEBULAPlusConstruction.cc | grep -v Plus | head
```

Expected: no output. If anything appears, fix it manually (most commonly a stale string literal or comment).

- [ ] **Step 3: Build the construction lib**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target smconstruction -j$(nproc) 2>&1 | tail -10
```

Expected: build succeeds. If there are unresolved symbols, the most likely cause is a missed substitution.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/smg4lib/include/NEBULAPlusConstruction.hh \
        libs/smg4lib/src/construction/include/NEBULAPlusConstruction.hh \
        libs/smg4lib/src/construction/src/NEBULAPlusConstruction.cc
git commit -m "feat(g4): clone NEBULAConstruction as NEBULAPlusConstruction"
```

---

### Task 1.10: Add NEBULAPlusSimDataConverter

**Files:**
- Create: `libs/smg4lib/include/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh`
- Create: `libs/smg4lib/src/data/include/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh` (mirror)
- Create: `libs/smg4lib/src/data/src/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.cc`

- [ ] **Step 1: Generate via sed**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -e 's/NEBULASimDataConverter_TArtNEBULAPla/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla/g' \
    -e 's/TArtNEBULAPla/TArtNEBULAPlusPla/g' \
    -e 's/TNEBULASimParameter/TNEBULAPlusSimParameter/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/fNEBULASimData/fNEBULAPlusSimData/g' \
    -e 's/fNEBULAPlaArray/fNEBULAPlusPlaArray/g' \
    -e 's/fNEBULASimParameter/fNEBULAPlusSimParameter/g' \
    libs/smg4lib/include/NEBULASimDataConverter_TArtNEBULAPla.hh \
    > libs/smg4lib/include/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh
cp libs/smg4lib/include/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh \
   libs/smg4lib/src/data/include/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh

sed -e 's/NEBULASimDataConverter_TArtNEBULAPla/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla/g' \
    -e 's/TArtNEBULAPla/TArtNEBULAPlusPla/g' \
    -e 's/TNEBULASimParameter/TNEBULAPlusSimParameter/g' \
    -e 's/NEBULAParameter/NEBULAPlusParameter/g' \
    -e 's/fNEBULASimData/fNEBULAPlusSimData/g' \
    -e 's/fNEBULAPlaArray/fNEBULAPlusPlaArray/g' \
    -e 's/fNEBULASimParameter/fNEBULAPlusSimParameter/g' \
    libs/smg4lib/src/data/src/NEBULASimDataConverter_TArtNEBULAPla.cc \
    > libs/smg4lib/src/data/src/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.cc
```

- [ ] **Step 2: Verify all references match**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
grep -nE "\\bNEBULA\\b|TArtNEBULAPla\\b" \
    libs/smg4lib/src/data/src/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.cc | grep -v Plus
```

Expected: no output.

- [ ] **Step 3: Build smdata**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target smdata -j$(nproc) 2>&1 | tail -10
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/smg4lib/include/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh \
        libs/smg4lib/src/data/include/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh \
        libs/smg4lib/src/data/src/NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.cc
git commit -m "feat(data): NEBULAPlusSimDataConverter writes TArtNEBULAPlusPla"
```

---

### Task 1.11: Wire NEBULA-Plus into DeutDetectorConstruction

**Files:**
- Modify: `libs/sim_deuteron_core/include/DeutDetectorConstruction.hh`
- Modify: `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`

- [ ] **Step 1: Add forward declaration and members in the header**

Open `libs/sim_deuteron_core/include/DeutDetectorConstruction.hh`. Locate the line `class NEBULAConstruction;` (~line 9). Immediately after it, insert:

```cpp
class NEBULAPlusConstruction;
```

Locate `NEBULAConstruction *fNEBULAConstruction;` (around line 74). Immediately after it, insert:

```cpp
  NEBULAPlusConstruction *fNEBULAPlusConstruction;
```

Locate `G4VSensitiveDetector* fNEBULASD = 0;` (around line 94). Immediately after it, insert:

```cpp
  G4VSensitiveDetector* fNEBULAPlusSD = 0;
```

- [ ] **Step 2: Add include and instantiation in the .cc**

Open `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`. After `#include "NEBULAConstruction.hh"` (~line 21), insert:

```cpp
#include "NEBULAPlusConstruction.hh"
#include "NEBULAPlusSD.hh"
#include "G4SDManager.hh"
```

(If `G4SDManager.hh` is already included nearby, skip that line.)

- [ ] **Step 3: Instantiate / delete the construction object**

Find `fNEBULAConstruction = new NEBULAConstruction();` (~line 68). Immediately after, add:

```cpp
  fNEBULAPlusConstruction = new NEBULAPlusConstruction();
```

Find `delete fNEBULAConstruction;` (~line 83). Immediately after, add:

```cpp
  delete fNEBULAPlusConstruction;
```

- [ ] **Step 4: Place NEBULA-Plus geometry in Construct()**

Find the block (~lines 288-289):

```cpp
  fNEBULAConstruction->ConstructSub();
  fNEBULAConstruction->PutNEBULA(expHall_log);
```

Immediately after `PutNEBULA(expHall_log);`, add:

```cpp
  fNEBULAPlusConstruction->ConstructSub();
  fNEBULAPlusConstruction->PutNEBULAPlus(expHall_log);
```

- [ ] **Step 5: Wire the SD assignment**

Find the block (~lines 297-303):

```cpp
  if (fNEBULAConstruction->DoesNeutExist()){
    G4LogicalVolume *neut_log = fNEBULAConstruction->GetLogicNeut();
    // ... existing SD attach ...
  }
  if (fNEBULAConstruction->DoesVetoExist()){
    G4LogicalVolume *veto_log = fNEBULAConstruction->GetLogicVeto();
    // ... existing SD attach ...
  }
```

Immediately after the `fNEBULAConstruction->DoesVetoExist()` block ends, add the parallel block for NEBULA-Plus (use the existing block as a template and substitute):

```cpp
  fNEBULAPlusSD = new NEBULAPlusSD("/SD/NEBULAPlus");
  G4SDManager::GetSDMpointer()->AddNewDetector(fNEBULAPlusSD);
  if (fNEBULAPlusConstruction->DoesNeutExist()){
    G4LogicalVolume *neut_log = fNEBULAPlusConstruction->GetLogicNeut();
    neut_log->SetSensitiveDetector(fNEBULAPlusSD);
  }
  if (fNEBULAPlusConstruction->DoesVetoExist()){
    G4LogicalVolume *veto_log = fNEBULAPlusConstruction->GetLogicVeto();
    veto_log->SetSensitiveDetector(fNEBULAPlusSD);
  }
```

(If the existing NEBULA SD block uses a different attach path / function — e.g., a smg4lib helper — match its style exactly; the snippet above is a literal `SetSensitiveDetector` fallback. Check the surrounding lines.)

- [ ] **Step 6: Initialize the new pointer to nullptr in the constructor init list**

Find the constructor of `DeutDetectorConstruction` (search for `DeutDetectorConstruction::DeutDetectorConstruction()`). In the initialiser list, add `fNEBULAPlusConstruction(nullptr)` next to `fNEBULAConstruction(nullptr)`. If the init list isn't where the member is set, ensure assignment to `nullptr` happens before `Construct()`.

- [ ] **Step 7: Build**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target sim_deuteron_core -j$(nproc) 2>&1 | tail -15
```

Expected: build succeeds. Common failure: `NEBULAPlusConstruction` was forward-declared in the header but not included in the .cc — verify Step 2.

- [ ] **Step 8: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/sim_deuteron_core/include/DeutDetectorConstruction.hh \
        libs/sim_deuteron_core/src/DeutDetectorConstruction.cc
git commit -m "feat(g4): wire NEBULAPlusConstruction into DeutDetectorConstruction"
```

---

### Task 1.12: Register NEBULA-Plus SimData in `sim_deuteron` main

**Files:**
- Modify: `apps/sim_deuteron/main.cc`

- [ ] **Step 1: Find the existing NEBULA initializer line**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
grep -nE "NEBULASimDataInitializer|NEBULASimDataConverter" apps/sim_deuteron/main.cc
```

Note the line numbers and exact variable names.

- [ ] **Step 2: Add NEBULA-Plus initializer + converter setup**

Open `apps/sim_deuteron/main.cc`. After the line that creates / calls `NEBULASimDataInitializer`, insert (preserving the same variable-naming style used by the NEBULA lines):

```cpp
  NEBULAPlusSimDataInitializer initNplus;
  initNplus.Initialize();
```

After the line that registers the NEBULA converter, insert:

```cpp
  NEBULAPlusSimDataConverter_TArtNEBULAPlusPla cnvNplus("NEBULAPlusSimDataConverter");
  cnvNplus.Initialize();
  SimDataManager::GetSimDataManager()->AddConverter(&cnvNplus);
```

Make sure to also add the corresponding `#include` lines at the top of `apps/sim_deuteron/main.cc`:

```cpp
#include "NEBULAPlusSimDataInitializer.hh"
#include "NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh"
```

If the existing NEBULA registration pattern differs from the snippet above (e.g., uses `new` and adds to a list), match that exact pattern.

- [ ] **Step 3: Build `sim_deuteron`**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target sim_deuteron -j$(nproc) 2>&1 | tail -15
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add apps/sim_deuteron/main.cc
git commit -m "feat(app): register NEBULA-Plus SimData in sim_deuteron"
```

---

### Task 1.13: Add NEBULA-Plus geometry-load lines to a sim macro

**Files:**
- Create: `configs/simulation/macros/test_nebula_plus.mac`

- [ ] **Step 1: Write a minimal smoke-test macro**

Create `configs/simulation/macros/test_nebula_plus.mac`:

```
/control/verbose 2
/run/verbose 1

/samurai/geometry/NEBULA/ParameterFileName           {SMSIMDIR}/configs/simulation/geometry/s021/NEBULA_samurai21.csv
/samurai/geometry/NEBULA/DetectorParameterFileName   {SMSIMDIR}/configs/simulation/geometry/s021/NEBULA_Detectors_samurai21.csv

/samurai/geometry/NEBULAPlus/ParameterFileName           {SMSIMDIR}/configs/simulation/geometry/s021/NEBULAPlus_samurai21.csv
/samurai/geometry/NEBULAPlus/DetectorParameterFileName   {SMSIMDIR}/configs/simulation/geometry/s021/NEBULAPlus_Detectors_samurai21.csv

/samurai/geometry/Update

/action/gun/Type Pencil
/action/gun/Z 0
/action/gun/A 1
/action/gun/SetBeamParticle
/action/gun/Position 0 0 -4367 mm
/action/gun/Energy 200 MeV
/action/file/RunName nebula_plus_smoke
/run/beamOn 1
```

- [ ] **Step 2: Run sim_deuteron with this macro**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
source setup.sh
./bin/sim_deuteron configs/simulation/macros/test_nebula_plus.mac 2>&1 | tail -40
```

Expected: simulation prints "Creating NEBULAPlus" or equivalent in the construction trace, the run finishes, and a `.root` output appears under `results/` or the current working directory.

- [ ] **Step 3: Inspect the output ROOT file**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
ROOT_OUT=$(ls -t results/*nebula_plus_smoke*.root 2>/dev/null | head -1)
if [ -z "$ROOT_OUT" ]; then
  ROOT_OUT=$(ls -t *nebula_plus_smoke*.root 2>/dev/null | head -1)
fi
echo "Output: $ROOT_OUT"
root -l -q -b "$ROOT_OUT" -e 'auto* t=(TTree*)_file0->Get("tree"); t->Print();' 2>&1 | grep -E "fNEBULA"
```

Expected: output contains both `fNEBULASimData` and `fNEBULAPlusSimData`, and both `fNEBULASimParameter` and `fNEBULAPlusSimParameter`.

- [ ] **Step 4: Commit the macro**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add configs/simulation/macros/test_nebula_plus.mac
git commit -m "feat(mac): smoke macro that activates both NEBULA and NEBULA-Plus"
```

---

### Task 1.14: Integration test — smoke run

**Files:**
- Create: `tests/integration/test_sim_deuteron_with_nebula_plus.sh`
- Modify: `tests/integration/CMakeLists.txt`

- [ ] **Step 1: Write the shell test**

```bash
#!/bin/bash
# tests/integration/test_sim_deuteron_with_nebula_plus.sh
# Runs sim_deuteron with the NEBULA-Plus smoke macro and asserts that
# the output tree has the new branches.
set -e
cd "$(dirname "$0")/../.."
SMSIMDIR=$(pwd)
export SMSIMDIR
source setup.sh > /dev/null 2>&1

# Run a single-event simulation
OUT=$(mktemp -d)
cd "$OUT"
"$SMSIMDIR"/bin/sim_deuteron \
    "$SMSIMDIR"/configs/simulation/macros/test_nebula_plus.mac \
    > sim.log 2>&1
ROOT_FILE=$(ls *.root | head -1)
if [ -z "$ROOT_FILE" ]; then
    echo "FAIL: no ROOT output produced"
    tail -20 sim.log
    exit 1
fi

# Assert both branches exist
BRANCHES=$(root -l -q -b -e "auto* t=(TTree*)TFile::Open(\"$ROOT_FILE\")->Get(\"tree\"); t->GetListOfBranches()->Print();" 2>/dev/null \
           | grep -oE "fNEBULA[A-Za-z]+SimData(Parameter)?")
echo "$BRANCHES" | grep -q "fNEBULAPlusSimData"      || { echo "FAIL: fNEBULAPlusSimData missing"; exit 1; }
echo "$BRANCHES" | grep -q "fNEBULAPlusSimParameter" || { echo "FAIL: fNEBULAPlusSimParameter missing"; exit 1; }
echo "PASS: NEBULA + NEBULA-Plus branches present"

cd /
rm -rf "$OUT"
```

Make executable:

```bash
chmod +x tests/integration/test_sim_deuteron_with_nebula_plus.sh
```

- [ ] **Step 2: Register in CMake**

Append to `tests/integration/CMakeLists.txt`:

```cmake
add_test(
    NAME test_sim_deuteron_with_nebula_plus
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_sim_deuteron_with_nebula_plus.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(test_sim_deuteron_with_nebula_plus PROPERTIES LABELS "integration")
```

- [ ] **Step 3: Run the test**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake .. >/dev/null
ctest -R test_sim_deuteron_with_nebula_plus -V 2>&1 | tail -25
```

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add tests/integration/test_sim_deuteron_with_nebula_plus.sh \
        tests/integration/CMakeLists.txt
git commit -m "test(integration): smoke run with NEBULA-Plus produces branches"
```

---

### Task 1.15: Phase 1 sign-off

- [ ] **Step 1: Full build + targeted ctest**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . -j$(nproc) 2>&1 | tail -5
ctest -L unit -L integration --output-on-failure 2>&1 | tail -20
```

Expected: all green. Note the test count for the Phase 2 baseline.

- [ ] **Step 2: Tag the Phase 1 boundary**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git tag nebula-plus-phase1
echo "Phase 1 complete. NEBULA-Plus geometry + SimData chain in place."
```

---

## Phase 2 — Reco refactor (extract NEBULABaseReco, rename, sweep)

Goal: refactor existing `NEBULAReco` into `NEBULABaseReco` + `NEBULAReco`, sweep all 16 call sites, delete old files, ensure regression test passes against a pre-refactor golden fixture.

### Task 2.1: Capture pre-refactor golden fixture

**Files:**
- Create: `tools/dump_nebula_reco_golden.cc`
- Create: `tests/fixtures/nebula_reco_golden.root`

- [ ] **Step 1: Find a representative input file**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
ls -t results/*nebula*.root data/*nebula*.root 2>/dev/null | head -3
```

Pick the smallest file with NEBULA branches. If none exists, run `sim_deuteron` with a NEBULA-only mac to generate one. Name the chosen file `$INPUT_ROOT` for the next steps.

- [ ] **Step 2: Write a one-shot dump script**

Create `tools/dump_nebula_reco_golden.cc`:

```cpp
// One-shot tool: run the legacy NEBULAReco against $INPUT_ROOT
// and dump a deterministic representation of its output into
// tests/fixtures/nebula_reco_golden.root. This file is the regression
// pin for Phase 2: after the refactor, NEBULAReco must reproduce it.
#include <cstdlib>
#include <TFile.h>
#include <TTree.h>
#include <TClonesArray.h>
#include "NEBULAReco.hh"
#include "GeometryManager.hh"
#include "RecoEvent.hh"
#include "RecoNeutron.hh"

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: dump_nebula_reco_golden <input.root> <output.root>\n");
        return 1;
    }
    TFile* fin = TFile::Open(argv[1]);
    TTree* t = (TTree*) fin->Get("tree");
    TClonesArray* neb = nullptr;
    t->SetBranchAddress("fNEBULASimData", &neb);

    GeometryManager geo("configs/simulation/geometry/s021/");
    NEBULAReco reco(geo);
    reco.SetTargetPosition({0,0,0});

    TFile* fout = TFile::Open(argv[2], "RECREATE");
    TTree* tout = new TTree("golden", "legacy NEBULAReco outputs");
    std::vector<RecoNeutron>* cands = new std::vector<RecoNeutron>();
    tout->Branch("cands", &cands);

    Long64_t n = t->GetEntries();
    for (Long64_t i = 0; i < n; ++i) {
        t->GetEntry(i);
        *cands = reco.ReconstructNeutrons(neb);
        tout->Fill();
    }
    tout->Write();
    fout->Close();
    fin->Close();
    return 0;
}
```

- [ ] **Step 3: Register in CMake (tools/ subdir if it exists, else inline build)**

Look for `tools/CMakeLists.txt`:

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
ls tools/CMakeLists.txt 2>/dev/null
```

If it doesn't exist, build the dumper inline:

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
g++ -std=c++17 ../tools/dump_nebula_reco_golden.cc \
    -I../libs/analysis/include \
    -I../libs/smg4lib/include \
    -I../libs/smlogger/include \
    -I$(root-config --incdir) \
    $(root-config --libs) \
    -L./lib -lanalysis -lsmdata \
    -Wl,-rpath,$(pwd)/lib \
    -o dump_nebula_reco_golden
```

(Adjust include flags to match the real header locations.)

- [ ] **Step 4: Run the dump**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
mkdir -p tests/fixtures
./build/dump_nebula_reco_golden \
    "$INPUT_ROOT" \
    tests/fixtures/nebula_reco_golden.root
ls -la tests/fixtures/nebula_reco_golden.root
```

Expected: file is created and non-empty (KB-range).

- [ ] **Step 5: Commit fixture and dumper tool**

Note: `tests/fixtures/*.root` may be `.gitignore`-d. If so:

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add -f tests/fixtures/nebula_reco_golden.root tools/dump_nebula_reco_golden.cc
git commit -m "test: capture pre-refactor NEBULA reco golden fixture"
```

---

### Task 2.2: Write the legacy-compat test (RED)

**Files:**
- Create: `tests/analysis/test_NEBULAReco_legacy_compat.cc`
- Modify: `tests/analysis/CMakeLists.txt`

- [ ] **Step 1: Write the test**

```cpp
// tests/analysis/test_NEBULAReco_legacy_compat.cc
// After the refactor, NEBULAReco must produce bit-for-bit (within fp
// tolerance) the same outputs as the legacy NEBULAReco saved
// in tests/fixtures/nebula_reco_golden.root.
#include <gtest/gtest.h>
#include <TFile.h>
#include <TTree.h>
#include <TClonesArray.h>
#include <vector>
#include "NEBULAReco.hh"
#include "GeometryManager.hh"
#include "RecoEvent.hh"
#include "RecoNeutron.hh"

namespace {

TEST(NEBULARecoLegacyCompat, MatchesGoldenFixture) {
    TFile* fgold = TFile::Open("tests/fixtures/nebula_reco_golden.root");
    ASSERT_NE(nullptr, fgold);
    TTree* tgold = (TTree*) fgold->Get("golden");
    ASSERT_NE(nullptr, tgold);
    std::vector<RecoNeutron>* gcands = nullptr;
    tgold->SetBranchAddress("cands", &gcands);

    // Replay against the same input.
    // Choose: store the input path in fixture's user info, OR document
    // that the fixture is paired with a fixed input file. Here we
    // assume the dumper was run against a known sim output.
    TFile* fin = TFile::Open("tests/fixtures/nebula_reco_input.root");
    ASSERT_NE(nullptr, fin);
    TTree* tin = (TTree*) fin->Get("tree");
    TClonesArray* neb = nullptr;
    tin->SetBranchAddress("fNEBULASimData", &neb);

    GeometryManager geo("configs/simulation/geometry/s021/");
    NEBULAReco reco(geo);
    reco.SetTargetPosition({0,0,0});

    Long64_t n = tin->GetEntries();
    ASSERT_EQ(n, tgold->GetEntries());
    for (Long64_t i = 0; i < n; ++i) {
        tin->GetEntry(i);
        tgold->GetEntry(i);

        reco.SetInput(neb);
        auto candidates = reco.ReconstructNeutrons();
        ASSERT_EQ(candidates.size(), gcands->size())
            << "event " << i << " candidate count mismatch";
        for (size_t k = 0; k < candidates.size(); ++k) {
            EXPECT_NEAR(candidates[k].fTime,    (*gcands)[k].fTime,    1e-6);
            EXPECT_NEAR(candidates[k].fEnergy,  (*gcands)[k].fEnergy,  1e-4);
            EXPECT_NEAR(candidates[k].fBeta,    (*gcands)[k].fBeta,    1e-6);
        }
    }
    fin->Close();
    fgold->Close();
}

}  // namespace
```

- [ ] **Step 2: Copy the input file into tests/fixtures so the test is self-contained**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
cp "$INPUT_ROOT" tests/fixtures/nebula_reco_input.root
git add -f tests/fixtures/nebula_reco_input.root
```

- [ ] **Step 3: Wire into `tests/analysis/CMakeLists.txt`**

Append (use existing analysis-test linker libs as template):

```cmake
add_executable(test_NEBULAReco_legacy_compat
    test_NEBULAReco_legacy_compat.cc
)
target_link_libraries(test_NEBULAReco_legacy_compat PRIVATE
    analysis
    smg4lib
    GTest::gtest
    GTest::gtest_main
    ${ROOT_LIBRARIES}
)
gtest_discover_tests(test_NEBULAReco_legacy_compat
    PROPERTIES LABELS "analysis"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

- [ ] **Step 4: Build — it MUST fail to compile (NEBULAReco doesn't exist yet)**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake .. >/dev/null
cmake --build . --target test_NEBULAReco_legacy_compat -j$(nproc) 2>&1 | tail -10
```

Expected: compile error for `NEBULAReco.hh: No such file or directory`. This is RED.

- [ ] **Step 5: Commit the failing test**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add tests/analysis/test_NEBULAReco_legacy_compat.cc \
        tests/analysis/CMakeLists.txt
git commit -m "test(analysis): failing legacy-compat for NEBULAReco refactor"
```

---

### Task 2.3: Extract `NEBULABaseReco`

**Files:**
- Create: `libs/analysis/include/NEBULABaseReco.hh`
- Create: `libs/analysis/src/NEBULABaseReco.cc`

- [ ] **Step 1: Read the legacy class to identify the public algorithm bits**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
cat libs/analysis/include/NEBULAReco.hh
sed -n '1,80p' libs/analysis/src/NEBULAReco.cc
```

Identify which private methods are pure-algorithm (no NEBULA-specific data extraction) and which are extraction-specific (the parts that touch `fNEBULASimData` / `TArtNEBULAPla`).

- [ ] **Step 2: Create `NEBULABaseReco.hh` exposing only the pure-algorithm public API**

```cpp
// libs/analysis/include/NEBULABaseReco.hh
#ifndef NEBULABASERECO_HH
#define NEBULABASERECO_HH

#include "TObject.h"
#include "TVector3.h"
#include <vector>

#include "GeometryManager.hh"
#include "RecoEvent.hh"
#include "RecoNeutron.hh"

// Unified hit view, hit_indices_* removed for simplicity; the base class
// works on a uniform NEBULAHit struct.
struct NEBULAHit {
    int     moduleID;
    TVector3 position;
    double  energy;
    double  time;
    double  qave;
    int     wall_tag = 0; // 0 = unknown (single-detector reco), 1..4 set by joint

    NEBULAHit() = default;
    NEBULAHit(int id, const TVector3& p, double e, double t, double q)
        : moduleID(id), position(p), energy(e), time(t), qave(q) {}
};

class NEBULABaseReco : public TObject {
public:
    explicit NEBULABaseReco(const GeometryManager& geo);
    virtual ~NEBULABaseReco() = default;

    // Parameter setters (same defaults as the legacy class)
    void SetTimeWindow(double w)        { fTimeWindow = w; }
    void SetEnergyThreshold(double t)   { fEnergyThreshold = t; }
    void SetPositionSmearing(double s)  { fPositionSmearing = s; }
    void SetTimeSmearing(double s)      { fTimeSmearing = s; }
    void SetTargetPosition(const TVector3& p) { fTargetPosition = p; }

    double GetTimeWindow()      const { return fTimeWindow; }
    double GetEnergyThreshold() const { return fEnergyThreshold; }

    // Template-method workflow:
    //   ExtractHits() (virtual, derived) -> ClusterHits -> ReconstructFromCluster
    std::vector<RecoNeutron> ReconstructNeutrons();
    void ProcessEvent(RecoEvent& event);

protected:
    // Derived classes must know which input they own.
    virtual std::vector<NEBULAHit> ExtractHits() = 0;

    // Shared algorithm steps (copied verbatim from the legacy class).
    std::vector<std::vector<NEBULAHit>> ClusterHits(const std::vector<NEBULAHit>& hits);
    RecoNeutron ReconstructFromCluster(const std::vector<NEBULAHit>& cluster);
    double CalculateBeta(double flightLength, double tof);
    double CalculateNeutronEnergy(double beta);
    TVector3 ApplyPositionSmearing(const TVector3& pos);
    double  ApplyTimeSmearing(double time);

    const GeometryManager& fGeoManager;
    TVector3 fTargetPosition{0,0,0};
    double fTimeWindow = 3.0;
    double fEnergyThreshold = 6.0;
    double fPositionSmearing = 0.0;
    double fTimeSmearing = 0.0;

    static const double kLightSpeed;
    static const double kNeutronMass;

    ClassDef(NEBULABaseReco, 1);
};

#endif
```

- [ ] **Step 3: Create `NEBULABaseReco.cc` by moving the algorithm bodies from the legacy `.cc`**

Open `libs/analysis/src/NEBULAReco.cc` and copy the bodies of:
- `ClusterHits()`
- `ReconstructFromCluster()`
- `CalculateBeta()`
- `CalculateNeutronEnergy()`
- `ApplyPositionSmearing()`
- `ApplyTimeSmearing()`
- the two `static const` constants

into `libs/analysis/src/NEBULABaseReco.cc`, with class qualifier `NEBULABaseReco::`. Also implement `ReconstructNeutrons()` and `ProcessEvent()` as the template-method drivers:

```cpp
std::vector<RecoNeutron> NEBULABaseReco::ReconstructNeutrons() {
    auto hits = ExtractHits();
    auto clusters = ClusterHits(hits);
    std::vector<RecoNeutron> out;
    out.reserve(clusters.size());
    for (const auto& cl : clusters) {
        out.push_back(ReconstructFromCluster(cl));
    }
    return out;
}

void NEBULABaseReco::ProcessEvent(RecoEvent& event) {
    auto cands = ReconstructNeutrons();
    for (const auto& c : cands) event.AddNeutron(c);
}
```

Constructor:

```cpp
NEBULABaseReco::NEBULABaseReco(const GeometryManager& geo)
    : fGeoManager(geo) {}

ClassImp(NEBULABaseReco)
```

- [ ] **Step 4: Add includes to LinkDef so the new base class is in the dict**

This is wired in Task 2.5 when the LinkDef is edited. For now just save the source.

- [ ] **Step 5: Build (will fail until Task 2.4 brings NEBULAReco)**

We expect the analysis lib to be incomplete here because `NEBULAReco` doesn't exist yet. Don't build standalone. Commit and move on.

- [ ] **Step 6: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/analysis/include/NEBULABaseReco.hh \
        libs/analysis/src/NEBULABaseReco.cc
git commit -m "refactor(analysis): extract NEBULABaseReco from NEBULAReco"
```

---

### Task 2.4: Add `NEBULAReco` (derived)

**Files:**
- Create: `libs/analysis/include/NEBULAReco.hh`
- Create: `libs/analysis/src/NEBULAReco.cc`

- [ ] **Step 1: Header**

```cpp
// libs/analysis/include/NEBULAReco.hh
#ifndef NEBULARECO_HH
#define NEBULARECO_HH

#include "NEBULABaseReco.hh"

class TClonesArray;

// Single-detector NEBULA reconstruction.
// Consumes the `fNEBULASimData` branch (TClonesArray of anaroot TArtNEBULAPla).
class NEBULAReco : public NEBULABaseReco {
public:
    explicit NEBULAReco(const GeometryManager& geo) : NEBULABaseReco(geo) {}
    ~NEBULAReco() override = default;

    void SetInput(TClonesArray* neb_hits) { fNebHits = neb_hits; }

    // Convenience overload for legacy callers passing the array directly.
    std::vector<RecoNeutron> ReconstructNeutrons(TClonesArray* neb_hits) {
        SetInput(neb_hits);
        return NEBULABaseReco::ReconstructNeutrons();
    }
    using NEBULABaseReco::ReconstructNeutrons;

protected:
    std::vector<NEBULAHit> ExtractHits() override;

private:
    TClonesArray* fNebHits = nullptr;

    ClassDef(NEBULAReco, 1);
};

#endif
```

- [ ] **Step 2: `.cc` — port the NEBULA-specific extraction from the legacy class**

Open `libs/analysis/src/NEBULAReco.cc` and copy the body of `ExtractHits(TClonesArray* nebulaData)` into `libs/analysis/src/NEBULAReco.cc` as `NEBULAReco::ExtractHits()`, reading from `fNebHits` instead of an argument. Set `hit.wall_tag = 3` for sub1 / `4` for sub2 if you can disambiguate from the underlying `TArtNEBULAPla` (Layer field), else leave it 0; tags get set by the joint reco anyway.

```cpp
// libs/analysis/src/NEBULAReco.cc
#include "NEBULAReco.hh"
#include "TArtNEBULAPla.hh"
#include "TClonesArray.h"

ClassImp(NEBULAReco)

std::vector<NEBULAHit> NEBULAReco::ExtractHits() {
    std::vector<NEBULAHit> hits;
    if (!fNebHits) return hits;
    const Int_t n = fNebHits->GetEntriesFast();
    hits.reserve(n);
    for (Int_t i = 0; i < n; ++i) {
        auto* pla = static_cast<TArtNEBULAPla*>(fNebHits->UncheckedAt(i));
        // [adapt to the same field reads as the legacy class did]
        // Below is the structure; replace field accessors with the
        // actual TArtNEBULAPla getters used in NEBULAReco.cc.
        if (pla->IsVeto()) continue;
        NEBULAHit h;
        h.moduleID = pla->GetID();
        h.position = TVector3(pla->GetX(), pla->GetY(), pla->GetZ());
        h.energy   = pla->GetEdep();
        h.time     = pla->GetT();
        h.qave     = pla->GetQave();
        h.wall_tag = (pla->GetLayer() == 3) ? 3 : 4;
        if (h.energy < fEnergyThreshold) continue;
        hits.push_back(h);
    }
    return hits;
}
```

The exact getter names (`GetX`, `GetEdep`, `GetT`, etc.) must match what `NEBULAReco::ExtractHits` originally used. Open the legacy `.cc` and copy that block almost verbatim.

- [ ] **Step 3: Build analysis lib**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target analysis -j$(nproc) 2>&1 | tail -15
```

Expected: build succeeds (the legacy NEBULAReco still exists, so symbols still resolve).

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/analysis/include/NEBULAReco.hh libs/analysis/src/NEBULAReco.cc
git commit -m "refactor(analysis): add NEBULAReco derived class"
```

---

### Task 2.5: Update LinkDef + CMake dict, run the legacy-compat test

**Files:**
- Modify: `libs/analysis/include/AnalysisLinkDef.h`
- Modify: `libs/analysis/src/LinkDef.h`
- Modify: `libs/analysis/CMakeLists.txt`

- [ ] **Step 1: Replace the `NEBULAReco` pragma in both LinkDefs**

In `libs/analysis/src/LinkDef.h` (line 19) and `libs/analysis/include/AnalysisLinkDef.h` (line 11), replace the old pragma:

```cpp
#pragma link C++ class NEBULAReco+;
```

with:

```cpp
#pragma link C++ class NEBULABaseReco+;
#pragma link C++ class NEBULAReco+;
```

- [ ] **Step 2: Update CMake dict header list**

Open `libs/analysis/CMakeLists.txt`. Find `ANALYSIS_DICT_HEADERS` and replace the `NEBULAReco.hh` line with two lines:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NEBULABaseReco.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NEBULAReco.hh
```

- [ ] **Step 3: Rebuild analysis, run the legacy-compat test**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake .. >/dev/null
cmake --build . --target analysis test_NEBULAReco_legacy_compat -j$(nproc) 2>&1 | tail -15
ctest -R test_NEBULAReco_legacy_compat -V 2>&1 | tail -30
```

Expected: test PASSES. If it fails with candidate-count mismatch, the `ExtractHits` translation in Task 2.4 lost something — re-check the legacy implementation field-by-field.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/analysis/include/AnalysisLinkDef.h \
        libs/analysis/src/LinkDef.h \
        libs/analysis/CMakeLists.txt
git commit -m "refactor(analysis): wire NEBULABaseReco + NEBULAReco into dict (legacy compat green)"
```

---

### Task 2.6: Update apps + scripts — sweep all 16 call sites

**Files:**
- Modify: `apps/run_reconstruction/main.cc`
- Modify: `libs/analysis/include/NEBULAFrameRotation.hh`
- Modify: `scripts/analysis/nebula_reco_quality/run_nebula_reco.C`
- Modify: `scripts/analysis/batch_analysis.C`
- Modify: `scripts/analysis/legacy_target_reco/analyze_synthetic_px_reco.C`
- Modify: `scripts/analysis/ypol_phys/extract_phys_observables.C`
- Modify: `scripts/analysis/ypol_phys/render_phys_report.py`
- Modify: `scripts/visualization/run_display_safe.C`
- Modify: `scripts/analysis/nebula_reco_quality/README.md`

- [ ] **Step 1: Sweep with a careful sed pattern**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
for f in apps/run_reconstruction/main.cc \
         libs/analysis/include/NEBULAFrameRotation.hh \
         scripts/analysis/nebula_reco_quality/run_nebula_reco.C \
         scripts/analysis/batch_analysis.C \
         scripts/analysis/legacy_target_reco/analyze_synthetic_px_reco.C \
         scripts/analysis/ypol_phys/extract_phys_observables.C \
         scripts/analysis/ypol_phys/render_phys_report.py \
         scripts/visualization/run_display_safe.C \
         scripts/analysis/nebula_reco_quality/README.md; do
    sed -i 's/\bNEBULAReco\b/NEBULAReco/g' "$f"
done
```

- [ ] **Step 2: Update the `#include` line in `apps/run_reconstruction/main.cc`**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
sed -i 's|"NEBULAReco.hh"|"NEBULAReco.hh"|g' apps/run_reconstruction/main.cc
```

- [ ] **Step 3: Confirm no straggler references remain**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git grep -nE "\bNEBULAReco\b" -- ':!libs/analysis/include/NEBULAReco.hh' \
                                          ':!libs/analysis/src/NEBULAReco.cc' \
                                          ':!tools/dump_nebula_reco_golden.cc'
```

Expected: no output.

- [ ] **Step 4: Rebuild full project**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . -j$(nproc) 2>&1 | tail -15
```

Expected: build succeeds. `apps/run_reconstruction` and all ROOT macros now reference `NEBULAReco`.

- [ ] **Step 5: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add apps/run_reconstruction/main.cc \
        libs/analysis/include/NEBULAFrameRotation.hh \
        scripts/analysis/nebula_reco_quality/run_nebula_reco.C \
        scripts/analysis/batch_analysis.C \
        scripts/analysis/legacy_target_reco/analyze_synthetic_px_reco.C \
        scripts/analysis/ypol_phys/extract_phys_observables.C \
        scripts/analysis/ypol_phys/render_phys_report.py \
        scripts/visualization/run_display_safe.C \
        scripts/analysis/nebula_reco_quality/README.md
git commit -m "refactor: sweep NEBULAReco -> NEBULAReco in apps and scripts"
```

---

### Task 2.7: Delete legacy NEBULAReco

**Files:**
- Delete: `libs/analysis/include/NEBULAReco.hh`
- Delete: `libs/analysis/src/NEBULAReco.cc`

- [ ] **Step 1: Delete the files**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git rm libs/analysis/include/NEBULAReco.hh \
       libs/analysis/src/NEBULAReco.cc
```

- [ ] **Step 2: Final grep guard**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git grep -nE "\bNEBULAReco\b" -- ':!tools/dump_nebula_reco_golden.cc' || echo "all clean"
```

Expected: `all clean` printed, OR only `tools/dump_nebula_reco_golden.cc` references appear (that tool is allowed to keep the legacy name in its source until we decide to remove or rename it).

- [ ] **Step 3: Build + full test sweep**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . -j$(nproc) 2>&1 | tail -10
ctest -L unit -L analysis --output-on-failure 2>&1 | tail -25
```

Expected: build + ctest green.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git commit -m "refactor: delete legacy NEBULAReco.hh/cc (now NEBULAReco)"
```

---

### Task 2.8: Phase 2 sign-off

- [ ] **Step 1: Full build + ctest**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . -j$(nproc) 2>&1 | tail -5
ctest -L unit -L analysis -L integration --output-on-failure 2>&1 | tail -25
```

Expected: all green.

- [ ] **Step 2: Tag the Phase 2 boundary**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git tag nebula-plus-phase2
echo "Phase 2 complete. NEBULAReco refactored into NEBULABaseReco + NEBULAReco."
```

---

## Phase 3 — NEBULAPlusReco + NebulaJointReco

Goal: add the two new derived reco classes, expose them through the dict, and add the basic / combined tests.

### Task 3.1: Add NEBULAPlusReco

**Files:**
- Create: `libs/analysis/include/NEBULAPlusReco.hh`
- Create: `libs/analysis/src/NEBULAPlusReco.cc`

- [ ] **Step 1: Header**

```cpp
// libs/analysis/include/NEBULAPlusReco.hh
#ifndef NEBULAPLUSRECO_HH
#define NEBULAPLUSRECO_HH

#include "NEBULABaseReco.hh"

class TClonesArray;

class NEBULAPlusReco : public NEBULABaseReco {
public:
    explicit NEBULAPlusReco(const GeometryManager& geo) : NEBULABaseReco(geo) {}
    ~NEBULAPlusReco() override = default;

    void SetInput(TClonesArray* npl_hits) { fNplHits = npl_hits; }

    std::vector<RecoNeutron> ReconstructNeutrons(TClonesArray* npl_hits) {
        SetInput(npl_hits);
        return NEBULABaseReco::ReconstructNeutrons();
    }
    using NEBULABaseReco::ReconstructNeutrons;

protected:
    std::vector<NEBULAHit> ExtractHits() override;

private:
    TClonesArray* fNplHits = nullptr;

    ClassDef(NEBULAPlusReco, 1);
};

#endif
```

- [ ] **Step 2: Implementation**

```cpp
// libs/analysis/src/NEBULAPlusReco.cc
#include "NEBULAPlusReco.hh"
#include "TArtNEBULAPlusPla.hh"
#include "TClonesArray.h"

ClassImp(NEBULAPlusReco)

std::vector<NEBULAHit> NEBULAPlusReco::ExtractHits() {
    std::vector<NEBULAHit> hits;
    if (!fNplHits) return hits;
    const Int_t n = fNplHits->GetEntriesFast();
    hits.reserve(n);
    for (Int_t i = 0; i < n; ++i) {
        auto* pla = static_cast<TArtNEBULAPlusPla*>(fNplHits->UncheckedAt(i));
        if (pla->fIsVeto) continue;
        NEBULAHit h;
        h.moduleID = pla->fID;
        h.position = pla->fPos;
        h.energy   = pla->fEdep;
        h.time     = pla->fTime;
        h.qave     = 0.5 * (pla->fQU + pla->fQD);
        h.wall_tag = pla->fLayer;     // 1 = NPL-A, 2 = NPL-B
        if (h.energy < fEnergyThreshold) continue;
        hits.push_back(h);
    }
    return hits;
}
```

- [ ] **Step 3: Build**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target analysis -j$(nproc) 2>&1 | tail -10
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/analysis/include/NEBULAPlusReco.hh libs/analysis/src/NEBULAPlusReco.cc
git commit -m "feat(analysis): add NEBULAPlusReco (single-detector NPL reco)"
```

---

### Task 3.2: Add NebulaJointReco

**Files:**
- Create: `libs/analysis/include/NebulaJointReco.hh`
- Create: `libs/analysis/src/NebulaJointReco.cc`

- [ ] **Step 1: Header**

```cpp
// libs/analysis/include/NebulaJointReco.hh
#ifndef NEBULAJOINTRECO_HH
#define NEBULAJOINTRECO_HH

#include "NEBULABaseReco.hh"

class TClonesArray;

// Combines NEBULA + NEBULA-Plus hits into a single reconstruction pass.
// Resulting RecoNeutron candidates carry wall_tag inherited from
// NEBULAHit, distinguishing 1/2 (NEBULA-Plus walls) vs 3/4 (NEBULA walls).
class NebulaJointReco : public NEBULABaseReco {
public:
    explicit NebulaJointReco(const GeometryManager& geo) : NEBULABaseReco(geo) {}
    ~NebulaJointReco() override = default;

    void SetInputs(TClonesArray* neb_hits, TClonesArray* npl_hits) {
        fNebHits = neb_hits;
        fNplHits = npl_hits;
    }

protected:
    std::vector<NEBULAHit> ExtractHits() override;

private:
    TClonesArray* fNebHits = nullptr;
    TClonesArray* fNplHits = nullptr;

    ClassDef(NebulaJointReco, 1);
};

#endif
```

- [ ] **Step 2: Implementation**

```cpp
// libs/analysis/src/NebulaJointReco.cc
#include "NebulaJointReco.hh"
#include "TArtNEBULAPla.hh"
#include "TArtNEBULAPlusPla.hh"
#include "TClonesArray.h"

ClassImp(NebulaJointReco)

std::vector<NEBULAHit> NebulaJointReco::ExtractHits() {
    std::vector<NEBULAHit> hits;
    // NEBULA-Plus first (upstream walls, lower wall_tag indices 1,2)
    if (fNplHits) {
        const Int_t n = fNplHits->GetEntriesFast();
        for (Int_t i = 0; i < n; ++i) {
            auto* pla = static_cast<TArtNEBULAPlusPla*>(fNplHits->UncheckedAt(i));
            if (pla->fIsVeto) continue;
            NEBULAHit h;
            h.moduleID = pla->fID;
            h.position = pla->fPos;
            h.energy   = pla->fEdep;
            h.time     = pla->fTime;
            h.qave     = 0.5 * (pla->fQU + pla->fQD);
            h.wall_tag = pla->fLayer;       // 1 or 2
            if (h.energy < fEnergyThreshold) continue;
            hits.push_back(h);
        }
    }
    // NEBULA (downstream walls, wall_tag 3,4)
    if (fNebHits) {
        const Int_t n = fNebHits->GetEntriesFast();
        for (Int_t i = 0; i < n; ++i) {
            auto* pla = static_cast<TArtNEBULAPla*>(fNebHits->UncheckedAt(i));
            if (pla->IsVeto()) continue;
            NEBULAHit h;
            h.moduleID = pla->GetID();
            h.position = TVector3(pla->GetX(), pla->GetY(), pla->GetZ());
            h.energy   = pla->GetEdep();
            h.time     = pla->GetT();
            h.qave     = pla->GetQave();
            h.wall_tag = (pla->GetLayer() == 3) ? 3 : 4;
            if (h.energy < fEnergyThreshold) continue;
            hits.push_back(h);
        }
    }
    return hits;
}
```

(Adjust TArtNEBULAPla getters to match the legacy class — same as Task 2.4 Step 2.)

- [ ] **Step 3: Build**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . --target analysis -j$(nproc) 2>&1 | tail -10
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/analysis/include/NebulaJointReco.hh libs/analysis/src/NebulaJointReco.cc
git commit -m "feat(analysis): add NebulaJointReco combining NEBULA + NEBULA-Plus hits"
```

---

### Task 3.3: Register the new reco classes in the dict

**Files:**
- Modify: `libs/analysis/include/AnalysisLinkDef.h`
- Modify: `libs/analysis/src/LinkDef.h`
- Modify: `libs/analysis/CMakeLists.txt`

- [ ] **Step 1: Add pragmas**

In both `libs/analysis/include/AnalysisLinkDef.h` and `libs/analysis/src/LinkDef.h`, after the existing `#pragma link C++ class NEBULAReco+;` line, insert:

```cpp
#pragma link C++ class NEBULAPlusReco+;
#pragma link C++ class NebulaJointReco+;
```

- [ ] **Step 2: Add the two new headers to `ANALYSIS_DICT_HEADERS`**

In `libs/analysis/CMakeLists.txt`, after `NEBULAReco.hh`, add:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NEBULAPlusReco.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NebulaJointReco.hh
```

- [ ] **Step 3: Rebuild**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake .. >/dev/null
cmake --build . --target analysis -j$(nproc) 2>&1 | tail -10
```

Expected: dict regenerates, build succeeds.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add libs/analysis/include/AnalysisLinkDef.h \
        libs/analysis/src/LinkDef.h \
        libs/analysis/CMakeLists.txt
git commit -m "build: wire NEBULAPlusReco + NebulaJointReco into analysis dict"
```

---

### Task 3.4: Unit test — NEBULAPlusReco basic

**Files:**
- Create: `tests/unit/test_NEBULAPlusReco_basic.cc`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the test**

```cpp
// tests/unit/test_NEBULAPlusReco_basic.cc
#include <gtest/gtest.h>
#include <TClonesArray.h>
#include "NEBULAPlusReco.hh"
#include "TArtNEBULAPlusPla.hh"
#include "GeometryManager.hh"

namespace {

TEST(NEBULAPlusRecoBasic, ClustersTwoCoincidentHitsIntoOne) {
    TClonesArray arr("TArtNEBULAPlusPla", 4);
    auto add = [&](int idx, int layer, int sub, int id, double x, double y, double z, double t, double e) {
        auto* h = new (arr[idx]) TArtNEBULAPlusPla();
        h->fLayer = layer; h->fSubLayer = sub; h->fID = id;
        h->fPos.SetXYZ(x, y, z); h->fTime = t; h->fEdep = e;
        h->fQU = e; h->fQD = e; h->fTU = t; h->fTD = t;
        h->fIsVeto = false;
    };
    add(0, 1, 1, 101, 0.0,  0.0, 8089.0, 60.0, 8.0);  // Wall-A Sub1
    add(1, 1, 2, 201, 30.0, 0.0, 8219.0, 60.5, 6.0);  // Wall-A Sub2, close in space-time

    GeometryManager geo("configs/simulation/geometry/s021/");
    NEBULAPlusReco reco(geo);
    reco.SetTargetPosition({0,0,0});
    reco.SetInput(&arr);

    auto cands = reco.ReconstructNeutrons();
    EXPECT_EQ(1u, cands.size())
        << "two close hits in adjacent sub-layers should merge into one neutron";
}

TEST(NEBULAPlusRecoBasic, RejectsBelowThreshold) {
    TClonesArray arr("TArtNEBULAPlusPla", 1);
    auto* h = new (arr[0]) TArtNEBULAPlusPla();
    h->fLayer = 1; h->fSubLayer = 1; h->fID = 101;
    h->fPos.SetXYZ(0, 0, 8089); h->fTime = 60.0;
    h->fEdep = 2.0;   // below the 6 MeV default threshold
    h->fIsVeto = false;

    GeometryManager geo("configs/simulation/geometry/s021/");
    NEBULAPlusReco reco(geo);
    reco.SetInput(&arr);

    auto cands = reco.ReconstructNeutrons();
    EXPECT_EQ(0u, cands.size());
}

}  // namespace
```

- [ ] **Step 2: Wire into `tests/unit/CMakeLists.txt`**

```cmake
add_executable(test_NEBULAPlusReco_basic
    test_NEBULAPlusReco_basic.cc
)
target_link_libraries(test_NEBULAPlusReco_basic PRIVATE
    analysis
    smg4lib
    GTest::gtest
    GTest::gtest_main
    ${ROOT_LIBRARIES}
)
gtest_discover_tests(test_NEBULAPlusReco_basic
    PROPERTIES LABELS "unit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

- [ ] **Step 3: Build + run**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake .. >/dev/null
cmake --build . --target test_NEBULAPlusReco_basic -j$(nproc) 2>&1 | tail -5
ctest -R test_NEBULAPlusReco_basic -V 2>&1 | tail -15
```

Expected: both test cases PASS.

- [ ] **Step 4: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add tests/unit/test_NEBULAPlusReco_basic.cc tests/unit/CMakeLists.txt
git commit -m "test(unit): NEBULAPlusReco basic clustering + threshold"
```

---

### Task 3.5: Unit test — NebulaJointReco combined

**Files:**
- Create: `tests/unit/test_NebulaJointReco_combined.cc`
- Modify: `tests/unit/CMakeLists.txt`

**Scope note:** `TArtNEBULAPla` is anaroot's data class and its fields are filled via a multi-stage calibration pipeline (`SetTUCal`, `SetQUCal`, etc.), not direct `SetEdep`/`SetTime`/`SetPos`. Constructing a synthetic NEBULA hit in a unit test is impractical. This unit test therefore exercises only the NPL side of the joint reco plus the empty-NEBULA boundary; a cross-detector merge test belongs in an end-to-end fixture (deferred — see the self-review section at the bottom).

- [ ] **Step 1: Write the test (NPL-only + empty-NEB boundary)**

```cpp
// tests/unit/test_NebulaJointReco_combined.cc
#include <gtest/gtest.h>
#include <TClonesArray.h>
#include "NebulaJointReco.hh"
#include "TArtNEBULAPlusPla.hh"
#include "TArtNEBULAPla.hh"
#include "GeometryManager.hh"

namespace {

TEST(NebulaJointRecoCombined, HandlesNullNebHitsGracefully) {
    TClonesArray npl("TArtNEBULAPlusPla", 1);
    auto* hp = new (npl[0]) TArtNEBULAPlusPla();
    hp->fLayer = 1; hp->fSubLayer = 1; hp->fID = 101;
    hp->fPos.SetXYZ(0, 0, 8089); hp->fTime = 60.0;
    hp->fEdep = 8.0; hp->fIsVeto = false;

    GeometryManager geo("configs/simulation/geometry/s021/");
    NebulaJointReco reco(geo);
    reco.SetTargetPosition({0,0,0});

    // null NEBULA input must not crash; reco runs on NPL only
    reco.SetInputs(nullptr, &npl);
    auto cands = reco.ReconstructNeutrons();
    EXPECT_GE(cands.size(), 1u);
    EXPECT_EQ(cands[0].fWallTag, 1) << "wall_tag should propagate from NPL hit";
}

TEST(NebulaJointRecoCombined, HandlesEmptyArraysGracefully) {
    TClonesArray neb("TArtNEBULAPla", 0);
    TClonesArray npl("TArtNEBULAPlusPla", 0);
    GeometryManager geo("configs/simulation/geometry/s021/");
    NebulaJointReco reco(geo);
    reco.SetInputs(&neb, &npl);
    EXPECT_NO_THROW({ auto cands = reco.ReconstructNeutrons(); EXPECT_EQ(cands.size(), 0u); });
}

}  // namespace
```

If `RecoNeutron` doesn't yet have a `fWallTag` field, add it in this task (one extra Int_t member + ClassDef bump). The legacy class can stay backwards compatible by defaulting it to 0.

- [ ] **Step 2: Add `fWallTag` to `RecoNeutron` if missing**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
grep -nE "fWallTag|wall_tag" libs/analysis/include/RecoNeutron.hh
```

If not present, edit `libs/analysis/include/RecoNeutron.hh` to add:

```cpp
public:
    Int_t fWallTag = 0;  // 1..2 = NEBULA-Plus, 3..4 = NEBULA, 0 = unset
```

Bump the `ClassDef(RecoNeutron, N)` version by 1. Then update `NEBULABaseReco::ReconstructFromCluster` to set it from cluster's first hit `wall_tag`.

- [ ] **Step 3: Wire into CMake**

Append to `tests/unit/CMakeLists.txt`:

```cmake
add_executable(test_NebulaJointReco_combined
    test_NebulaJointReco_combined.cc
)
target_link_libraries(test_NebulaJointReco_combined PRIVATE
    analysis
    smg4lib
    GTest::gtest
    GTest::gtest_main
    ${ROOT_LIBRARIES}
)
gtest_discover_tests(test_NebulaJointReco_combined
    PROPERTIES LABELS "unit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

- [ ] **Step 4: Build + run**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake .. >/dev/null
cmake --build . --target test_NebulaJointReco_combined -j$(nproc) 2>&1 | tail -5
ctest -R test_NebulaJointReco_combined -V 2>&1 | tail -15
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add tests/unit/test_NebulaJointReco_combined.cc tests/unit/CMakeLists.txt \
        libs/analysis/include/RecoNeutron.hh \
        libs/analysis/src/NEBULABaseReco.cc
git commit -m "test(unit): NebulaJointReco combined; RecoNeutron gets fWallTag"
```

---

### Task 3.6: Phase 3 sign-off + final integration

**Files:**
- (Verification only)

- [ ] **Step 1: Full clean rebuild**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake --build . -j$(nproc) 2>&1 | tail -5
```

Expected: success.

- [ ] **Step 2: Run every label**

```bash
ctest -L unit -L analysis -L integration --output-on-failure 2>&1 | tail -30
```

Expected: all green.

- [ ] **Step 3: Grep guard final check**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
echo "Stragglers of legacy class name:"
git grep -nE "\bNEBULAReco\b" -- ':!tools/dump_nebula_reco_golden.cc' || echo "  (none)"
echo
echo "Stragglers of placeholder Z (should be empty):"
git grep -nE "NEBULA-?Plus.*Z *= *8[0-9]{3} *mm" -- configs/ docs/ | grep -v "8089" || echo "  (only 8089)"
```

Expected: both guards clean.

- [ ] **Step 4: Tag Phase 3 and update memory**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git tag nebula-plus-phase3
git log --oneline nebula-plus-phase1..HEAD | wc -l
echo "Phase 3 complete. NEBULA-Plus + Joint reco landed."
```

---

## Self-review summary

This plan covers, by spec section:

| Spec section | Covered by tasks |
|--------------|------------------|
| §2 Goals 1 (G4 module) | 1.7, 1.8, 1.9, 1.11 |
| §2 Goals 2 (ROOT data class + SimData branches) | 1.2, 1.3, 1.4, 1.5, 1.6, 1.10, 1.12 |
| §2 Goals 3 (reco refactor hierarchy) | 2.3, 2.4, 3.1, 3.2 |
| §2 Goals 4 (user-macro callable) | exposed via Task 3.x classes; verified in 3.4–3.5 |
| §2 Goals 5 (existing behaviour preserved) | 2.1, 2.2, 2.5 (legacy-compat golden test) |
| §3 Geometry numbers | 1.1 |
| §4 New files / modify / delete | mapped 1:1 to tasks above |
| §5 C++ API | 1.x for G4 side, 2.x and 3.x for reco side |
| §6 Data flow + branch names | 1.6, 1.10, 1.12, 1.13, 1.14 |
| §7 CMake impact | 1.4, 2.5, 3.3 |
| §8 Test strategy | 1.4 (parameter reader), 1.14 (smoke), 2.2 (legacy compat), 3.4 (NPL reco), 3.5 (joint reco) |
| §9 Risks (Z placeholder, default smearing) | exposed via messenger from Task 1.8, plus design notes in 2.3 |
| §10 Implementation order (3 phases) | Phase 1: 1.x, Phase 2: 2.x, Phase 3: 3.x |

### Spec items NOT covered by this plan (deferred)

- §8 `test_NEBULAPlusGeometry.cc`, `test_NEBULAPlusSimDataConverter.cc`,
  `test_NEBULABaseReco_refactor.cc`, `test_NebulaJointReco_efficiency.cc`,
  `test_vis_with_nebula_plus.sh`: these are described in the spec but
  the regression / smoke set in this plan (legacy_compat + basic + combined +
  parameter reader + sim_deuteron smoke) is the minimum-viable test
  matrix. Add the remaining tests as a follow-up after Phase 3 lands.
- **Cross-detector merge unit test**: synthesising a NEBULA hit in a unit
  test requires going through anaroot's multi-stage calibration setters
  (TURaw / TUCal / QURaw / QUCal / etc.) which is more work than the value
  delivered. The integration test (Task 1.14) already runs a real
  `sim_deuteron` event through both detectors; the cross-merge can be
  asserted there once Phase 3 lands, by adding a step that runs
  `NebulaJointReco` over the smoke output and counts cross-detector
  clusters. This is a small follow-up but is not in scope for this plan.
- §11 R_cluster / Δt messenger exposure: deferred; current setter-only
  API is sufficient.
- §11 RecoNeutron persistence as a tree branch: deferred; users hold
  the candidate vector in-memory.

These deferrals are intentional and noted here for transparency.
