#ifndef NEBULAPLUSCONTRUCTION_HH
#define NEBULAPLUSCONTRUCTION_HH

#include "G4VUserDetectorConstruction.hh"
#include <TString.h>
#include <vector>
#include "G4ThreeVector.hh"

class G4LogicalVolume;
class G4Material;
class G4VSensitiveDetector;
class DetectorConstruction;
class NEBULAPlusConstructionMessenger;
class TNEBULAPlusSimParameter;


class NEBULAPlusConstruction : public G4VUserDetectorConstruction
{
public:
  NEBULAPlusConstruction();
  virtual ~NEBULAPlusConstruction();

  G4VPhysicalVolume* Construct();
  G4LogicalVolume* ConstructSub();
  G4LogicalVolume* PutNEBULAPlus(G4LogicalVolume* ExpHall_log);
  G4LogicalVolume* GetLogicNeut(){return fLogicNeut;}
  G4LogicalVolume* GetLogicVeto(){return fLogicVeto;}

  G4ThreeVector GetPosition(){return fPosition;}

  bool DoesNeutExist(){return fNeutExist;}
  bool DoesVetoExist(){return fVetoExist;}

protected:
  NEBULAPlusConstructionMessenger* fNEBULAPlusConstructionMessenger;

  TNEBULAPlusSimParameter *fNEBULAPlusSimParameter;

  G4ThreeVector fPosition;

  G4LogicalVolume* fLogicNeut;
  G4LogicalVolume* fLogicVeto;

  G4Material* fVacuumMaterial;
  G4Material* fNeutMaterial;

  bool fNeutExist;
  bool fVetoExist;
  G4VSensitiveDetector* fNEBULAPlusSD;


};

#endif
