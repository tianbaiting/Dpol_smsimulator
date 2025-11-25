
#include "MenatePhysics.hh"

#include "G4ParticleDefinition.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4VProcess.hh"

#include "menate_R.hh"

MenatePhysics::MenatePhysics(G4int ver, G4String calcMeth)
  : G4VPhysicsConstructor("MenatePhysics"), verbose(ver), 
    wasActivated(false)
{
  if(verbose > 1) { 
    G4cout << "### MenatePhysics: " << GetPhysicsName() 
	   << G4endl; 
  }

  fCalcMeth = calcMeth;
  //  fCalcMeth = "ORIGINAL";
  // fCalcMeth = "carbon_inel_only";
}

MenatePhysics::MenatePhysics(const G4String&,
    G4int ver, G4bool, const G4String&)
  : G4VPhysicsConstructor("MenatePhysics"), verbose(ver), 
    wasActivated(false)
{
  if(verbose > 1) { 
    G4cout << "### MenatePhysics: " << GetPhysicsName() 
	   << G4endl; 
  }

  fCalcMeth = "ORIGINAL";
}

MenatePhysics::~MenatePhysics()
{}

void MenatePhysics::ConstructParticle()
{
  G4Geantino::GeantinoDefinition();
  G4ChargedGeantino::ChargedGeantinoDefinition();
  G4Neutron::NeutronDefinition();
}

void MenatePhysics::ConstructProcess()
{
  if(wasActivated) { return; }
  wasActivated = true;

  //  G4cout << "MenatePhysics::ConstructProcess" << G4endl;

  G4ParticleTable::G4PTblDicIterator* theParticleIterator = GetParticleIterator();// for Geant4.10
  theParticleIterator->reset();
  while( (*theParticleIterator)() ){
    G4ParticleDefinition* particle = theParticleIterator->value();
    G4ProcessManager* pmanager = particle->GetProcessManager();
    G4String particleName = particle->GetParticleName();
 
    if(particleName == "neutron"){
      // delete other process
      G4ProcessVector* pv = particle->GetProcessManager()->GetProcessList();
      //      for(int i=0; i<pv->size(); ++i) std::cout << (*pv)[i]->GetProcessName() << std::endl;
      if(fCalcMeth == "ORIGINAL"){ // remove all neutron physics except transportation
	while(pv->size() > 1){
	  if((*pv)[0]->GetProcessName() == "Transportation"){
	    pmanager->RemoveProcess(1);
	  }else{
	    pmanager->RemoveProcess(0);
	  }
	}
      }else if(fCalcMeth == "carbon_inel_only"){ // remove only neutron inelastic
	int n = pv->size();
	for(int i=0; i<n; ++i){
	  if((*pv)[i]->GetProcessName() == "NeutronInelastic"){
	    pmanager->RemoveProcess(i);
	    break;
	  }
	}
      }else{
	G4Exception(0,0,FatalException,0);
      }

      //      for(int i=0; i<pv->size(); ++i) std::cout << (*pv)[i]->GetProcessName() << std::endl;

      G4String NeutronProcessName = "MENATE_R";
      //      G4String NeutronProcessName = "NeutronInelastic";
      menate_R* theMenate = new menate_R(NeutronProcessName);
      theMenate->SetMeanFreePathCalcMethod(fCalcMeth);
      pmanager->AddDiscreteProcess(theMenate); // add menate process
    }
  }
}
