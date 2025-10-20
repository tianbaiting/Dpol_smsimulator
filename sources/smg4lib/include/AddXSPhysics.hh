#ifndef ADDXSPHYSICS_HH
#define ADDXSPHYSICS_HH

#include "G4VPhysicsConstructor.hh"

#include "G4HadronicProcessType.hh"

class G4VCrossSectionDataSet;
class G4ParticleDefinition;

class AddXSPhysics : public G4VPhysicsConstructor
{
public: 
  AddXSPhysics(G4int ver = 0, G4double min=0, G4double max=100000*CLHEP::GeV);
  // obsolete
  AddXSPhysics(const G4String& name , 
	       G4int ver = 0, G4bool hp = false, 
	       const G4String& type="");
  virtual ~AddXSPhysics();

  // This method will be invoked in the Construct() method. 
  // each particle type will be instantiated
  virtual void ConstructParticle();
 
  // This method will be invoked in the Construct() method.
  // each physics process will be instantiated and
  // registered to the process manager of each particle type 
  virtual void ConstructProcess();

  void AddXSection(const G4ParticleDefinition* part,
		   G4VCrossSectionDataSet* cross,
		   G4HadronicProcessType subtype) ;
  //  void AddNeutronHPPhysics();

private:
  G4int    verbose;
  G4bool   wasActivated;

  G4double theMin,theMax;
};

#endif
