#include "BeamSimDataMessenger.hh"
#include "SimDataManager.hh"

#include "BeamSimDataInitializer.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithADouble.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWith3VectorAndUnit.hh"
#include "G4UIcmdWithoutParameter.hh"

//____________________________________________________________________
BeamSimDataMessenger::BeamSimDataMessenger()
{
  fActionBeamDataDir = new G4UIdirectory("/action/data/Beam/");
  fActionBeamDataDir->SetGuidance(" Set BeamData Action Parameters.");

  fStoreBeamDataCmd = new G4UIcmdWithABool("/action/data/Beam/StoreData",this);
  fStoreBeamDataCmd->SetGuidance("Store BeamData or not");
  fStoreBeamDataCmd->SetGuidance("true (default)");
  fStoreBeamDataCmd->SetParameterName("StoreBeamData",true);
  fStoreBeamDataCmd->SetDefaultValue(true);

}
//____________________________________________________________________
BeamSimDataMessenger::~BeamSimDataMessenger()
{
  delete fStoreBeamDataCmd;
  delete fActionBeamDataDir;
}
//____________________________________________________________________
void BeamSimDataMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  if( command == fStoreBeamDataCmd ){
    SimDataManager *sm = SimDataManager::GetSimDataManager();
    BeamSimDataInitializer *initializer 
      = (BeamSimDataInitializer*)sm->FindInitializer("BeamSimDataInitializer");
    initializer->SetDataStore(fStoreBeamDataCmd->GetNewBoolValue(newValue));
  }

}
//____________________________________________________________________

