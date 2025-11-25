#ifndef G4PHYSLISTFACTORYSAMURAI_HH
#define G4PHYSLISTFACTORYSAMURAI_HH

#include "globals.hh"

#include <vector>

class G4VModularPhysicsList;
class PhysicsListBasicMessenger;

// origin is G4PhysListFactorySAMURAI, but not a copy or inheritance

class G4PhysListFactorySAMURAI
{
public:
  G4PhysListFactorySAMURAI();
  ~G4PhysListFactorySAMURAI();

  G4VModularPhysicsList* GetReferencePhysList(const G4String&);
  inline void SetVerbose(G4int val){verbose = val;}

protected:
  std::vector<G4String> listnames_em;
  G4int verbose;
  PhysicsListBasicMessenger* fPhysicsListBasicMessenger;  

};

#endif



