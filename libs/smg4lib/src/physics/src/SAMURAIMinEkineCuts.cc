
#include "SAMURAIMinEkineCuts.hh"

#include "G4Step.hh"
#include "G4UserLimits.hh"
#include "G4VParticleChange.hh"
#include "G4EnergyLossTables.hh"
#include "G4SystemOfUnits.hh"//Geant4.10

// for information: locate MinEkineCuts

SAMURAIMinEkineCuts::SAMURAIMinEkineCuts(const G4String& aName)
//  : SpecialCuts(aName)
  : G4SpecialCuts(aName)
{
  if (verboseLevel>1) {
    G4cout << GetProcessName() << " is created "<< G4endl;
  }
  SetProcessType(fUserDefined);
}

SAMURAIMinEkineCuts::~SAMURAIMinEkineCuts()
{}

G4VParticleChange* SAMURAIMinEkineCuts::PostStepDoIt(const G4Track& aTrack, 
						     const G4Step&)
{
  aParticleChange.Initialize(aTrack);
  aParticleChange.ProposeEnergy(0.) ;
  aParticleChange.ProposeLocalEnergyDeposit (aTrack.GetKineticEnergy()) ;
  //  aParticleChange.ProposeTrackStatus(fStopButAlive); // don't need to call process like Decay
  aParticleChange.ProposeTrackStatus(fStopAndKill);
  return &aParticleChange;
}

G4double SAMURAIMinEkineCuts::PostStepGetPhysicalInteractionLength(const G4Track& aTrack,
								   G4double ,
								   G4ForceCondition* condition)
{
  // condition is set to "Not Forced"
  *condition = NotForced;

  const G4DynamicParticle* aParticle = aTrack.GetDynamicParticle();
  G4ParticleDefinition* aParticleDef = aTrack.GetDefinition();

  G4double     proposedStep = DBL_MAX;
  if (aParticleDef->GetPDGCharge() != 0.0) {  // charged particles
    //min kinetic energy
    G4double eKine = aParticle->GetKineticEnergy();
    const G4MaterialCutsCouple* couple = aTrack.GetMaterialCutsCouple();

    G4double eMin = 1*MeV;
    G4double rangeNow = G4EnergyLossTables::GetRange(aParticleDef,eKine,couple);

    if (eKine < eMin ) {
      proposedStep = 0.;
    } else {
      G4double rangeMin = G4EnergyLossTables::GetRange(aParticleDef,eMin,couple);
      G4double temp = rangeNow - rangeMin;
      if (proposedStep > temp) proposedStep = temp;
    }
  }else{
    G4double eKine = aParticle->GetKineticEnergy();
    G4double eMin = 1*MeV;
    if (eKine < eMin ) {
      proposedStep = 0.;
    }
  } 

  return proposedStep;
}
