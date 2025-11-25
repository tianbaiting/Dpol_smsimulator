#include "DeutDetectorConstructionMessenger.hh"
#include "DeutDetectorConstruction.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWith3VectorAndUnit.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4SystemOfUnits.hh"//Geant4.10

DeutDetectorConstructionMessenger::DeutDetectorConstructionMessenger(DeutDetectorConstruction* Det)
 : fDetectorConstruction(Det)
{
  fGeometryDirectory = new G4UIdirectory("/samurai/geometry/");
  fGeometryDirectory->SetGuidance("Modification Commands for SAMURAI experiment");

  fUpdateGeometryCmd = new G4UIcmdWithoutParameter("/samurai/geometry/Update",this);
  fUpdateGeometryCmd->SetGuidance("Update SAMURAI Geometry");

  fAutoConfigGeometryCmd = new G4UIcmdWithAString("/samurai/geometry/AutoConfig",this);
  fAutoConfigGeometryCmd->SetGuidance("Configure SAMURAI Geometry Automatically");
  fAutoConfigGeometryCmd->SetParameterName("MacroName", true);
  fAutoConfigGeometryCmd->SetDefaultValue("macro/geom_autoconfig.mac");

  fFillAirCmd = new G4UIcmdWithABool("/samurai/geometry/FillAir",this);
  fFillAirCmd->SetGuidance("Fill Air or Vacuum for Experimental room");
  fFillAirCmd->SetParameterName("FillAir",true);
  fFillAirCmd->SetDefaultValue(false);

  fTargetDirectory = new G4UIdirectory("/samurai/geometry/Target/");
  fTargetDirectory->SetGuidance("Modification Commands for the Target");

  fSetTargetCmd = new G4UIcmdWithABool("/samurai/geometry/Target/SetTarget",this);
  fSetTargetCmd->SetGuidance("Define Target");
  fSetTargetCmd->SetParameterName("SetTarget",true);
  fSetTargetCmd->SetDefaultValue(false);

  fTargetMatCmd = new G4UIcmdWithAString("/samurai/geometry/Target/Material",this);
  fTargetMatCmd->SetGuidance("Set Material of Target (CsI/Sn/empty)");
  fTargetMatCmd->SetGuidance("  CsI (default)");
  fTargetMatCmd->SetParameterName("TargetMaterial",true);
  fTargetMatCmd->SetDefaultValue("CsI");

  fTargetSizeCmd = new G4UIcmdWith3VectorAndUnit("/samurai/geometry/Target/Size",this);
  fTargetSizeCmd->SetGuidance("Set Size of Target");
  fTargetSizeCmd->SetGuidance("  40 40 3*mm (default)");
  fTargetSizeCmd->SetUnitCategory("Length");
  fTargetSizeCmd->SetParameterName("x","y","z",true);
  fTargetSizeCmd->SetDefaultValue(G4ThreeVector(40,40,3*mm));
  fTargetSizeCmd->AvailableForStates(G4State_Idle);

  fTargetPosCmd = new G4UIcmdWith3VectorAndUnit("/samurai/geometry/Target/Position",this);
  fTargetPosCmd->SetGuidance("Set Position of Target");
  fTargetPosCmd->SetGuidance("  -65 0 -615*mm (default)");
  fTargetPosCmd->SetUnitCategory("Length");
  fTargetPosCmd->SetParameterName("x","y","z",true);
  fTargetPosCmd->SetDefaultValue(G4ThreeVector(-65,0,615*mm));
  fTargetPosCmd->AvailableForStates(G4State_Idle);

  fTargetAngleCmd = new G4UIcmdWithADoubleAndUnit("/samurai/geometry/Target/Angle",this);
  fTargetAngleCmd->SetGuidance("Set Orientation of Target");
  fTargetAngleCmd->SetGuidance("  10 deg (default)");
  fTargetAngleCmd->SetUnitCategory("Angle");
  fTargetAngleCmd->SetParameterName("TargetAngle",true);
  fTargetAngleCmd->SetDefaultValue(10*deg);
  fTargetAngleCmd->AvailableForStates(G4State_Idle);

  fPDCDirectory = new G4UIdirectory("/samurai/geometry/PDC/");
  fPDCDirectory->SetGuidance("Modification Commands for PDCs");

  fPDCAngleCmd = new G4UIcmdWithADoubleAndUnit("/samurai/geometry/PDC/Angle",this);
  fPDCAngleCmd->SetGuidance("Set Angle of PDCs");
  fPDCAngleCmd->SetGuidance("  57 deg (default)");
  fPDCAngleCmd->SetUnitCategory("Angle");
  fPDCAngleCmd->SetParameterName("PDCAngle",true);
  fPDCAngleCmd->SetDefaultValue(57*deg);
  fPDCAngleCmd->AvailableForStates(G4State_Idle);

  fPDC1PosCmd = new G4UIcmdWith3VectorAndUnit("/samurai/geometry/PDC/Position1",this);
  fPDC1PosCmd->SetGuidance("Set Position of PDC1");
  fPDC1PosCmd->SetGuidance("  400 0 4100*mm (default)");
  fPDC1PosCmd->SetUnitCategory("Length");
  fPDC1PosCmd->SetParameterName("x","y","z",true);
  fPDC1PosCmd->SetDefaultValue(G4ThreeVector(400,0,4100*mm));
  fPDC1PosCmd->AvailableForStates(G4State_Idle);

  fPDC2PosCmd = new G4UIcmdWith3VectorAndUnit("/samurai/geometry/PDC/Position2",this);
  fPDC2PosCmd->SetGuidance("Set Position of PDC2");
  fPDC2PosCmd->SetGuidance("  400 0 5100*mm (default)");
  fPDC2PosCmd->SetUnitCategory("Length");
  fPDC2PosCmd->SetParameterName("x","y","z",true);
  fPDC2PosCmd->SetDefaultValue(G4ThreeVector(400,0,5100*mm));
  fPDC2PosCmd->AvailableForStates(G4State_Idle);

  fDumpDirectory = new G4UIdirectory("/samurai/geometry/Dump/");
  fDumpDirectory->SetGuidance("Modification Commands for the Dump");

  fSetDumpCmd = new G4UIcmdWithABool("/samurai/geometry/Dump/SetDump",this);
  fSetDumpCmd->SetGuidance("Define Dump");
  fSetDumpCmd->SetParameterName("SetDump",true);
  fSetDumpCmd->SetDefaultValue(false);

  fDumpAngleCmd = new G4UIcmdWithADoubleAndUnit("/samurai/geometry/Dump/Angle",this);
  fDumpAngleCmd->SetGuidance("Set Angle of the Beam Dump");
  fDumpAngleCmd->SetGuidance("  39.3 deg (default)");
  fDumpAngleCmd->SetUnitCategory("Angle");
  fDumpAngleCmd->SetParameterName("DumpAngle",true);
  fDumpAngleCmd->SetDefaultValue(39.3*deg);
  fDumpAngleCmd->AvailableForStates(G4State_Idle);

  fDumpPosCmd = new G4UIcmdWith3VectorAndUnit("/samurai/geometry/Dump/Position",this);
  fDumpPosCmd->SetGuidance("Set Position of the Beam Dump");
  fDumpPosCmd->SetGuidance("  0 -250 10000*mm (default)");
  fDumpPosCmd->SetUnitCategory("Length");
  fDumpPosCmd->SetParameterName("x","y","z",true);
  fDumpPosCmd->SetDefaultValue(G4ThreeVector(0,-25*cm,10*m));
  fDumpPosCmd->AvailableForStates(G4State_Idle);
}

