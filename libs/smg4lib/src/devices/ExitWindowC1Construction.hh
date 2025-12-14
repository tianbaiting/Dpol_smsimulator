#ifndef EXITWINDOWC1CONSTRUCTION_HH
#define EXITWINDOWC1CONSTRUCTION_HH

#include "G4VUserDetectorConstruction.hh"
#include "G4ThreeVector.hh"

class G4LogicalVolume;
class G4Material;
class DetectorConstruction;

class ExitWindowC1Construction : public G4VUserDetectorConstruction
{
  friend class DetectorConstruction;
  
public:
  ExitWindowC1Construction();
  virtual ~ExitWindowC1Construction();

  G4VPhysicalVolume* Construct();
  virtual G4LogicalVolume* ConstructSub();

  G4double GetAngle(){return fAngle;}
  void SetAngle(G4double val){fAngle=val;}
  G4ThreeVector GetPosition(){return fPosition;}
  void SetPosition(G4ThreeVector val){fPosition = val;}

  G4LogicalVolume *GetWindowHoleVolume(){return fWindowHole_log;}

protected:
  G4LogicalVolume* fLogicExitWindowC1;

  G4double fAngle;
  G4ThreeVector fPosition;

  G4Material* fWorldMaterial;
  G4Material* fFlangeMaterial;
  G4Material* fVacuumMaterial;

  G4double fWindow_flange_dx;
  G4double fWindow_flange_dy;
  G4double fWindow_flange_dz;
  G4double fWindow_hole_dx;
  G4double fWindow_hole_dy;
  G4double fWindow_hole_dz;

  G4LogicalVolume* fWindowHole_log;
};

#endif

