#include "PhysicsListBasicMessenger.hh"
#include "G4VUserPhysicsList.hh"
#include "G4UIdirectory.hh"
#include "G4UIcommand.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4String.hh"
#include "G4Gamma.hh"
#include "G4Electron.hh"
#include "G4Positron.hh"
#include "G4SystemOfUnits.hh"
//____________________________________________________________________
PhysicsListBasicMessenger::PhysicsListBasicMessenger()
  : fPhysicsList(0)
{
  fActionCutDir = new G4UIdirectory("/action/cut/");
  fActionCutDir->SetGuidance(" Set Cut Action Parameters.");

  fCutGammaCmd = new G4UIcmdWithADoubleAndUnit("/action/cut/gamma",this);
  fCutGammaCmd->SetGuidance("Set cut value for gamma");
  fCutGammaCmd->SetParameterName("cutgamma",false);
  fCutGammaCmd->SetUnitCategory("Length");
  fCutGammaCmd->SetRange("cutgamma>0.0");
  fCutGammaCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fCutElectronCmd = new G4UIcmdWithADoubleAndUnit("/action/cut/electron",this);
  fCutElectronCmd->SetGuidance("Set cut value for electron");
  fCutElectronCmd->SetParameterName("cutelectron",false);
  fCutElectronCmd->SetUnitCategory("Length");
  fCutElectronCmd->SetRange("cutelectron>0.0");
  fCutElectronCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fCutPositronCmd = new G4UIcmdWithADoubleAndUnit("/action/cut/positron",this);
  fCutPositronCmd->SetGuidance("Set cut value for positron");
  fCutPositronCmd->SetParameterName("cutpositron",false);
  fCutPositronCmd->SetUnitCategory("Length");
  fCutPositronCmd->SetRange("cutpositron>0.0");
  fCutPositronCmd->AvailableForStates(G4State_PreInit,G4State_Idle);
}
//____________________________________________________________________
PhysicsListBasicMessenger::~PhysicsListBasicMessenger()
{
  delete fCutGammaCmd;
  delete fCutElectronCmd;
  delete fCutPositronCmd;
  delete fActionCutDir;
}
//____________________________________________________________________
void PhysicsListBasicMessenger::Register(G4VUserPhysicsList* physicsList)
{
  fPhysicsList = physicsList;
}
//____________________________________________________________________
void PhysicsListBasicMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  if(command == fCutGammaCmd){
    fPhysicsList->SetCutValue(fCutGammaCmd->GetNewDoubleValue(newValue),"gamma");
    G4cout<<"Cutvalue for gamma = "<<fPhysicsList->GetCutValue("gamma")/mm<<" mm"<<G4endl;

  }else if(command == fCutElectronCmd){
    fPhysicsList->SetCutValue(fCutElectronCmd->GetNewDoubleValue(newValue),"e-");
    G4cout<<"Cutvalue for e- = "<<fPhysicsList->GetCutValue("e-")/mm<<" mm"<<G4endl;

  }else if(command == fCutPositronCmd){
    fPhysicsList->SetCutValue(fCutPositronCmd->GetNewDoubleValue(newValue),"e+");
    G4cout<<"Cutvalue for e+ = "<<fPhysicsList->GetCutValue("e+")/mm<<" mm"<<G4endl;
  }

}
//____________________________________________________________________

