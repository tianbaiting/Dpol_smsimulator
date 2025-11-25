#ifndef DeutSteppingAction_h
#define DeutSteppingAction_h

#include "G4UserSteppingAction.hh"
#include "G4ThreeVector.hh"

/// Stepping action class for determining target position
/// Credit: Boyuan Zhang, 28 Jun 2023

/// Note: This class is only used for automatically determining the target
///       position. It is abandoned during normal runs. For information of
///       routine stepping action, pls refer to sensitive detector classes
///       (smg4lib/construction/src/xxxSD.cc).

class G4Step;

class DeutSteppingAction : public G4UserSteppingAction
{
  public:
    DeutSteppingAction();
    ~DeutSteppingAction() override;

    void UserSteppingAction(const G4Step*) override;

    inline void SetTargetAngleExp(G4double angle)  { fTargetAngleExp = angle; }
    inline G4double GetTargetAngle() const { return fTargetAngle; }
    inline G4ThreeVector GetTargetPos() const { return fTargetPos; }
    inline G4double GetAngleDifference() const {return fAngleDifference;}

  private:
    G4ThreeVector fTargetPos;    // Target position in laboratory frame
    G4double fTargetAngle;       // Target rotation angle
    G4double fTargetAngleExp;    // Expected target rotation angle
    G4double fAngleDifference;   // Angle between this step and fTargetAngleExp
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif