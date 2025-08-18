#include "DeutSteppingAction.hh"
#include "SimDataManager.hh"
#include "TFragSimParameter.hh"
#include "G4Step.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DeutSteppingAction::DeutSteppingAction()
: G4UserSteppingAction(), 
  fAngleDifference(NAN)
{
  auto sman = SimDataManager::GetSimDataManager();
  auto prm = (TFragSimParameter*)sman->FindParameter("FragParameter");
  fTargetAngleExp = prm->fTargetAngle;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DeutSteppingAction::~DeutSteppingAction()
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DeutSteppingAction::UserSteppingAction(const G4Step* step)
{
  G4Track* track = step->GetTrack();
  G4String particle = track->GetParticleDefinition()->GetParticleName();
  G4int parentid = track->GetParentID();

  if (particle == "deuteron" && parentid == 0){

    G4StepPoint* preStep = step->GetPreStepPoint();
    G4ThreeVector dir, dirExp;
    dir = preStep->GetMomentumDirection();
    dirExp.setRhoPhiTheta(1, M_PI, fTargetAngleExp);
    G4double dirDiff = dir.angle(dirExp);

    if (std::isnan(fAngleDifference) || dirDiff < fAngleDifference){
      fAngleDifference = dirDiff;
      fTargetPos = preStep->GetPosition();
      fTargetAngle = preStep->GetMomentumDirection().theta();
    }

  }
}
