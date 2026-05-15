#include "NEBULAPlusConstructionMessenger.hh"
#include "NEBULAPlusConstruction.hh"

#include "NEBULAPlusSimParameterReader.hh"
#include "NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh"

#include "SimDataManager.hh"
#include "SimDataInitializer.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithABool.hh"

//____________________________________________________________________
NEBULAPlusConstructionMessenger::NEBULAPlusConstructionMessenger()
{
  std::cout<<"NEBULAPlusConstructionMessenger"<<std::endl;

  fNEBULAPlusDirectory = new G4UIdirectory("/samurai/geometry/NEBULAPlus/");
  fNEBULAPlusDirectory->SetGuidance("Modification Commands for NEBULAPlus");

  fParameterFileNameCmd = new G4UIcmdWithAString("/samurai/geometry/NEBULAPlus/ParameterFileName",this);
  fParameterFileNameCmd->SetGuidance("Parameter file name for NEBULAPlus.");
  fParameterFileNameCmd->SetParameterName("ParaNEBULAPlus",false);

  fDetectorParameterFileNameCmd = new G4UIcmdWithAString("/samurai/geometry/NEBULAPlus/DetectorParameterFileName",this);
  fDetectorParameterFileNameCmd->SetGuidance("Parameter file name for Neutron Detectors.");
  fDetectorParameterFileNameCmd->SetParameterName("ParaNEBULAPlus",false);

  fNEBULAPlusStoreStepsCmd = new G4UIcmdWithABool("/action/data/NEBULAPlus/StoreSteps",this);
  fNEBULAPlusStoreStepsCmd->SetGuidance("Store Steps in NEBULAPlus or not");
  fNEBULAPlusStoreStepsCmd->SetGuidance("false (default)");
  fNEBULAPlusStoreStepsCmd->SetDefaultValue(false);
  fNEBULAPlusStoreStepsCmd->SetParameterName("StoreSteps",true);

  fNEBULAPlusResolutionCmd = new G4UIcmdWithABool("/action/data/NEBULAPlus/Resolution",this);
  fNEBULAPlusResolutionCmd->SetGuidance("Convolute NEBULAPlus data by resolution or not");
  fNEBULAPlusResolutionCmd->SetGuidance("true (default)");
  fNEBULAPlusResolutionCmd->SetDefaultValue(true);
  fNEBULAPlusResolutionCmd->SetParameterName("Resolution",true);
}
//____________________________________________________________________
NEBULAPlusConstructionMessenger::~NEBULAPlusConstructionMessenger()
{
  delete fParameterFileNameCmd;
  delete fDetectorParameterFileNameCmd;
  delete fNEBULAPlusStoreStepsCmd;
  delete fNEBULAPlusResolutionCmd;
  delete fNEBULAPlusDirectory;
}
//____________________________________________________________________
void NEBULAPlusConstructionMessenger::SetNewValue(G4UIcommand* command, G4String  newValue )
{
  if( command == fParameterFileNameCmd ){
    NEBULAPlusSimParameterReader reader;
    reader.ReadNEBULAPlusParameters(newValue.data());

  }else  if( command == fDetectorParameterFileNameCmd ){
    NEBULAPlusSimParameterReader reader;
    reader.ReadNEBULAPlusDetectorParameters(newValue.data());

  }else if( command == fNEBULAPlusStoreStepsCmd ){
    SimDataManager *sman = SimDataManager::GetSimDataManager();
    SimDataInitializer *initializer = sman->FindInitializer("NEBULAPlusSimDataInitializer");
    initializer->SetDataStore(fNEBULAPlusStoreStepsCmd->GetNewBoolValue(newValue));

  }else if( command == fNEBULAPlusResolutionCmd ){
    SimDataManager *sman = SimDataManager::GetSimDataManager();
    NEBULAPlusSimDataConverter_TArtNEBULAPlusPla *converter
      = (NEBULAPlusSimDataConverter_TArtNEBULAPlusPla*)sman->FindConverter("NEBULAPlusSimDataConverter_TArtNEBULAPlusPla");
    if (converter!=0){
      converter->SetIncludeResolution(fNEBULAPlusResolutionCmd->GetNewBoolValue(newValue));
    }else{
      G4cout<<__FILE__
	    <<": NEBULAPlusSimDataConverter_TArtNEBULAPlusPla is not defined, skip"<<G4endl;
    }
  }
}
//____________________________________________________________________
