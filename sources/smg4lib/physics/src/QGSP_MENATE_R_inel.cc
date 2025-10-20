
#include "QGSP_MENATE_R_inel.hh"

#include "G4EmStandardPhysics.hh"
#include "G4EmExtraPhysics.hh"
#include "G4DecayPhysics.hh"
#include "G4HadronElasticPhysics.hh"
//#include "HadronPhysicsQGSP_INCLXX.hh"//Geant4.9
#include "G4HadronPhysicsINCLXX.hh"//Geant4.10
//#include "G4QStoppingPhysics.hh"
#include "G4StoppingPhysics.hh"
//#include "G4IonInclAblaPhysics.hh"
#include "G4IonINCLXXPhysics.hh"
#include "SAMURAICutsPhysics.hh"
#include "MenatePhysics.hh"

//#include "G4DataQuestionaire.hh" // Geant4.10.6
#include "G4SystemOfUnits.hh"

QGSP_MENATE_R_inel::QGSP_MENATE_R_inel(G4int ver, const G4String&) : G4VModularPhysicsList()
{
  //G4DataQuestionaire it(photon, neutron, neutronxs); // G4.10.6
  G4cout << "<<< Geant4 Physics List simulation engine: QGSP_MENATE_R_inel"<< G4endl;
  G4cout << G4endl;

  defaultCutValue = 1*mm;  
  SetVerboseLevel(ver);

  // EM Physics
  RegisterPhysics( new G4EmStandardPhysics(ver) );

  // Synchroton Radiation & GN Physics
  RegisterPhysics( new G4EmExtraPhysics(ver) );

  // Decays
  RegisterPhysics( new G4DecayPhysics(ver) );

  // Hadron Elastic scattering
  RegisterPhysics( new G4HadronElasticPhysics(ver) );

  // Hadron Physics
  //RegisterPhysics( new HadronPhysicsQGSP_INCLXX(ver));
  RegisterPhysics( new G4HadronPhysicsINCLXX(ver));

  // Stopping Physics
  //  RegisterPhysics( new G4QStoppingPhysics(ver) );
  RegisterPhysics( new G4StoppingPhysics(ver) );

  // Ion Physics
  //  RegisterPhysics( new G4IonInclAblaPhysics(ver)); // used with INCLXX
  RegisterPhysics( new G4IonINCLXXPhysics(ver));

  // MENATE_R_inel
  RegisterPhysics( new MenatePhysics(ver, "carbon_inel_only") );

  // SAMURAICutsPhysics
  RegisterPhysics( new SAMURAICutsPhysics(ver) );
}

QGSP_MENATE_R_inel::~QGSP_MENATE_R_inel()
{;}

void QGSP_MENATE_R_inel::SetCuts()
{
  SetCutsWithDefault();   
}
