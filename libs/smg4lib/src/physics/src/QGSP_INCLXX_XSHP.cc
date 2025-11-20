
#include "QGSP_INCLXX_XSHP.hh"

#include "G4EmStandardPhysics.hh"
#include "G4EmExtraPhysics.hh"
#include "G4DecayPhysics.hh"
#include "G4HadronElasticPhysicsHP.hh"
//#include "HadronPhysicsQGSP_INCLXX_HP.hh"//Geant4.9
#include "QGSP_INCLXX_HP.hh"//Geant4.10
//#include "G4QStoppingPhysics.hh"
#include "G4StoppingPhysics.hh"
//#include "G4IonInclAblaPhysics.hh"
#include "G4IonINCLXXPhysics.hh"
#include "AddXSPhysics.hh"
#include "SAMURAICutsPhysics.hh"

//#include "G4DataQuestionaire.hh" // Geant4.10.6
#include "G4SystemOfUnits.hh"// Geant4.10

QGSP_INCLXX_XSHP::QGSP_INCLXX_XSHP(G4int ver, const G4String&) : G4VModularPhysicsList()
{
  //G4DataQuestionaire it(photon, neutron, neutronxs); //G4.10.6
  G4cout << "<<< Geant4 Physics List simulation engine: QGSP_INCLXX_XSHP"<< G4endl;
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
  RegisterPhysics( new G4HadronElasticPhysicsHP(ver) );

  // Hadron Physics
  //RegisterPhysics( new HadronPhysicsQGSP_INCLXX_HP(ver));
  //RegisterPhysics( new G4HadronPhysicsINCLXX(ver));
  //                                         name,quasiElastic,neutronHP,ftfp                   
  RegisterPhysics( new G4HadronPhysicsINCLXX("hInelastic INCLXX",true,true,false));

  // Stopping Physics
  //  RegisterPhysics( new G4QStoppingPhysics(ver) );
  RegisterPhysics( new G4StoppingPhysics(ver) );

  // Ion Physics
  //  RegisterPhysics( new G4IonInclAblaPhysics(ver)); // used with INCLXX
  RegisterPhysics( new G4IonINCLXXPhysics(ver));

  // Add XS data set
  RegisterPhysics( new AddXSPhysics(ver));

  // SAMURAICutsPhysics
  RegisterPhysics( new SAMURAICutsPhysics(ver) );
}

QGSP_INCLXX_XSHP::~QGSP_INCLXX_XSHP()
{;}

void QGSP_INCLXX_XSHP::SetCuts()
{
  SetCutsWithDefault();   
}
