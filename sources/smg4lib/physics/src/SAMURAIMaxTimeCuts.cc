
#include "SAMURAIMaxTimeCuts.hh"

#include "G4Step.hh"
#include "G4UserLimits.hh"
#include "G4VParticleChange.hh"
#include "G4EnergyLossTables.hh"
#include "G4SystemOfUnits.hh"// for Geant4.10
#include "G4PhysicalConstants.hh"// for Geant4.10

// for infomation: locate MaxTimeCuts

SAMURAIMaxTimeCuts::SAMURAIMaxTimeCuts(const G4String& aName)
//  : SpecialCuts(aName), fMaxTime(1*microsecond)
  : G4SpecialCuts(aName), fMaxTime(1*microsecond)
{
  if (verboseLevel>1) {
    G4cout << GetProcessName() << " is created "<< G4endl;
  }
  SetProcessType(fUserDefined);
}

SAMURAIMaxTimeCuts::~SAMURAIMaxTimeCuts()
{;}

G4VParticleChange* SAMURAIMaxTimeCuts::PostStepDoIt(const G4Track& aTrack, 
						    const G4Step&)
{
  aParticleChange.Initialize(aTrack);
  aParticleChange.ProposeEnergy(0.) ;
  aParticleChange.ProposeLocalEnergyDeposit (aTrack.GetKineticEnergy()) ;
  //  aParticleChange.ProposeTrackStatus(fStopButAlive); // don't need to call process like Decay
  aParticleChange.ProposeTrackStatus(fStopAndKill);
  return &aParticleChange;
}

G4double SAMURAIMaxTimeCuts::PostStepGetPhysicalInteractionLength(const G4Track& aTrack,
								  G4double ,
								  G4ForceCondition* condition)
{
  // condition is set to "Not Forced"
  *condition = NotForced;

  const G4DynamicParticle* aParticle = aTrack.GetDynamicParticle();

  // can apply cuts for specific particles - use if(particleDef):
  //   G4ParticleDefinition* aParticleDef = aTrack.GetDefinition();

  //max time limit
  G4double dTime= fMaxTime - aTrack.GetGlobalTime();
  G4double proposedStep = DBL_MAX;
  if (dTime < 0. ) {
    proposedStep = 0.;
  } else {  
    G4double beta = (aParticle->GetTotalMomentum())/(aParticle->GetTotalEnergy());
    G4double temp = beta*c_light*dTime;
    if (proposedStep > temp) proposedStep = temp;                  
  }

  return proposedStep;
}
