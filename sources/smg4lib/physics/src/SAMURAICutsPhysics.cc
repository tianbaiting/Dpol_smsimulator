
#include "SAMURAICutsPhysics.hh"

#include "G4ParticleDefinition.hh"
#include "G4ProcessManager.hh"

#include "SAMURAIMaxTimeCuts.hh"
#include "SAMURAIMinEkineCuts.hh"
#include "G4StepLimiter.hh"

// time cut for particle > 1 microsec
// energy cut is not used 

SAMURAICutsPhysics::SAMURAICutsPhysics(G4int ver)
  : G4VPhysicsConstructor("SAMURAICutsPhysics"), verbose(ver), 
    wasActivated(false)
{
  if(verbose > 1) { 
    G4cout << "### SAMURAICutsPhysics: " << GetPhysicsName() 
	   << G4endl; 
  }
}

SAMURAICutsPhysics::SAMURAICutsPhysics(const G4String&,
				       G4int ver, G4bool, const G4String&)
  : G4VPhysicsConstructor("SAMURAICutsPhysics"), verbose(ver), 
    wasActivated(false)
{
  if(verbose > 1) { 
    G4cout << "### SAMURAICutsPhysics: " << GetPhysicsName() 
	   << G4endl; 
  }
}

SAMURAICutsPhysics::~SAMURAICutsPhysics()
{}

void SAMURAICutsPhysics::ConstructParticle()
{}

void SAMURAICutsPhysics::ConstructProcess()
{
  if(wasActivated) { return; }
  wasActivated = true;

  //  G4cout << "SAMURAICutsPhysics::ConstructProcess" << G4endl;

  // added for Geant4.10
  G4ParticleTable::G4PTblDicIterator* theParticleIterator = GetParticleIterator();

  theParticleIterator->reset();
  while( (*theParticleIterator)() ){
    G4ParticleDefinition* particle = theParticleIterator->value();
    G4ProcessManager* pmanager = particle->GetProcessManager();
    G4String particleName = particle->GetParticleName();

    // time cuts for ALL particles
    pmanager->AddDiscreteProcess(new SAMURAIMaxTimeCuts());

    // Energy cuts for low energy particles
    // not well checked
    //    pmanager->AddDiscreteProcess(new SAMURAIMinEkineCuts());
 
    // Step limit applied to all particles
    // (no meaning ??)
    pmanager->AddProcess(new G4StepLimiter, -1,-1,1);
  }
}
