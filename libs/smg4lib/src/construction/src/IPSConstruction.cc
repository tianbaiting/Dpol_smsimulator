#include "IPSConstruction.hh"

#include "G4Box.hh"
#include "G4GeometryManager.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4RotationMatrix.hh"
#include "G4SolidStore.hh"
#include "G4SystemOfUnits.hh"
#include "G4Transform3D.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

}

IPSConstruction::IPSConstruction()
  : fVacuumMaterial(nullptr),
    fScintillatorMaterial(nullptr),
    fLogicModule(nullptr),
    fLogicActive(nullptr)
{
  auto* nist = G4NistManager::Instance();
  fVacuumMaterial = nist->FindOrBuildMaterial("G4_Galactic");
  fScintillatorMaterial = nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
}

IPSConstruction::~IPSConstruction()
{;}

G4VPhysicalVolume* IPSConstruction::Construct()
{
  G4GeometryManager::GetInstance()->OpenGeometry();
  G4PhysicalVolumeStore::GetInstance()->Clean();
  G4LogicalVolumeStore::GetInstance()->Clean();
  G4SolidStore::GetInstance()->Clean();

  const G4double worldHalfSize = 1.0 * m;
  auto* solidWorld = new G4Box("IPSWorld", worldHalfSize, worldHalfSize, worldHalfSize);
  auto* logicWorld = new G4LogicalVolume(solidWorld, fVacuumMaterial, "IPSWorld");
  auto* physWorld = new G4PVPlacement(nullptr, {}, logicWorld, "IPSWorld", nullptr, false, 0);
  logicWorld->SetVisAttributes(G4VisAttributes::GetInvisible());

  ConstructSub();
  PlaceModules(logicWorld, G4Transform3D());

  return physWorld;
}

void IPSConstruction::ConstructSub()
{
  const G4double halfLength = 0.5 * kModuleLengthMm * mm;
  const G4double halfWidth = 0.5 * kModuleWidthMm * mm;
  const G4double halfThickness = 0.5 * kModuleThicknessMm * mm;
  const G4double activeMargin = 0.05 * mm;

  auto* moduleSolid = new G4Box("IPSModuleSolid", halfWidth, halfThickness, halfLength);
  fLogicModule = new G4LogicalVolume(moduleSolid, fScintillatorMaterial, "IPS");

  // [EN] Shrink the active volume slightly so Geant4 does not see coincident mother-daughter boundaries at the scintillator faces. / [CN] 将敏感体略微内缩，避免Geant4把塑闪母子体共面边界识别为几何噪声。
  auto* activeSolid = new G4Box("IPSActiveSolid", halfWidth - activeMargin, halfThickness - activeMargin, halfLength - activeMargin);
  fLogicActive = new G4LogicalVolume(activeSolid, fScintillatorMaterial, "IPSActive");
  new G4PVPlacement(nullptr, {}, fLogicActive, "IPSActive", fLogicModule, false, 0, false);

  auto moduleVis = G4VisAttributes(G4Colour(0.0, 0.8, 0.2, 0.7));
  moduleVis.SetForceSolid(true);
  fLogicModule->SetVisAttributes(moduleVis);
  fLogicActive->SetVisAttributes(G4Colour(0.0, 0.6, 0.9, 0.3));
}

void IPSConstruction::PlaceModules(G4LogicalVolume* mother, const G4Transform3D& barrelTransform)
{
  for (int i = 0; i < kModuleCount; ++i) {
    const double phi = (2.0 * kPi * static_cast<double>(i)) / static_cast<double>(kModuleCount);
    auto rotation = G4RotationMatrix();
    rotation.rotateZ(phi);
    const G4ThreeVector position(
      kRadiusMm * std::cos(phi) * mm,
      kRadiusMm * std::sin(phi) * mm,
      0.0
    );
    // [EN] Rotate each bar around the barrel axis so the local transverse frame tracks the target-centered azimuth. / [CN] 每个条块绕桶轴转动，使局部横向坐标跟随以靶为中心的方位角。
    const G4Transform3D moduleTransform = barrelTransform * G4Transform3D(rotation, position);
    new G4PVPlacement(
      moduleTransform,
      fLogicModule,
      "IPSModule",
      mother,
      false,
      i,
      true
    );
  }
}
