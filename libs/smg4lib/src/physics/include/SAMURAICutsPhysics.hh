#ifndef SAMURAICUTSPHYSICS_HH
#define SAMURAICUTSPHYSICS_HH

#include "globals.hh"
#include "G4VPhysicsConstructor.hh"

// G4HadronElasticPhysics.cc and DMXPhysicsList::AddTransportation is refered

class SAMURAICutsPhysics : public G4VPhysicsConstructor
{
public: 
  SAMURAICutsPhysics(G4int ver = 0); 
  // obsolete
  SAMURAICutsPhysics(const G4String& name , 
		     G4int ver = 0, G4bool hp = false, 
		     const G4String& type="");
  virtual ~SAMURAICutsPhysics();

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
};

#endif
