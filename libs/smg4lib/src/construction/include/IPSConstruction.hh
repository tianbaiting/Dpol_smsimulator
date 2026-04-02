#ifndef IPSCONSTRUCTION_HH
#define IPSCONSTRUCTION_HH

#include "G4Transform3D.hh"
#include "G4VUserDetectorConstruction.hh"

class G4LogicalVolume;
class G4Material;

class IPSConstruction : public G4VUserDetectorConstruction
{
public:
  IPSConstruction();
  virtual ~IPSConstruction();

  G4VPhysicalVolume* Construct() override;
  void ConstructSub();
  void PlaceModules(G4LogicalVolume* mother, const G4Transform3D& barrelTransform);
  G4LogicalVolume* GetActiveLogicalVolume() const { return fLogicActive; }

  static constexpr int kModuleCount = 32;
  static constexpr double kModuleWidthMm = 20.0;
  static constexpr double kModuleThicknessMm = 20.0;
  static constexpr double kModuleLengthMm = 200.0;
  static constexpr double kRadiusMm = 135.0;

private:
  G4Material* fVacuumMaterial;
  G4Material* fScintillatorMaterial;
  G4LogicalVolume* fLogicModule;
  G4LogicalVolume* fLogicActive;
};

#endif
