#ifndef EXITWINDOWC2CONSTRUCTION_HH
#define EXITWINDOWC2CONSTRUCTION_HH

#include "G4VUserDetectorConstruction.hh"
#include "G4ThreeVector.hh"

class G4LogicalVolume;
class G4Material;
class DetectorConstruction;

class ExitWindowC2Construction : public G4VUserDetectorConstruction
{
  friend class DetectorConstruction;
  
public:
  ExitWindowC2Construction();
  virtual ~ExitWindowC2Construction();

  G4VPhysicalVolume* Construct();
  virtual G4LogicalVolume* ConstructSub();

  G4double GetAngle(){return fAngle;}
  void SetAngle(G4double val){fAngle=val;}
  G4ThreeVector GetPosition(){return fPosition;}
  void SetPosition(G4ThreeVector val){fPosition = val;}
  void PutExitWindow(G4LogicalVolume *expHall_log);

  G4LogicalVolume *GetWindowHoleVolume(){return fWindowHole_log;}
  G4VPhysicalVolume *GetWindowHolePhys(){return fWindowHole_phys;}

protected:
  G4LogicalVolume* fLogicExitWindowC2;

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

  G4double fWindow_tubeholder_dx;
  G4double fWindow_tubeholder_dy;
  G4double fWindow_tubeholder_dz;

  G4LogicalVolume* fWindowFlange_log;
  G4LogicalVolume* fWindowHole_log;
  G4VPhysicalVolume* fWindowHole_phys;
};

#endif

