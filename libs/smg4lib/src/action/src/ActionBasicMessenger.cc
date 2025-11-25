#include "ActionBasicMessenger.hh"
#include "PrimaryGeneratorActionBasic.hh"
#include "SimDataManager.hh"
#include "TRunSimParameter.hh"

#include "SimDataInitializer.hh"
#include "NEBULASimDataConverter_TArtNEBULAPla.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithADouble.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWith3VectorAndUnit.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4SystemOfUnits.hh"//Geant4.10

ActionBasicMessenger* ActionBasicMessenger::fActionBasicMessenger(0);
//____________________________________________________________________
ActionBasicMessenger* ActionBasicMessenger::GetInstance()
{
  if(fActionBasicMessenger == 0){
    fActionBasicMessenger = new ActionBasicMessenger;
  }
  return fActionBasicMessenger;
}
//____________________________________________________________________
ActionBasicMessenger::ActionBasicMessenger()
  : fPrimaryGeneratorActionBasic(0)
{
  //fOutputFileParameter = OutputFileParameter::GetInstance();

  fActionDir = new G4UIdirectory("/action/");
  fActionDir->SetGuidance(" Set Action Parameters.");

  fActionFileDir = new G4UIdirectory("/action/file/");
  fActionFileDir->SetGuidance(" Set File Action Parameters.");

  fActionGunDir = new G4UIdirectory("/action/gun/");
  fActionGunDir->SetGuidance(" Set File Action Parameters.");

  fActionTreeDir = new G4UIdirectory("/action/gun/tree/");
  fActionTreeDir->SetGuidance(" Set Action Parameters for Tree Input.");
  
  fActionDataDir = new G4UIdirectory("/action/data/");
  fActionDataDir->SetGuidance(" Set Data Action Parameters.");

  fRunNameCmd = new G4UIcmdWithAString("/action/file/RunName",this);
  fRunNameCmd->SetGuidance("Set run (output file) name");
  fRunNameCmd->SetParameterName("RunName",false);
  fRunNameCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fOutputSaveDirCmd = new G4UIcmdWithAString("/action/file/SaveDirectory",this);
  fOutputSaveDirCmd->SetGuidance("Set directory for output file");
  fOutputSaveDirCmd->SetParameterName("SaveDir",false);
  fOutputSaveDirCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fOverWriteCmd = new G4UIcmdWithAString("/action/file/OverWrite",this);
  fOverWriteCmd->SetGuidance("OverWrite ROOT file [y/n/ask]");
  fOverWriteCmd->SetParameterName("overWrite",false);
  fOverWriteCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fOutputTreeNameCmd = new G4UIcmdWithAString("/action/file/tree/TreeName",this);
  fOutputTreeNameCmd->SetGuidance("Set tree name");
  fOutputTreeNameCmd->SetParameterName("treeName",false);
  fOutputTreeNameCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fOutputTreeTitleCmd = new G4UIcmdWithAString("/action/file/tree/TreeTitle",this);
  fOutputTreeTitleCmd->SetGuidance("Set tree title");
  fOutputTreeTitleCmd->SetParameterName("treeTitle",false);
  fOutputTreeTitleCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamTypeCmd = new G4UIcmdWithAString("/action/gun/Type",this);
  fBeamTypeCmd->SetGuidance("Set beam type");
  fBeamTypeCmd->SetParameterName("beamType",false);
  fBeamTypeCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamACmd = new G4UIcmdWithAnInteger("/action/gun/A",this);
  fBeamACmd->SetGuidance("Set mass number of beam");
  fBeamACmd->SetGuidance("  24 (default)");
  fBeamACmd->SetParameterName("BeamA",true);
  fBeamACmd->SetDefaultValue(24);
  fBeamACmd->AvailableForStates(G4State_Idle);

  fBeamZCmd = new G4UIcmdWithAnInteger("/action/gun/Z",this);
  fBeamZCmd->SetGuidance("Set atomic number of beam");
  fBeamZCmd->SetGuidance("  8 (default)");
  fBeamZCmd->SetParameterName("BeamZ",true);
  fBeamZCmd->SetDefaultValue(8);
  fBeamZCmd->AvailableForStates(G4State_Idle);

  fSetBeamParticleNameCmd = new G4UIcmdWithAString("/action/gun/SetBeamParticleName",this);
  fSetBeamParticleNameCmd->SetGuidance("Set beam particle from name");
  fSetBeamParticleNameCmd->SetParameterName("ParticleName",false);
  fSetBeamParticleNameCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fSetBeamParticleCmd = new G4UIcmdWithoutParameter("/action/gun/SetBeamParticle",this);
  fSetBeamParticleCmd->SetGuidance("Set beam particle from defined A and Z");

  fBeamEnergyCmd = new G4UIcmdWithADoubleAndUnit("/action/gun/Energy",this);
  fBeamEnergyCmd->SetGuidance("Set beam energy per nucleon");
  fBeamEnergyCmd->SetParameterName("beamEnergy",false);
  fBeamEnergyCmd->SetUnitCategory("Energy");
  fBeamEnergyCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamBrhoCmd = new G4UIcmdWithADouble("/action/gun/Brho",this);
  fBeamBrhoCmd->SetGuidance("Set beam brho in Tm");
  fBeamBrhoCmd->SetParameterName("beamBrho",false);
  fBeamBrhoCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamPositionCmd = new G4UIcmdWith3VectorAndUnit("/action/gun/Position",this);
  fBeamPositionCmd->SetGuidance("Set beam position");
  fBeamPositionCmd->SetGuidance("  0 0 -400*cm (default)");
  fBeamPositionCmd->SetUnitCategory("Length");
  fBeamPositionCmd->SetParameterName("x","y","z",true);
  fBeamPositionCmd->SetDefaultValue(G4ThreeVector(0,0,-400*cm));
  fBeamPositionCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamPositionXSigmaCmd = new G4UIcmdWithADoubleAndUnit("/action/gun/PositionXSigma",this);
  fBeamPositionXSigmaCmd->SetGuidance("Set beam sigma of Xpos");
  fBeamPositionXSigmaCmd->SetParameterName("beamPosXsigma",false);
  fBeamPositionXSigmaCmd->SetUnitCategory("Length");
  fBeamPositionXSigmaCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamPositionYSigmaCmd = new G4UIcmdWithADoubleAndUnit("/action/gun/PositionYSigma",this);
  fBeamPositionYSigmaCmd->SetGuidance("Set beam sigma of Ypos");
  fBeamPositionYSigmaCmd->SetParameterName("beamPosYsigma",false);
  fBeamPositionYSigmaCmd->SetUnitCategory("Length");
  fBeamPositionYSigmaCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamAngleXCmd = new G4UIcmdWithADoubleAndUnit("/action/gun/AngleX",this);
  fBeamAngleXCmd->SetGuidance("Set beam Xang");
  fBeamAngleXCmd->SetParameterName("beamAngX",false);
  fBeamAngleXCmd->SetUnitCategory("Angle");
  fBeamAngleXCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamAngleYCmd = new G4UIcmdWithADoubleAndUnit("/action/gun/AngleY",this);
  fBeamAngleYCmd->SetGuidance("Set beam Yang");
  fBeamAngleYCmd->SetParameterName("beamAngY",false);
  fBeamAngleYCmd->SetUnitCategory("Angle");
  fBeamAngleYCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamAngleXSigmaCmd = new G4UIcmdWithADoubleAndUnit("/action/gun/AngleXSigma",this);
  fBeamAngleXSigmaCmd->SetGuidance("Set beam sigma of Xang");
  fBeamAngleXSigmaCmd->SetParameterName("beamAngXsigma",false);
  fBeamAngleXSigmaCmd->SetUnitCategory("Angle");
  fBeamAngleXSigmaCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fBeamAngleYSigmaCmd = new G4UIcmdWithADoubleAndUnit("/action/gun/AngleYSigma",this);
  fBeamAngleYSigmaCmd->SetGuidance("Set beam sigma of Yang");
  fBeamAngleYSigmaCmd->SetParameterName("beamAngYsigma",false);
  fBeamAngleYSigmaCmd->SetUnitCategory("Angle");
  fBeamAngleYSigmaCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  // for simulation using tree input
  fInputFileNameCmd = new G4UIcmdWithAString("/action/gun/tree/InputFileName",this);
  fInputFileNameCmd->SetGuidance("Set filename of Tree input.");
  fInputFileNameCmd->SetParameterName("filename",true);
  fInputFileNameCmd->SetDefaultValue("");
  fInputFileNameCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fInputTreeNameCmd = new G4UIcmdWithAString("/action/gun/tree/TreeName",this);
  fInputTreeNameCmd->SetGuidance("Set Name of input tree.");
  fInputTreeNameCmd->SetParameterName("treename",true);
  fInputTreeNameCmd->SetDefaultValue("tree_input");
  fInputTreeNameCmd->AvailableForStates(G4State_PreInit,G4State_Idle);

  fTreeBeamOnCmd = new G4UIcmdWithAnInteger("/action/gun/tree/beamOn",this);
  fTreeBeamOnCmd->SetGuidance("Start run for Tree input");
  fTreeBeamOnCmd->SetGuidance("  0 (default)");
  fTreeBeamOnCmd->SetGuidance("  0 (0:n_event = Entires of Tree, >0 n_event = input number)");
  fTreeBeamOnCmd->SetParameterName("NumOfBeam",true);
  fTreeBeamOnCmd->SetDefaultValue(0);
  fTreeBeamOnCmd->AvailableForStates(G4State_Idle);

  fSkipNeutronCmd = new G4UIcmdWithABool("/action/gun/SkipNeutron",this);
  fSkipNeutronCmd->SetGuidance("Skip Neutron simulation or not");
  fSkipNeutronCmd->SetGuidance("false (default)");
  fSkipNeutronCmd->SetParameterName("SkipNeutron",true);
  fSkipNeutronCmd->SetDefaultValue(false);

  fSkipGammaCmd = new G4UIcmdWithABool("/action/gun/SkipGamma",this);
  fSkipGammaCmd->SetGuidance("Skip Gamma simulation or not");
  fSkipGammaCmd->SetGuidance("false (default)");
  fSkipGammaCmd->SetParameterName("SkipGamma",true);
  fSkipGammaCmd->SetDefaultValue(false);

  fSkipHeavyIonCmd = new G4UIcmdWithABool("/action/gun/SkipHeavyIon",this);
  fSkipHeavyIonCmd->SetGuidance("Skip HeavyIon simulation or not");
  fSkipHeavyIonCmd->SetGuidance("false (default)");
  fSkipHeavyIonCmd->SetParameterName("SkipHeavyIon",true);
  fSkipHeavyIonCmd->SetDefaultValue(false);

  fSkipLargeYAngCmd = new G4UIcmdWithABool("/action/gun/SkipLargeYAng",this);
  fSkipLargeYAngCmd->SetGuidance("Skip to generate the particle if Y angle is large");
  fSkipLargeYAngCmd->SetGuidance("false (default)");
  fSkipLargeYAngCmd->SetParameterName("SkipLargeYAng",true);
  fSkipLargeYAngCmd->SetDefaultValue(false);


}
//____________________________________________________________________
ActionBasicMessenger::~ActionBasicMessenger()
{
  delete fRunNameCmd;
  delete fOutputSaveDirCmd;
  delete fOverWriteCmd;
  delete fOutputTreeNameCmd;
  delete fOutputTreeTitleCmd;
  delete fBeamTypeCmd;
  delete fBeamACmd;
  delete fBeamZCmd;
  delete fBeamEnergyCmd;
  delete fBeamPositionCmd;
  delete fBeamPositionXSigmaCmd;
  delete fBeamPositionYSigmaCmd;
  delete fBeamAngleXCmd;
  delete fBeamAngleYCmd;
  delete fBeamAngleXSigmaCmd;
  delete fBeamAngleYSigmaCmd;
  delete fInputFileNameCmd;
  delete fInputTreeNameCmd;
  delete fTreeBeamOnCmd;
  delete fSkipNeutronCmd;
  delete fSkipGammaCmd;
  delete fSkipHeavyIonCmd;

  delete fSkipLargeYAngCmd;

  delete fActionDir;
  delete fActionFileDir;
  delete fActionGunDir;
  delete fActionTreeDir;
  delete fActionDataDir;

  fActionBasicMessenger = 0;
}
//____________________________________________________________________
void ActionBasicMessenger::Register(PrimaryGeneratorActionBasic* primaryGeneratorActionBasic)
{
  fPrimaryGeneratorActionBasic = primaryGeneratorActionBasic;
}
//____________________________________________________________________
void ActionBasicMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  if(command == fRunNameCmd){
    SimDataManager *sman = SimDataManager::GetSimDataManager();    
    TRunSimParameter *prm = (TRunSimParameter*)sman->FindParameter("RunParameter");
    if (prm==0){
      prm = new TRunSimParameter("RunParameter");
      sman->AddParameter(prm);
    }
    prm->fRunName = newValue;

  }else if(command == fOutputSaveDirCmd){
    SimDataManager *sman = SimDataManager::GetSimDataManager();    
    TRunSimParameter *prm = (TRunSimParameter*)sman->FindParameter("RunParameter");
    if (prm==0){
      prm = new TRunSimParameter("RunParameter");
      sman->AddParameter(prm);
    }
    prm->fSaveDir = newValue;
    if ( (prm->fSaveDir.Last('/')+1) != prm->fSaveDir.Length() ){
      prm->fSaveDir += "/";
    }

  }else if(command == fOverWriteCmd){
    SimDataManager *sman = SimDataManager::GetSimDataManager();    
    TRunSimParameter *prm = (TRunSimParameter*)sman->FindParameter("RunParameter");
    if (prm==0){
      prm = new TRunSimParameter("RunParameter");
      sman->AddParameter(prm);
    }
    prm->fOverWrite = newValue;

  }else if(command == fOutputTreeNameCmd){
    SimDataManager *sman = SimDataManager::GetSimDataManager();    
    TRunSimParameter *prm = (TRunSimParameter*)sman->FindParameter("RunParameter");
    if (prm==0){
      prm = new TRunSimParameter("RunParameter");
      sman->AddParameter(prm);
    }
    prm->fTreeName = newValue;
  }else if(command == fOutputTreeTitleCmd){
    SimDataManager *sman = SimDataManager::GetSimDataManager();    
    TRunSimParameter *prm = (TRunSimParameter*)sman->FindParameter("RunParameter");
    if (prm==0){
      prm = new TRunSimParameter("RunParameter");
      sman->AddParameter(prm);
    }
    prm->fTreeTitle = newValue;


  }else if(command == fBeamTypeCmd){
    if(!fPrimaryGeneratorActionBasic->SetBeamType(newValue)) G4Exception(0, 0, FatalException, 0);

  }else if(command == fBeamACmd){
    if(!fPrimaryGeneratorActionBasic->SetBeamA(fBeamACmd->GetNewIntValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
    }
  }else if(command == fBeamZCmd){
    if(!fPrimaryGeneratorActionBasic->SetBeamZ(fBeamZCmd->GetNewIntValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
    }

  }else if(command == fSetBeamParticleNameCmd){
    if(!fPrimaryGeneratorActionBasic->SetBeamParticle(newValue))
      G4Exception(0, 0, FatalException, 0);

  }else if(command == fSetBeamParticleCmd){
    if(!fPrimaryGeneratorActionBasic->SetBeamParticle())
      G4Exception(0, 0, FatalException, 0);

  }else if(command == fBeamEnergyCmd){
    if(!fPrimaryGeneratorActionBasic->SetBeamEnergy(fBeamEnergyCmd->GetNewDoubleValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
    }

  }else if(command == fBeamBrhoCmd){
    if(!fPrimaryGeneratorActionBasic->SetBeamBrho(fBeamBrhoCmd->GetNewDoubleValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
    }

  }else if(command == fBeamPositionCmd){
    if(!fPrimaryGeneratorActionBasic->SetBeamPosition(fBeamPositionCmd->GetNew3VectorValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
    }
  }else if(command == fBeamPositionXSigmaCmd){
   if(!fPrimaryGeneratorActionBasic->SetBeamPositionXSigma(fBeamPositionXSigmaCmd->GetNewDoubleValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
   }
  }else if(command == fBeamPositionYSigmaCmd){
   if(!fPrimaryGeneratorActionBasic->SetBeamPositionYSigma(fBeamPositionYSigmaCmd->GetNewDoubleValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
   }

  }else if(command == fBeamAngleXCmd){
   if(!fPrimaryGeneratorActionBasic->SetBeamAngleX(fBeamAngleXCmd->GetNewDoubleValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
   }
  }else if(command == fBeamAngleYCmd){
   if(!fPrimaryGeneratorActionBasic->SetBeamAngleY(fBeamAngleYCmd->GetNewDoubleValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
   }

  }else if(command == fBeamAngleXSigmaCmd){
   if(!fPrimaryGeneratorActionBasic->SetBeamAngleXSigma(fBeamAngleXSigmaCmd->GetNewDoubleValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
   }
  }else if(command == fBeamAngleYSigmaCmd){
   if(!fPrimaryGeneratorActionBasic->SetBeamAngleYSigma(fBeamAngleYSigmaCmd->GetNewDoubleValue(newValue))){
      G4Exception(0, 0, FatalException, 0);
   }

  }else if( command == fInputFileNameCmd ) { 
    fPrimaryGeneratorActionBasic->SetInputTreeFileName(newValue);
  }else if( command == fInputTreeNameCmd ) { 
    fPrimaryGeneratorActionBasic->SetInputTreeName(newValue);

  }else if( command == fTreeBeamOnCmd ){
    G4int NumOfBeam = fTreeBeamOnCmd->GetNewIntValue(newValue);
    fPrimaryGeneratorActionBasic->TreeBeamOn(NumOfBeam);

  }else if( command == fSkipNeutronCmd ){
    fPrimaryGeneratorActionBasic->SetSkipNeutron(fSkipNeutronCmd->GetNewBoolValue(newValue));

  }else if( command == fSkipGammaCmd ){
    fPrimaryGeneratorActionBasic->SetSkipGamma(fSkipGammaCmd->GetNewBoolValue(newValue));

  }else if( command == fSkipHeavyIonCmd ){
    fPrimaryGeneratorActionBasic->SetSkipHeavyIon(fSkipHeavyIonCmd->GetNewBoolValue(newValue));

  }else if( command == fSkipLargeYAngCmd ){
    fPrimaryGeneratorActionBasic->SetSkipLargeYAng(fSkipLargeYAngCmd->GetNewBoolValue(newValue));


  }


}
//____________________________________________________________________

