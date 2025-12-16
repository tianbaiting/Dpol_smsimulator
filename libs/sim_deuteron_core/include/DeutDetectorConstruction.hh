#ifndef DeutDetectorConstruction_H
#define DeutDetectorConstruction_H 1

class G4LogicalVolume;
class G4VPhysicalVolume;
class DeutDetectorConstructionMessenger;
class DipoleConstruction;
class PDCConstruction;
class NEBULAConstruction;
class ExitWindowNConstruction;
class ExitWindowC2Construction;

class G4VSensitiveDetector;
class G4VModularPhysicsList;


#include "G4VUserDetectorConstruction.hh"
#include "G4ThreeVector.hh"
#include "G4String.hh"

class DeutDetectorConstruction : public G4VUserDetectorConstruction
{
public:

  DeutDetectorConstruction();
  ~DeutDetectorConstruction();
  G4VPhysicalVolume* GetPhysicalWorld() const { return physiWorld; }
  void SetPDCAngle(G4double angle);
  void SetPDC1Pos(G4ThreeVector pos);
  void SetPDC2Pos(G4ThreeVector pos);
  void SetDumpAngle(G4double angle);
  void SetDumpPos(G4ThreeVector pos);
  void SetTargetAngle(G4double angle);
  void SetTargetPos(G4ThreeVector pos);
  void UpdateGeometry();
  void AutoConfigGeometry(G4String outputMacroFile);

  inline void SetInputMacroFile(G4String inputMacroFile) 
  { fInputMacroFile = inputMacroFile; }
  
  inline void SetModularPhysicsList(G4VModularPhysicsList* phyList)
  { fModularPhysicsList = phyList; }

  G4VPhysicalVolume* Construct() override;

  void SetFillAir(G4bool tf){fFillAir = tf;}

  void SetTarget(G4bool tf){fSetTarget = tf;}
  void SetTargetMat(G4String mat){fTargetMat = mat;}
  void SetTargetSize(G4ThreeVector size){fTargetSize = size;}
  void SetDump(G4bool tf){fSetDump = tf;}
               
private:
  G4VPhysicalVolume* physiWorld;  // 添加成员变量
  G4bool fFillAir;
  G4bool fSetTarget;
  G4bool fSetDump;

  DeutDetectorConstructionMessenger *fDetectorConstructionMessenger;

  DipoleConstruction *fDipoleConstruction;
  PDCConstruction    *fPDCConstruction;
  NEBULAConstruction *fNEBULAConstruction;

  G4String      fTargetMat;
  G4ThreeVector fTargetPos;   // position at laboratory coordinate in mm
  G4ThreeVector fTargetSize;
  G4double      fTargetAngle; // angle (clockwise) in rad

  G4double      fPDCAngle;    // angle (clockwise) in rad
  G4ThreeVector fPDC1Pos;     // position at rotated coordinate in mm
  G4ThreeVector fPDC2Pos;     // position at rotated coordinate in mm

  G4double      fDumpAngle;   // angle (clockwise) in rad
  G4ThreeVector fDumpPos;     // position at rotated coordinate in mm

  G4VSensitiveDetector* fTargetSD = 0; // reserved for track reconstruction
  G4VSensitiveDetector* fPDCSD_U = 0;
  G4VSensitiveDetector* fPDCSD_X = 0;
  G4VSensitiveDetector* fPDCSD_V = 0;
  G4VSensitiveDetector* fNEBULASD = 0;

  ExitWindowNConstruction *fExitWindowNConstruction;  
  G4VSensitiveDetector* fNeutronWinSD;

  ExitWindowC2Construction *fExitWindowC2Construction;  
  G4VSensitiveDetector* fWindowHoleSD;

  // VacuumUpstreamConstruction *fVacuumUpstreamConstruction;
  // VacuumDownstreamConstruction *fVacuumDownstreamConstruction;
  
  G4String fInputMacroFile;
  G4VModularPhysicsList* fModularPhysicsList;
};

#endif

