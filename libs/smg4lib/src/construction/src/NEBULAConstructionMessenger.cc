#include "NEBULAConstructionMessenger.hh"
#include "NEBULAConstruction.hh"

#include "NEBULASimParameterReader.hh"
#include "NEBULASimDataConverter_TArtNEBULAPla.hh"

#include "SimDataManager.hh"
#include "SimDataInitializer.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithABool.hh"

//____________________________________________________________________
NEBULAConstructionMessenger::NEBULAConstructionMessenger()
{
  std::cout<<"NEBULAConstructionMessenger"<<std::endl;

  fNEBULADirectory = new G4UIdirectory("/samurai/geometry/NEBULA/");
  fNEBULADirectory->SetGuidance("Modification Commands for NEBULA");

  fParameterFileNameCmd = new G4UIcmdWithAString("/samurai/geometry/NEBULA/ParameterFileName",this);
  fParameterFileNameCmd->SetGuidance("Parameter file name for NEBULA.");
  fParameterFileNameCmd->SetParameterName("ParaNEBULA",false);

  fDetectorParameterFileNameCmd = new G4UIcmdWithAString("/samurai/geometry/NEBULA/DetectorParameterFileName",this);
  fDetectorParameterFileNameCmd->SetGuidance("Parameter file name for Neutron Detectors.");
  fDetectorParameterFileNameCmd->SetParameterName("ParaNEBULA",false);

  fNEBULAStoreStepsCmd = new G4UIcmdWithABool("/action/data/NEBULA/StoreSteps",this);
  fNEBULAStoreStepsCmd->SetGuidance("Store Steps in NEBULA or not");
  fNEBULAStoreStepsCmd->SetGuidance("false (default)");
  fNEBULAStoreStepsCmd->SetDefaultValue(false);
  fNEBULAStoreStepsCmd->SetParameterName("StoreSteps",true);

  fNEBULAResolutionCmd = new G4UIcmdWithABool("/action/data/NEBULA/Resolution",this);
  fNEBULAResolutionCmd->SetGuidance("Convolute NEBULA data by resolution or not");
  fNEBULAResolutionCmd->SetGuidance("true (default)");
  fNEBULAResolutionCmd->SetDefaultValue(true);
  fNEBULAResolutionCmd->SetParameterName("Resolution",true);
}
//____________________________________________________________________
NEBULAConstructionMessenger::~NEBULAConstructionMessenger()
{
  delete fParameterFileNameCmd;
  delete fDetectorParameterFileNameCmd;
  delete fNEBULAStoreStepsCmd;
  delete fNEBULAResolutionCmd;
  delete fNEBULADirectory;
}
//____________________________________________________________________
void NEBULAConstructionMessenger::SetNewValue(G4UIcommand* command, G4String  newValue )
{
  if( command == fParameterFileNameCmd ){
    NEBULASimParameterReader reader;
    reader.ReadNEBULAParameters(newValue.data());

  }else  if( command == fDetectorParameterFileNameCmd ){
    NEBULASimParameterReader reader;
    reader.ReadNEBULADetectorParameters(newValue.data());

  }else if( command == fNEBULAStoreStepsCmd ){
    SimDataManager *sman = SimDataManager::GetSimDataManager();
    SimDataInitializer *initializer = sman->FindInitializer("NEBULASimDataInitializer");
    initializer->SetDataStore(fNEBULAStoreStepsCmd->GetNewBoolValue(newValue));

  }else if( command == fNEBULAResolutionCmd ){
    SimDataManager *sman = SimDataManager::GetSimDataManager();
    NEBULASimDataConverter_TArtNEBULAPla *converter 
      = (NEBULASimDataConverter_TArtNEBULAPla*)sman->FindConverter("NEBULASimDataConverter_TArtNEBULAPla");
    if (converter!=0){
      converter->SetIncludeResolution(fNEBULAResolutionCmd->GetNewBoolValue(newValue));
    }else{
      G4cout<<__FILE__
	    <<": NEBULASimDataConverter_TArtNEBULAPla is not defined, skip"<<G4endl;
    }
  }
}
//____________________________________________________________________
