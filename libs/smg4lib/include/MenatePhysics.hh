#ifndef MENATEPHYSICS_HH
#define MENATEPHYSICS_HH

#include "globals.hh"
#include "G4VPhysicsConstructor.hh"

// G4HadronElasticPhysics.cc and DMXPhysicsList::AddTransportation is refered

class MenatePhysics : public G4VPhysicsConstructor
{
public: 
  MenatePhysics(G4int ver = 0, G4String calcMeth="ORIGINAL");
  // obsolete
  MenatePhysics(const G4String& name , 
		G4int ver = 0, G4bool hp = false, 
		const G4String& type="");
  virtual ~MenatePhysics();

  // This method will be invoked in the Construct() method. 
  // each particle type will be instantiated
  virtual void ConstructParticle();
 
  // This method will be invoked in the Construct() method.
  // each physics process will be instantiated and
  // registered to the process manager of each particle type 
  virtual void ConstructProcess();

private:
  G4int    verbose;
  G4bool   wasActivated;

  G4String fCalcMeth;
};

#endif
