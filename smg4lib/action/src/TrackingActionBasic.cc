#include "TrackingActionBasic.hh"

#include <iostream>
#include <map>
#include "G4Track.hh"
#include "G4TrackingManager.hh"
#include "G4TrackVector.hh"

//____________________________________________________________________
TrackingActionBasic::TrackingActionBasic()
{
}
//____________________________________________________________________
TrackingActionBasic::~TrackingActionBasic()
{
}
//____________________________________________________________________
void TrackingActionBasic::PreUserTrackingAction(const G4Track* atrack)
{
  int trackid = atrack->GetTrackID();
  int parentid = atrack->GetParentID();
  fTrackID2ParentID.insert(std::pair<int,int>(trackid,parentid));
}
//____________________________________________________________________
void TrackingActionBasic::PostUserTrackingAction(const G4Track* atrack)
{}
//____________________________________________________________________
void TrackingActionBasic::Clear()
{
  fTrackID2ParentID.clear();
}
//____________________________________________________________________
int TrackingActionBasic::GetNumOfTrack()
{
  int n = fTrackID2ParentID.size();
  return n;
}
//____________________________________________________________________
int TrackingActionBasic::GetParentID(int trackID)
{
  if (GetNumOfTrack()==0) return -1;

  std::map<int,int>::iterator iter = fTrackID2ParentID.find(trackID);
  if (iter!=fTrackID2ParentID.end())  return iter->second;

  std::cout<<__FILE__<<" Something wrong with TrackID and ParentID"<<std::endl;
  return -1;
}
//____________________________________________________________________
