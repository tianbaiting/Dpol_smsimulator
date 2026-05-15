# NEBULA-Plus simulation + reconstruction design

- **Date**: 2026-05-15
- **Author**: Baiting Tian (with Claude Code brainstorming)
- **Scope**: Add a new Geant4 sub-detector `NEBULAPlus` to smsimulator5.5
  covering geometry, SimData output, and a refactored neutron
  reconstruction stack (single-detector NEBULA, single-detector
  NEBULA-Plus, and joint NEBULA + NEBULA-Plus).
- **Companion notes**:
  `docs/nebula_and_nebula_plus.tex` (physics + data-source summary
  written 2026-05-15) and
  `kondo_smsimulator/docs/papers/` (collected PDF references).

## 1. Background and motivation

NEBULA-Plus is the LPC-Caen / RIKEN extension of the NEBULA neutron-wall
array at SAMURAI (RIBF). It adds 90 plastic-scintillator bars sitting
upstream of NEBULA (between FDC2/HOD and NEBULA) and approximately
doubles the number of detection sub-layers seen by an incoming neutron.
Quoted gain in the Tanaka 2024 review (arXiv:2412.17887): single-neutron
efficiency ≈ ×2, four-neutron efficiency ≈ ×10, relative to NEBULA
alone.

Current state of the surrounding codebase:

- **smsimulator5.5** has a NEBULA G4 module (geometry, SD, SimData
  converter) and an analysis-side `NEBULAReco`
  (`libs/analysis/`). No NEBULA-Plus support.
- **anaroot** reserves `Layer=1,2` for NEBULA-Plus and channel names
  `V301–V410` for its veto bars, but the per-bar positions are
  placeholders (`Z=0/1`), and the reco code (`TArtCalibNEBULA`,
  `TArtNEBULAFilter`) only hardcodes `Layer == 1/2`, which is the *old*
  convention for NEBULA itself. anaroot cannot run a joint reco today.
- **LPC nptool plugin** (`/home/s053/exp/exp2301_s053/nptool/plugin/nebula-plus/`)
  has the canonical NEBULA-Plus geometry and standalone reco, but does
  not combine with NEBULA hits.

This spec describes how to make smsimulator5.5 self-sufficient for
NEBULA-Plus simulation and joint reconstruction, without depending on
anaroot reco updates.

## 2. Goals and non-goals

**Goals:**

1. New `NEBULAPlus` Geant4 sub-detector in `libs/smg4lib/`, parallel to
   the existing `NEBULA` module.
2. New ROOT data class `TArtNEBULAPlusPla` and SimData branches
   `fNEBULAPlusSimData` + `fNEBULAPlusSimParameter` written to the
   output tree.
3. Refactor `libs/analysis/NEBULAReco` into a small class
   hierarchy: `NEBULABaseReco` (abstract base) →
   `NEBULAReco` / `NEBULAPlusReco` / `NebulaJointReco`.
4. User analysis macros can instantiate any of the three reco classes
   and consume `RecoNeutron` candidates.
5. Existing NEBULA simulation behaviour preserved; existing
   reconstruction physics preserved bit-for-bit (regression-tested).

**Non-goals (out of scope for this spec):**

- Patching anaroot's `TArtCalibNEBULA` / `TArtNEBULAFilter` to know
  NEBULA-Plus. Tracked separately.
- Driving sim_deuteron itself to run reco as a built-in step. User
  analysis macros own the reco invocation.
- Efficiency-measurement driver scripts (single-neutron / multi-neutron
  ε vs energy). Will be a follow-up project.
- Real survey-quality calibration constants (PMT TimeReso, attenuation,
  walk). We re-use NEBULA values as placeholders; LPC commissioning
  data substitution is a follow-up.

## 3. Geometry data (authoritative numbers)

Derived from the LPC-Caen nptool plugin
(`commissioning/detector.yaml`) + APR 56 (2023) p.91 installation
report + the spacing convention agreed in brainstorming
(NEBULA-Plus Sub1 → Sub1 spacings 845 / 850 / 850.3 mm across the four
walls).

### 3.1 Target-frame layout (mm)

