#include "DeutTrackingAction.hh"
#include "SimDataManager.hh"
#include "TFragSimParameter.hh"
#include "G4Track.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DeutTrackingAction::DeutTrackingAction()
: TrackingActionBasic(),
  fPDCKeepAway(200)
{
  auto sman = SimDataManager::GetSimDataManager();
  auto prm = (TFragSimParameter*)sman->FindParameter("FragParameter");

  fPDC1Distance = prm->fPDC1Position.Z();
  fPDC2Distance = prm->fPDC2Position.Z();
  fDumpDistance = prm->fDumpPosition.Z();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DeutTrackingAction::~DeutTrackingAction()
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DeutTrackingAction::PostUserTrackingAction(const G4Track* track)
{
  G4String particle = track->GetParticleDefinition()->GetParticleName();
  G4int parentid = track->GetParentID();
  G4ThreeVector endPoint = track->GetPosition();
  G4ThreeVector endDirection = track->GetMomentumDirection();

  if (particle == "deuteron" && parentid == 0){
    fDumpAngle = endDirection.theta();
    endPoint.rotateY(fDumpAngle); // Change to out beam frame
    fDumpPos = G4ThreeVector{endPoint.x(), -250, fDumpDistance};
  }

  if (particle == "proton" && parentid == 0){
    fPDCAngle = endDirection.theta();
    G4double dTheta = fPDCAngle - fDumpAngle;

    // PDC size: 1700 * 800 * 190 mm
    G4double zPDC = fPDC1Distance - 95; // PDC1 front surface
    G4double xPDC = fDumpPos.x()/cos(dTheta) + zPDC*tan(dTheta) \
                    - 850 - fPDCKeepAway;
    fPDC1Pos.set(xPDC, 0, fPDC1Distance);
    fPDC2Pos.set(xPDC, 0, fPDC2Distance);
  }
}
