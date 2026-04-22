#ifndef VacuumDownstreamCONSTRUCTION_HH
#define VacuumDownstreamCONSTRUCTION_HH

#include "G4VUserDetectorConstruction.hh"
#include "G4ThreeVector.hh"

class G4LogicalVolume;
class G4Material;
class DetectorConstruction;

class VacuumDownstreamConstruction : public G4VUserDetectorConstruction
{
  friend class DetectorConstruction;
  
public:
  VacuumDownstreamConstruction();
  virtual ~VacuumDownstreamConstruction();

  G4VPhysicalVolume* Construct();
  virtual G4LogicalVolume* ConstructSub(G4VPhysicalVolume *dipole_phys,
					G4VPhysicalVolume *flange_phys);

  G4ThreeVector GetPosition(){return fPosition;}

  // [EN] When true, the downstream pipe is vacuum (G4_Galactic). When false,
  // it inherits the world material (air if FillAir=true, vacuum otherwise).
  // [CN] 真时为真空；假时跟随 world 材质，保持与磁铁腔体材质联通。
  void SetBeamLineVacuum(G4bool tf){fBeamLineVacuum = tf;}

protected:
  G4Material* fVacuumMaterial;
  G4ThreeVector fPosition;
  G4bool fBeamLineVacuum = false;
};

#endif