```
                                       NEBULA-Plus                NEBULA
                            ┌──────────────────────┐  ┌──────────────────────┐
target → ...  HOD → ... →   │ A-Veto  A1   A2      │  │ A-Veto  A1   A2      │
                            │ Z=7989 8089 8219     │  │ Z=9509 9784 9914     │
                            │   B-Veto B1   B2     │  │   B-Veto B1   B2     │
                            │   Z=8834 8934 9064   │  │   Z=10359 10634 10764│
                            └──────────────────────┘  └──────────────────────┘
                              4 sub-layers, 90 bars     4 sub-layers, 120 bars

Sub1→Sub1 spacings along beam Z:
  NPL-A → NPL-B   :  845.0 mm (NEBULA-Plus internal)
  NPL-B → NEB-A   :  850.0 mm (NPL-NEB gap chosen)
  NEB-A → NEB-B   :  850.3 mm (NEBULA internal)
```

NEBULA-Plus global origin (Wall-A Sub1 centre) at target-frame
**Z = 8089 mm**.

### 3.2 Per-bar layout (NEBULA-Plus internal frame; Z offsets relative to origin)

| Wall | SubLayer | Type | #bars | ID range | Z (mm) | X start (mm) | X centres (mm) |
|------|----------|------|-------|----------|--------|--------------|----------------|
| A    | 1 (front)| Neut | 22    | 101–122  |   0    | −1320        | −1320 → +1200, pitch 120 |
| A    | 2 (back) | Neut | 22    | 201–222  |  130   | −1320        | same |
| B    | 1 (front)| Neut | 23    | 301–323  |  845   | −1320        | −1320 → +1320, pitch 120 |
| B    | 2 (back) | Neut | 23    | 401–423  |  975   | −1320        | same |
| A    | 0 (veto) | Veto | 12    | 501–512  |  −100  | +1705        | +1705 → −1705, pitch 310 |
| B    | 0 (veto) | Veto | 12    | 601–612  |  +745  | +1705        | same |

X layout is intentionally **asymmetric** for the 22-bar walls
(X span −1320 → +1200, off-centre by −60 mm). This matches LPC's
as-built alignment in the nptool yaml; symmetry is not required for
the simulation.

Total: 90 Neut + 24 Veto = **114 elements**.

### 3.3 Material and PMT model

| Property | Value | Source |
|---------|-------|--------|
| Bar dimensions (W×H×L mm) | 120 × 1800 × 120 | nptool plugin |
| Veto dimensions | 320 × 1900 × 10 | nptool plugin |
| Bar pitch (X) | 120 mm (touching) | nptool plugin |
| Veto pitch (X) | 310 mm | NEBULA convention reused |
| Veto distance to wall | 100 mm upstream of Neut Sub1 | nptool `WallToVeto` |
| Sub-layer Z gap | 130 mm | NEBULA / NPL identical |
| Bar material | `G4_PLASTIC_SC_VINYLTOLUENE` | same as NEBULA; BC400/EJ200/PVT all within ~1 % |
| PMT refractive index n | 1.58 | nptool |
| Bulk attenuation length | 6680 mm | nptool |
| PMT time resolution σ_t | 0.240 ns/PMT | NEBULA default, refine after LPC calibration |

## 4. File layout (creates + modifies + deletes)

### 4.1 New files

```
libs/smg4lib/include/
    NEBULAPlusConstruction.hh
    NEBULAPlusConstructionMessenger.hh
    NEBULAPlusSD.hh
    NEBULAPlusSimDataInitializer.hh
    NEBULAPlusSimParameterReader.hh
    NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh
    TNEBULAPlusSimParameter.hh
    TArtNEBULAPlusPla.hh

libs/smg4lib/src/construction/src/
    NEBULAPlusConstruction.cc
    NEBULAPlusConstructionMessenger.cc
    NEBULAPlusSD.cc

libs/smg4lib/src/construction/include/
    NEBULAPlusConstruction.hh        (mirror of public header)
    NEBULAPlusConstructionMessenger.hh
    NEBULAPlusSD.hh

libs/smg4lib/src/data/src/
    TNEBULAPlusSimParameter.cc
    NEBULAPlusSimParameterReader.cc
    NEBULAPlusSimDataInitializer.cc
    NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.cc
    TArtNEBULAPlusPla.cc

libs/smg4lib/src/data/include/
    (mirror headers as above)

libs/analysis/include/
    NEBULABaseReco.hh
    NEBULAReco.hh
    NEBULAPlusReco.hh
    NebulaJointReco.hh

libs/analysis/src/
    NEBULABaseReco.cc
    NEBULAReco.cc           (mostly moved from NEBULAReco.cc)
    NEBULAPlusReco.cc
    NebulaJointReco.cc

configs/simulation/geometry/s021/
    NEBULAPlus_samurai21.csv
    NEBULAPlus_Detectors_samurai21.csv

tests/unit/
    test_NEBULAPlusSimParameterReader.cc
    test_NEBULAPlusGeometry.cc
    test_NEBULAPlusSimDataConverter.cc
    test_NEBULABaseReco_refactor.cc
    test_NEBULAPlusReco_basic.cc
    test_NebulaJointReco_combined.cc

tests/analysis/
    test_NebulaJointReco_efficiency.cc
    test_NEBULAReco_legacy_compat.cc

tests/integration/
    test_sim_deuteron_with_nebula_plus.sh
    test_vis_with_nebula_plus.sh
```

### 4.2 Modified files

| File | Change |
|------|--------|
| `libs/sim_deuteron_core/include/DeutDetectorConstruction.hh` | add forward decl `NEBULAPlusConstruction`; add members `fNEBULAPlusConstruction`, `fNEBULAPlusSD` |
| `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc` | instantiate `fNEBULAPlusConstruction`, create SD, call `PutNEBULAPlus(ExpHall_log)` in `Construct()` / `UpdateGeometry()`; delete in dtor |
| `apps/sim_deuteron/main.cc` | after `NEBULASimDataInitializer initNeb.Initialize()`, also call `NEBULAPlusSimDataInitializer initNplus.Initialize()` |
| `apps/run_reconstruction/main.cc` | replace all `NEBULAReco` with `NEBULAReco` |
| `libs/analysis/CMakeLists.txt` | in `ANALYSIS_DICT_HEADERS`, replace `NEBULAReco.hh` with the four new headers (`NEBULABaseReco.hh`, `NEBULAReco.hh`, `NEBULAPlusReco.hh`, `NebulaJointReco.hh`) |
| `libs/analysis/include/AnalysisLinkDef.h` | rename pragma class `NEBULAReco` to `NEBULAReco`; add 3 new pragmas |
| `libs/analysis/src/LinkDef.h` | same as above |
| `libs/analysis/include/NEBULAFrameRotation.hh` | comment string `NEBULAReco` → `NEBULAReco` |
| `scripts/analysis/nebula_reco_quality/run_nebula_reco.C` | rename class |
| `scripts/analysis/batch_analysis.C` | rename class |
| `scripts/analysis/legacy_target_reco/analyze_synthetic_px_reco.C` | rename class |
| `scripts/analysis/ypol_phys/extract_phys_observables.C` | rename class |
| `scripts/analysis/ypol_phys/render_phys_report.py` | rename class string |
| `scripts/visualization/run_display_safe.C` | rename class |
| `scripts/analysis/nebula_reco_quality/README.md` | rename class |
| `configs/simulation/macros/run.mac` (or equivalent) | add `/samurai/geometry/NEBULAPlus/ParameterFileName` and `DetectorParameterFileName` lines |

### 4.3 Deletes

```
libs/analysis/include/NEBULAReco.hh
libs/analysis/src/NEBULAReco.cc
```

ROOT `_C.d` cache files under `scripts/analysis/` are auto-regenerated;
no manual delete.

## 5. C++ API (sketch)

### 5.1 G4 side (mirror of NEBULA, no new patterns)

```cpp
// libs/smg4lib/include/NEBULAPlusConstruction.hh
class NEBULAPlusConstruction {
public:
  NEBULAPlusConstruction();
  ~NEBULAPlusConstruction();
  G4VPhysicalVolume* Construct();
  G4LogicalVolume*   PutNEBULAPlus(G4LogicalVolume* ExpHall_log);
  void               SetNEBULAPlusSD(G4VSensitiveDetector* sd) { fNEBULAPlusSD = sd; }
private:
  G4LogicalVolume* ConstructSub();
  TNEBULAPlusSimParameter* fNEBULAPlusSimParameter;
  NEBULAPlusConstructionMessenger* fMessenger;
  G4LogicalVolume *fLogicNeut, *fLogicVeto;
  G4VSensitiveDetector* fNEBULAPlusSD;
  G4Material *fVacuumMaterial, *fNeutMaterial;
  G4ThreeVector fPosition;
  bool fNeutExist, fVetoExist;
};
```

