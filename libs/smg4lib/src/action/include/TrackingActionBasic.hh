#ifndef TRACKINGACTIONBASIC_HH
#define TRACKINGACTIONBASIC_HH

#include "G4UserTrackingAction.hh"
#include <map>

class G4Track;

class TrackingActionBasic : public G4UserTrackingAction
{

public:
  TrackingActionBasic();
  virtual ~TrackingActionBasic();
  
  void PreUserTrackingAction(const G4Track*);
  void PostUserTrackingAction(const G4Track*);

  void Clear();
  int GetNumOfTrack();
  int GetParentID(int trackID);

private:
  std::map<int,int> fTrackID2ParentID;

};
#endif
