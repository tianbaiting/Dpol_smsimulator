#ifndef NEBULACONSTRUCTION_HH
#define NEBULACONSTRUCTION_HH

#include "G4VUserDetectorConstruction.hh"
#include <TString.h>
#include <vector>
#include "G4ThreeVector.hh"

class G4LogicalVolume;
class G4Material;
class G4VSensitiveDetector;
class DetectorConstruction;
class NEBULAConstructionMessenger;
class TNEBULASimParameter;


class NEBULAConstruction : public G4VUserDetectorConstruction
{
public:
  NEBULAConstruction();
  virtual ~NEBULAConstruction();

  G4VPhysicalVolume* Construct();
  G4LogicalVolume* ConstructSub();
  G4LogicalVolume* PutNEBULA(G4LogicalVolume* ExpHall_log);
  G4LogicalVolume* GetLogicNeut(){return fLogicNeut;}
  G4LogicalVolume* GetLogicVeto(){return fLogicVeto;}

  G4ThreeVector GetPosition(){return fPosition;}

  bool DoesNeutExist(){return fNeutExist;}
  bool DoesVetoExist(){return fVetoExist;}

protected:
  NEBULAConstructionMessenger* fNEBULAConstructionMessenger;

  TNEBULASimParameter *fNEBULASimParameter;

  G4ThreeVector fPosition;

  G4LogicalVolume* fLogicNeut;
  G4LogicalVolume* fLogicVeto;

  G4Material* fVacuumMaterial;
  G4Material* fNeutMaterial;

  bool fNeutExist;
  bool fVetoExist;
  G4VSensitiveDetector* fNEBULASD;


};

#endif