```cpp
// libs/smg4lib/include/TArtNEBULAPlusPla.hh
class TArtNEBULAPlusPla : public TObject {
public:
  Int_t    fID;          // 101..122, 201..222 (Wall-A Neut Sub1/2),
                         // 301..323, 401..423 (Wall-B Neut Sub1/2),
                         // 501..512 (Wall-A Veto), 601..612 (Wall-B Veto)
  Int_t    fLayer;       // 1 = Wall-A, 2 = Wall-B
  Int_t    fSubLayer;    // 0 = Veto, 1 = front sub, 2 = back sub
  Double_t fTime;        // ns, mean of TU/TD
  Double_t fEdep;        // MeV total energy deposit in this bar this event
  Double_t fQU, fQD;     // upper / lower PMT charges (arb. units)
  Double_t fTU, fTD;     // upper / lower PMT times (ns)
  TVector3 fPos;         // reconstructed hit position (mm, target frame)
  Bool_t   fIsVeto;
  ClassDef(TArtNEBULAPlusPla, 1);
};
```

Messenger commands exposed:

```
/samurai/geometry/NEBULAPlus/ParameterFileName           <path>
/samurai/geometry/NEBULAPlus/DetectorParameterFileName   <path>
/samurai/geometry/NEBULAPlus/Position                    <x> <y> <z> mm
/samurai/geometry/Update
```

### 5.2 Reco class hierarchy (libs/analysis/)

```cpp
// NEBULABaseReco.hh
class NEBULABaseReco : public TObject {
public:
  NEBULABaseReco(const GeometryManager& geo);
  virtual ~NEBULABaseReco() = default;

  void SetTimeWindow(double w)       { fTimeWindow = w; }
  void SetEnergyThreshold(double t)  { fEnergyThreshold = t; }
  void SetPositionSmearing(double s) { fPositionSmearing = s; }
  void SetTimeSmearing(double s)     { fTimeSmearing = s; }
  void SetTargetPosition(const TVector3& p) { fTargetPosition = p; }

  // Template method: ExtractHits (virtual) -> ClusterHits -> ReconstructFromCluster
  std::vector<RecoNeutron> ReconstructNeutrons();
  void ProcessEvent(RecoEvent& event);

protected:
  virtual std::vector<NEBULAHit> ExtractHits() = 0;
  std::vector<std::vector<NEBULAHit>> ClusterHits(const std::vector<NEBULAHit>&);
  RecoNeutron ReconstructFromCluster(const std::vector<NEBULAHit>&);
  double CalculateBeta(double flight_length, double tof);
  double CalculateNeutronEnergy(double beta);
  TVector3 ApplyPositionSmearing(const TVector3& pos);
  double   ApplyTimeSmearing(double t);

  const GeometryManager& fGeoManager;
  TVector3 fTargetPosition{0,0,0};
  // Defaults are placeholders to be reconciled with the actual values
  // hardcoded in libs/analysis/src/NEBULAReco.cc during the
  // refactor; the legacy-compat test pins them down.
  double fTimeWindow = 3.0;        // ns
  double fEnergyThreshold = 6.0;   // MeV
  double fPositionSmearing = 0.0;  // mm
  double fTimeSmearing = 0.0;      // ns
};

// NEBULAReco.hh         — single-detector NEBULA (uses fNEBULASimData branch)
// NEBULAPlusReco.hh     — single-detector NEBULA-Plus (uses fNEBULAPlusSimData branch)
// NebulaJointReco.hh    — joint, takes both branches; ExtractHits merges them with wall_tag
```

`NEBULAHit` struct (already exists) gains an optional field `int wall_tag`
(1=NPL-A, 2=NPL-B, 3=NEBULA-A, 4=NEBULA-B) so `ClusterHits` can label
candidates. The struct itself stays in `libs/analysis/include/`.

### 5.3 User analysis usage

```cpp
// User macro (simplified)
auto* f = TFile::Open("sim_output.root");
auto* t = (TTree*) f->Get("tree");

TClonesArray* neb = nullptr;
TClonesArray* npl = nullptr;
t->SetBranchAddress("fNEBULASimData",     &neb);
t->SetBranchAddress("fNEBULAPlusSimData", &npl);

GeometryManager geo("configs/simulation/geometry/s021/");
NebulaJointReco reco(geo);
reco.SetTargetPosition({0, 0, 0});

for (Long64_t i = 0; i < t->GetEntries(); ++i) {
    t->GetEntry(i);
    reco.SetInputs(neb, npl);
    const auto cands = reco.ReconstructNeutrons();
    // analyse cands ...
}
```

## 6. SimData branches and event-level data flow

