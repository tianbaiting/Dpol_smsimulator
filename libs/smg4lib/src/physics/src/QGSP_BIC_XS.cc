#include "QGSP_BIC_XS.hh"

#include "G4EmStandardPhysics.hh"
#include "G4EmExtraPhysics.hh"
#include "G4DecayPhysics.hh"
#include "G4HadronElasticPhysics.hh"
//#include "HadronPhysicsQGSP_BIC.hh"// for Geant4.9
#include "G4HadronPhysicsQGSP_BIC.hh"// for Geant4.10
//#include "G4QStoppingPhysics.hh"
#include "G4StoppingPhysics.hh"
//#include "G4IonBinaryCascadePhysics.hh"
#include "G4IonPhysics.hh"
#include "AddXSPhysics.hh"
#include "SAMURAICutsPhysics.hh"

//#include "G4DataQuestionaire.hh" // Geant4.10.6
#include "G4SystemOfUnits.hh"

QGSP_BIC_XS::QGSP_BIC_XS(G4int ver, const G4String&) : G4VModularPhysicsList()
{
  //G4DataQuestionaire it(photon, neutron, neutronxs); // G4.10.6
  G4cout << "<<< Geant4 Physics List simulation engine: QGSP_BIC_XS"<< G4endl;
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
  //RegisterPhysics( new HadronPhysicsQGSP_BIC(ver) );// Geant4.9
  RegisterPhysics( new G4HadronPhysicsQGSP_BIC(ver) );//Geant4.10

  // Stopping Physics
  //  RegisterPhysics( new G4QStoppingPhysics(ver) );
  RegisterPhysics( new G4StoppingPhysics(ver) );

  // Ion Physics
  //  RegisterPhysics( new G4IonBinaryCascadePhysics(ver));
  RegisterPhysics( new G4IonPhysics(ver));

  // Add XS data set
  RegisterPhysics( new AddXSPhysics(ver));

  // SAMURAICutsPhysics
  RegisterPhysics( new SAMURAICutsPhysics(ver) );
}

QGSP_BIC_XS::~QGSP_BIC_XS()
{;}

void QGSP_BIC_XS::SetCuts()
{
  SetCutsWithDefault();   
}
