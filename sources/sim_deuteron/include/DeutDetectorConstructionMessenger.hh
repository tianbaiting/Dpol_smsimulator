#ifndef DeutDetectorCONSTRUCTIONNEBULAMESSENGER_HH
#define DeutDetectorCONSTRUCTIONNEBULAMESSENGER_HH

#include "G4UImessenger.hh"

class DeutDetectorConstruction;
class G4UIdirectory;
class G4UIcmdWithoutParameter;
class G4UIcmdWithABool;
class G4UIcmdWithAString;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWith3VectorAndUnit;

class DeutDetectorConstructionMessenger : public G4UImessenger
{
public:
  DeutDetectorConstructionMessenger(DeutDetectorConstruction*);
  virtual ~DeutDetectorConstructionMessenger();

  void SetNewValue(G4UIcommand*, G4String);

protected:
  DeutDetectorConstruction* fDetectorConstruction;

  G4UIdirectory* fGeometryDirectory;
  G4UIcmdWithoutParameter* fUpdateGeometryCmd;
  G4UIcmdWithAString* fAutoConfigGeometryCmd;
  G4UIcmdWithABool* fFillAirCmd;

  G4UIdirectory* fTargetDirectory;
  G4UIcmdWithABool* fSetTargetCmd;
  G4UIcmdWithAString* fTargetMatCmd;
  G4UIcmdWith3VectorAndUnit* fTargetSizeCmd;
  G4UIcmdWith3VectorAndUnit* fTargetPosCmd;
  G4UIcmdWithADoubleAndUnit* fTargetAngleCmd;

  G4UIdirectory* fPDCDirectory;
  G4UIcmdWithADoubleAndUnit* fPDCAngleCmd;
  G4UIcmdWith3VectorAndUnit* fPDC1PosCmd;
  G4UIcmdWith3VectorAndUnit* fPDC2PosCmd;

  G4UIdirectory* fDumpDirectory;
  G4UIcmdWithABool* fSetDumpCmd;
  G4UIcmdWithADoubleAndUnit* fDumpAngleCmd;
  G4UIcmdWith3VectorAndUnit* fDumpPosCmd;

};

#endif