```
G4Step in NEBULA-Plus bar
  └─> NEBULAPlusSD::ProcessHits()
        accumulates per-bar { ID, Edep, t, hit_pos }
End of event
  └─> NEBULAPlusSimDataConverter_TArtNEBULAPlusPla::Convert()
        per hit bar:
          compute TU = t + (|Y_hit + 900| / (c/n))            (n = 1.58)
          compute TD = t + (|Y_hit - 900| / (c/n))
          QU = Edep * exp(-|Y_hit + 900| / 6680 mm)
          QD = Edep * exp(-|Y_hit - 900| / 6680 mm)
          add Gaussian time jitter σ = fTimeReso = 0.240 ns to TU/TD
          push TArtNEBULAPlusPla into fNEBULAPlusSimData (TClonesArray)

fNEBULAPlusSimData      : TClonesArray of TArtNEBULAPlusPla (event-level)
fNEBULAPlusSimParameter : TNEBULAPlusSimParameter snapshot (run-level)
```

Veto hits share `TArtNEBULAPlusPla` with `fIsVeto=true` / `fSubLayer=0`.
No separate veto branch.

## 7. Build / CMake impact (verified by inspection)

- `libs/smg4lib/src/construction/CMakeLists.txt`: uses
  `file(GLOB_RECURSE *.cc)` → **zero CMake change** for new .cc files.
- `libs/smg4lib/src/data/CMakeLists.txt`: uses
  `file(GLOB_RECURSE src/*.cc)` for sources and `file(GLOB include/T*.hh)`
  for the ROOT dictionary header set → **zero CMake change**
  (`TArtNEBULAPlusPla.hh` and `TNEBULAPlusSimParameter.hh` start with `T`
  and are auto-collected).
- `libs/analysis/CMakeLists.txt`: sources are GLOB but dictionary
  headers are an **explicit list**. The list must be edited to swap
  `NEBULAReco.hh` for `NEBULAReco.hh` and add three new
  headers.
- `libs/analysis/src/LinkDef.h` and `libs/analysis/include/AnalysisLinkDef.h`:
  rename one pragma class entry, add three new ones.

`smdata_linkdef.hh` in `libs/smg4lib/src/data/` already covers all
`T*` classes by glob; verify it does not need a manual addition for
`TArtNEBULAPlusPla` / `TNEBULAPlusSimParameter` (it should not, but
sanity-check during implementation).

## 8. Test strategy

### Unit (`tests/unit/`, GTest, label `unit`)

| Test | Asserts |
|------|---------|
| `test_NEBULAPlusSimParameterReader` | CSV parses to 114 bars, 22+22+23+23 sub-layer counts, 12 veto/wall, `fPosition.z = 8089`, `fTimeReso = 0.240` |
| `test_NEBULAPlusGeometry` | per-bar X start `-1320`, pitch 120; sub-layer Z offsets 0/130/845/975; veto Z offsets -100/+745 |
| `test_NEBULAPlusSimDataConverter` | synthetic step in bar ID=101, 5 MeV @ t=10 ns ⇒ one `TArtNEBULAPlusPla` with `fLayer=1`, `fSubLayer=1`, `fEdep≈5`, plausible TU/TD |
| `test_NEBULABaseReco_refactor` | Same hits → new `NEBULAReco` gives `RecoNeutron` results equal (within fp tolerance) to the old `NEBULAReco` output on a captured fixture |
| `test_NEBULAPlusReco_basic` | Hand-crafted `TArtNEBULAPlusPla` array with Layer 1/2 hits clusters correctly; hits with Layer 3/4 (if present by accident) are ignored |
| `test_NebulaJointReco_combined` | NEBULA + NEBULA-Plus hits at consistent space-time form one cluster; `wall_tag` set; cross-detector veto works |

### Analysis (`tests/analysis/`, label `analysis`)

| Test | Asserts |
|------|---------|
| `test_NebulaJointReco_efficiency` | Synthetic monoenergetic neutrons (200 MeV) thrown at the array; relative ε ordering `NebulaJointReco > NEBULAReco` and `NebulaJointReco > NEBULAPlusReco`; absolute single-n ε within `[0.20, 0.45]` for NEBULA-only and `[0.40, 0.75]` for joint |
| `test_NEBULAReco_legacy_compat` | Golden-file comparison: pre-refactor `NEBULAReco` output saved into `tests/fixtures/nebula_reco_golden.root`; new `NEBULAReco` reproduces it |

### Integration (`tests/integration/`, label `integration`)

