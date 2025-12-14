#include "DipoleConstructionMessenger.hh"
#include "DipoleConstruction.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithADouble.hh"
#include "G4UIcmdWithoutParameter.hh"
//#include "G4UIcmdWithABool.hh"

DipoleConstructionMessenger::DipoleConstructionMessenger(DipoleConstruction* Det)
 : fDipoleConstruction(Det)
{

  fDipoleDirectory = new G4UIdirectory("/samurai/geometry/Dipole/");
  fDipoleDirectory->SetGuidance("Modification Commands for Dipole");

  fDipoleAngleCmd = new G4UIcmdWithADoubleAndUnit("/samurai/geometry/Dipole/Angle",this);
  fDipoleAngleCmd->SetGuidance("Set angle of Dipole");
  fDipoleAngleCmd->SetGuidance("  0*deg (default)");
  fDipoleAngleCmd->SetUnitCategory("Angle");
  fDipoleAngleCmd->SetDefaultValue(0);
  fDipoleAngleCmd->AvailableForStates(G4State_Idle);

  fDipoleFieldFileCmd = new G4UIcmdWithAString("/samurai/geometry/Dipole/FieldFile",this);
  fDipoleFieldFileCmd->SetGuidance("Set input file of SAMURAI magnetic Field");
  fDipoleFieldFileCmd->SetDefaultValue("");
  fDipoleFieldFileCmd->AvailableForStates(G4State_Idle);

  fDipoleFieldFactorCmd = new G4UIcmdWithADouble("/samurai/geometry/Dipole/FieldFactor",this);
  fDipoleFieldFactorCmd->SetGuidance("Set field factor of the SAMURAI magnet");
  fDipoleFieldFactorCmd->SetGuidance("  1.0 (default)");
  fDipoleFieldFactorCmd->SetParameterName("MagFieldFactor",true);
  fDipoleFieldFactorCmd->SetDefaultValue(1.0);
  fDipoleFieldFactorCmd->AvailableForStates(G4State_Idle);

  fDipolePlotFieldCmd = new G4UIcmdWithoutParameter("/samurai/geometry/Dipole/PlotMagField",this);
  fDipolePlotFieldCmd->SetGuidance("Plot SAMURAI magnetic Field");
  fDipolePlotFieldCmd->AvailableForStates(G4State_Idle);

}

DipoleConstructionMessenger::~DipoleConstructionMessenger()
{
  delete fDipoleDirectory;

  delete fDipoleAngleCmd;
  delete fDipoleFieldFileCmd;
  delete fDipoleFieldFactorCmd;
  delete fDipolePlotFieldCmd;
}

void DipoleConstructionMessenger::SetNewValue(G4UIcommand* command, G4String  newValue )
{
  if      ( command == fDipoleAngleCmd ){
    G4double angle = fDipoleAngleCmd->GetNewDoubleValue(newValue);
    fDipoleConstruction->SetAngle(angle);

  }else if( command == fDipoleFieldFileCmd ){
    fDipoleConstruction->SetMagField(newValue);

  }else if( command == fDipoleFieldFactorCmd ){
    G4double fac = fDipoleFieldFactorCmd->GetNewDoubleValue(newValue);
    fDipoleConstruction->SetMagFieldFactor(fac);

  }else if( command == fDipolePlotFieldCmd ){
    fDipoleConstruction->PlotMagField();

  }
}


