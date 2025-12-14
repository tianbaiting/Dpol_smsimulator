#ifndef EXITWINDOWNCONSTRUCTION_HH
#define EXITWINDOWNCONSTRUCTION_HH

#include "G4VUserDetectorConstruction.hh"
#include "G4ThreeVector.hh"

class G4LogicalVolume;
class G4Material;
class DetectorConstruction;

class ExitWindowNConstruction : public G4VUserDetectorConstruction
{
  friend class DetectorConstruction;
  
public:
  ExitWindowNConstruction();
  virtual ~ExitWindowNConstruction();

  G4VPhysicalVolume* Construct();
  virtual G4LogicalVolume* ConstructSub();

  G4ThreeVector GetExitWindowNSize(){return fExitWindowNSize;}
  G4double GetAngle(){return fAngle;}
  void SetAngle(G4double val){fAngle=val;}
  G4ThreeVector GetPosition(){return fPosition;}
  void SetPosition(G4ThreeVector val){fPosition = val;}

  void PutExitWindow(G4LogicalVolume* expHall_log);

  G4LogicalVolume *GetWindowVolume(){return fExitWindow_log;}

protected:
  G4ThreeVector fExitWindowNSize;

  G4LogicalVolume* fExitWindow_log;
  G4LogicalVolume* fFlange_log;

  G4double fAngle;
  G4ThreeVector fPosition;

  G4Material* fWorldMaterial;
  G4Material* fNeutronWindowMaterial;
  G4Material* fVacuumMaterial;

  G4double fWindow_flange_dx;
  G4double fWindow_flange_dy;
  G4double fWindow_flange_dz;
  G4double fWindow_hole_dx;
  G4double fWindow_hole_dy;
  G4double fWindow_hole_dz;
};

#endif