| Test | Asserts |
|------|---------|
| `test_sim_deuteron_with_nebula_plus.sh` | `./run_sim.sh` with a NEBULA-Plus-enabled macro produces a root file containing `fNEBULAPlusSimData` and `fNEBULAPlusSimParameter` branches; bar count reconstructed from `fNEBULAPlusSimParameter` is 114 |
| `test_vis_with_nebula_plus.sh` | VRML2FILE viewer driver run produces `g4_00.wrl` containing both `NebulaModule`-class G4Box entries and NEBULA-Plus-corresponding ones (grep on volume names) |

### Static checks (CI add-ons)

- `grep -r NEBULAReco libs/ apps/ scripts/ docs/` must return zero
  hits after the refactor lands.
- Build with `BUILD_TESTS=ON` must succeed; `ctest -L unit -L analysis -L integration`
  must pass.
- Pre-refactor one-shot scan
  `tools/scan_old_roots_for_nebula_reconstructor.sh` walks `results/`
  / `data/` to confirm no persisted `NEBULAReco` objects
  exist; if any are found, decision deferred to follow-up.

## 9. Risks and mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| `NEBULAReco` regression in physics output after refactor | low | high | golden-file test `test_NEBULAReco_legacy_compat` runs both old and new on a captured fixture |
| Missed call site of `NEBULAReco` (especially in user scripts) | medium | low | CI grep guard rules out missed references |
| NEBULA-Plus absolute Z is a brainstorming-chosen value, not surveyed | high | medium | exposed via Messenger (`/samurai/geometry/NEBULAPlus/Position`) so mac override is one-line; LPC alignment can be substituted later without a rebuild |
| 22-bar walls X asymmetry might confuse users | low | low | document the −60 mm offset in `nebula_and_nebula_plus.tex` |
| `BC400` vs `EJ200` material substitution | low | low | both PVT-based; effective light yield difference ~1 %; we use `G4_PLASTIC_SC_VINYLTOLUENE` consistent with NEBULA |
| Time / energy smearing defaults inherited from NEBULA | medium | medium | exposed in `NEBULABaseReco` setters; not hardcoded; documented as placeholders |
| anaroot still cannot consume NEBULA-Plus | high | none (out of scope) | smsim-side reco does not depend on anaroot reco, so this is decoupled |

## 10. Implementation order (rough phases)

1. **Geometry + SimData chain**. Land NEBULAPlus G4 module, CSV reader,
   converter, ROOT dict. End-state: `sim_deuteron` runs, output tree has
   the new branches, VRML shows the bars. Tests:
   `test_NEBULAPlusSimParameterReader`, `test_NEBULAPlusGeometry`,
   `test_NEBULAPlusSimDataConverter`,
   `test_sim_deuteron_with_nebula_plus.sh`,
   `test_vis_with_nebula_plus.sh`.

2. **Reco refactor**. Extract `NEBULABaseReco`, rename
   `NEBULAReco` to `NEBULAReco`, sweep call sites. Delete the
   old files. CI grep. Tests:
   `test_NEBULABaseReco_refactor`, `test_NEBULAReco_legacy_compat`.

3. **NEBULA-Plus / joint reco**. Add `NEBULAPlusReco` and
   `NebulaJointReco`. Wire user-facing macros. Tests:
   `test_NEBULAPlusReco_basic`, `test_NebulaJointReco_combined`,
   `test_NebulaJointReco_efficiency`.

The three phases are independent enough that each can be its own PR /
commit set, but they share a single design (this document).

## 11. Open questions deferred to implementation

- Exact value of NEBULA-Plus `TimeReso` once LPC commissioning numbers
  are available. Default keeps NEBULA's 0.240 ns.
- Whether to expose `R_cluster` and `Δt_cluster` of the clustering step
  to G4 messenger or only to the Reco class setters. Currently the
  latter.
- Persistence of `RecoNeutron` candidates back into a tree: not done
  by default; user macro choice.

## 12. References

- `docs/nebula_and_nebula_plus.tex` (2026-05-15 physics + paths
  companion)
- `kondo_smsimulator/docs/papers/NEBULA-Plus_Kondo_RIKEN_APR2022.pdf`
  (Orr/Gibelin/Kondo/Otsu, RIKEN APR 56 (2023) p.91)
- `kondo_smsimulator/docs/papers/NEBULA-Plus_review_Tanaka2024_arxiv2412.17887.pdf`
  (Tanaka et al., arXiv:2412.17887)
- `/home/s053/exp/exp2301_s053/nptool/plugin/nebula-plus/` on
  `ribfana04` (LPC-Caen reference geometry + reco)
- Kondo, Tomai, Nakamura, NIM B 463 (2020) 173–178 (NEBULA ε at 200 MeV)
