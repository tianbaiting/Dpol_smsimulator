
#include "AddXSPhysics.hh"

#include "G4HadronicProcessType.hh"
#include "G4HadronicProcess.hh"
#include "G4ProcessManager.hh"
#include "G4VCrossSectionDataSet.hh"
#include "G4ParticleDefinition.hh"
#include "G4Neutron.hh"
#include "G4NeutronElasticXS.hh"
#include "G4NeutronInelasticXS.hh"

// almost same as G4NeutronCrossSectionXS

AddXSPhysics::AddXSPhysics(G4int ver, G4double min, G4double max)
  : G4VPhysicsConstructor("AddXSPhysics"), verbose(ver), 
    wasActivated(false)
{
  if(verbose > 1) { 
    G4cout << "### AddXSPhysics: " << GetPhysicsName() 
	   << G4endl; 
  }

  theMin = min;
  theMax = max;
}

AddXSPhysics::AddXSPhysics(const G4String&,
			   G4int ver, G4bool, const G4String&)
  : G4VPhysicsConstructor("AddXSPhysics"), verbose(ver), 
    wasActivated(false)
{
  if(verbose > 1) { 
    G4cout << "### ADDXSPhysics: " << GetPhysicsName() 
	   << G4endl; 
  }
}

AddXSPhysics::~AddXSPhysics()
{}

void AddXSPhysics::ConstructParticle()
{}

void AddXSPhysics::ConstructProcess()
{
  if(wasActivated) { return; }
  wasActivated = true;

  const G4ParticleDefinition* part = G4Neutron::Neutron();
  AddXSection(part, new G4NeutronElasticXS, fHadronElastic);
  AddXSection(part, new G4NeutronInelasticXS, fHadronInelastic);

  //  AddNeutronHPPhysics();
}

void AddXSPhysics::AddXSection(const G4ParticleDefinition* part,
			       G4VCrossSectionDataSet* cross,
			       G4HadronicProcessType subtype) 
{
  G4ProcessVector* pv = part->GetProcessManager()->GetPostStepProcessVector();
  size_t n = pv->size();
  if(0 < n) {
    for(size_t i=0; i<n; ++i) {
      if((*pv)[i]->GetProcessSubType() == subtype) {
        G4HadronicProcess* hp = static_cast<G4HadronicProcess*>((*pv)[i]);
	cross->SetMinKinEnergy(theMin);
	cross->SetMaxKinEnergy(theMax);
	hp->AddDataSet(cross);
        return;
      }
    }
  }
}

// following code doesn't work because energy range will fully overlap
// physics can fully overlap (later one will be used),
// but cross section CANNOT fully overlap (if partly overlaps, I thought that random value between two xs will be used)
//
// // G4NeutronHPBuilder
// 
// #include "G4HadronElasticProcess.hh"
// #include "G4NeutronHPElastic.hh"
// #include "G4NeutronHPElasticData.hh"
// #include "G4HadronInelasticProcess.hh"
// #include "G4NeutronHPInelastic.hh"
// #include "G4NeutronHPInelasticData.hh"
// 
// void AddXSPhysics::AddNeutronHPPhysics()
// {
//   G4double theMin=0;
//   G4double theMax=20*MeV;
// 
//   const G4ParticleDefinition* part = G4Neutron::Neutron();
//   G4ProcessVector* pv = part->GetProcessManager()->GetPostStepProcessVector();
//   size_t n = pv->size();
//   if(0 < n) {
//     for(size_t i=0; i<n; ++i) {
//       if((*pv)[i]->GetProcessSubType() == fHadronElastic) {
//         G4HadronElasticProcess* hp = static_cast<G4HadronElasticProcess*>((*pv)[i]);
// 	G4NeutronHPElastic* theHPElastic = new G4NeutronHPElastic;
// 	theHPElastic->SetMinEnergy(theMin);
// 	theHPElastic->SetMaxEnergy(theMax);
// 	G4NeutronHPElasticData* theHPElasticData = new G4NeutronHPElasticData;
// 	hp->AddDataSet(theHPElasticData);
// 	hp->RegisterMe(theHPElastic);
//       }else if((*pv)[i]->GetProcessSubType() == fHadronInelastic){
//         G4HadronInelasticProcess* hp = static_cast<G4HadronInelasticProcess*>((*pv)[i]);
// 	G4NeutronHPInelastic* theHPInelastic = new G4NeutronHPInelastic;
// 	theHPInelastic->SetMinEnergy(theMin);
// 	theHPInelastic->SetMaxEnergy(theMax);
// 	G4NeutronHPInelasticData* theHPInelasticData = new G4NeutronHPInelasticData;
// 	hp->AddDataSet(theHPInelasticData);
// 	hp->RegisterMe(theHPInelastic);
//       }
//     }
//   }
// }
