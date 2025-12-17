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

protected:
  G4Material* fVacuumMaterial;
  G4ThreeVector fPosition;
};

#endif