DeutDetectorConstructionMessenger::~DeutDetectorConstructionMessenger()
{
  delete fGeometryDirectory;
  delete fUpdateGeometryCmd;
  delete fAutoConfigGeometryCmd;
  delete fFillAirCmd;

  delete fTargetDirectory;
  delete fSetTargetCmd;
  delete fTargetMatCmd;
  delete fTargetSizeCmd;
  delete fTargetPosCmd;
  delete fTargetAngleCmd;

  delete fPDCDirectory;
  delete fPDCAngleCmd;
  delete fPDC1PosCmd;
  delete fPDC2PosCmd;

  delete fDumpDirectory;
  delete fDumpAngleCmd;
  delete fDumpPosCmd;
}

void DeutDetectorConstructionMessenger::SetNewValue(G4UIcommand* command, 
							G4String  newValue)
{
  if       ( command == fUpdateGeometryCmd ){
    fDetectorConstruction->UpdateGeometry();

  }else if ( command == fAutoConfigGeometryCmd){
    fDetectorConstruction->AutoConfigGeometry(newValue);
  
  }else if ( command == fFillAirCmd ){
    fDetectorConstruction->SetFillAir(fFillAirCmd->GetNewBoolValue(newValue));

  }else if ( command == fSetTargetCmd ){
    fDetectorConstruction->SetTarget(fSetTargetCmd->GetNewBoolValue(newValue));

  }else if ( command == fTargetMatCmd ){
    fDetectorConstruction->SetTargetMat(newValue);

  }else if ( command == fTargetSizeCmd ){
    fDetectorConstruction->SetTargetSize(fTargetSizeCmd->GetNew3VectorValue(newValue));

  }else if ( command == fTargetPosCmd ){
    fDetectorConstruction->SetTargetPos(fTargetPosCmd->GetNew3VectorValue(newValue));

  }else if ( command == fTargetAngleCmd ){
    fDetectorConstruction->SetTargetAngle(fTargetAngleCmd->GetNewDoubleValue(newValue));

  }else if ( command == fPDCAngleCmd ){
    fDetectorConstruction->SetPDCAngle(fPDCAngleCmd->GetNewDoubleValue(newValue));

  }else if ( command == fPDC1PosCmd ){
    fDetectorConstruction->SetPDC1Pos(fPDC1PosCmd->GetNew3VectorValue(newValue));

  }else if ( command == fPDC2PosCmd ){
    fDetectorConstruction->SetPDC2Pos(fPDC2PosCmd->GetNew3VectorValue(newValue));

  }else if ( command == fSetDumpCmd ){
    fDetectorConstruction->SetDump(fSetDumpCmd->GetNewBoolValue(newValue));

  }else if ( command == fDumpAngleCmd ){
    fDetectorConstruction->SetDumpAngle(fDumpAngleCmd->GetNewDoubleValue(newValue));
  
  }else if ( command == fDumpPosCmd ){
    fDetectorConstruction->SetDumpPos(fDumpPosCmd->GetNew3VectorValue(newValue));
  }
}
