#ifndef DeutTrackingAction_h
#define DeutTrackingAction_h

#include "TrackingActionBasic.hh"
#include "G4ThreeVector.hh"

/// Tracking action class for determining PDCs and beam dump positions
/// Credit: Boyuan Zhang, 28 Jun 2023

/// Note: This class is only used for automatically determining detector
///       positions. It is abandoned during normal runs.

class G4Tracking;

class DeutTrackingAction : public TrackingActionBasic
{
  public:
    DeutTrackingAction();
    ~DeutTrackingAction() override;

    void PostUserTrackingAction(const G4Track*) override;

    inline void SetPDCKeepAway(G4double keepaway)  { fPDCKeepAway = keepaway;  }
    inline void SetPDC1Distance(G4double distance) { fPDC1Distance = distance; }
    inline void SetPDC2Distance(G4double distance) { fPDC2Distance = distance; }
    inline void SetDumpDistance(G4double distance) { fDumpDistance = distance; }

    inline G4double      GetPDCAngle()  { return fPDCAngle;  }
    inline G4ThreeVector GetPDC1Pos()   { return fPDC1Pos;   }
    inline G4ThreeVector GetPDC2Pos()   { return fPDC2Pos;   }
    inline G4double      GetDumpAngle() { return fDumpAngle; }
    inline G4ThreeVector GetDumpPos()   { return fDumpPos;   }

  private:
    G4double      fPDCAngle;     // PDC angle relative to the beam (z-axis)
    G4ThreeVector fPDC1Pos;      // PDC position in ROTATED frame (same below)
    G4ThreeVector fPDC2Pos;
    G4double      fDumpAngle;
    G4ThreeVector fDumpPos;      // Dump position in ROTATED frame

    G4double fPDCKeepAway;       // PDC transverse distance from the beam
    G4double fPDC1Distance;      // Distance to the magnet center (same below)
    G4double fPDC2Distance;
    G4double fDumpDistance;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif