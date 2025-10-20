
#include "EventActionBasic.hh"

#include "G4Event.hh"
#include "G4RunManager.hh"
#include "G4Run.hh"

#include "SimDataManager.hh"
#include "TRunSimParameter.hh"

#include "TFile.h"
#include "TTree.h"
#include "TDirectory.h"

#include "TBeamSimData.hh"

#include "TrackingActionBasic.hh"

//____________________________________________________________________
EventActionBasic::EventActionBasic()
  : fTimeLast(0), fTimeStart(0)
{
  fSimDataManager = SimDataManager::GetSimDataManager();
  fRunSimParameter = (TRunSimParameter*)fSimDataManager->FindParameter("RunParameter");
}
//____________________________________________________________________
EventActionBasic::~EventActionBasic()
{;}
//____________________________________________________________________
void EventActionBasic::BeginOfEventAction(const G4Event* anEvent)
{
#if DEBUG
  std::cout<<"start BeginOfEventAction"<<std::endl;
#endif 
  // ------ counter -----
  if(anEvent->GetEventID()==0){
    time(&fTimeStart);
    fTimeLast = time(0);
  }

  G4RunManager *RunManager = G4RunManager::GetRunManager();
  const G4Run *Run = RunManager->GetCurrentRun();

  time_t timeNow = time(0);
  if(timeNow-fTimeLast>=1){
    G4cout << "\r Event:" << anEvent->GetEventID()+1 << " ("
	   << (double)(anEvent->GetEventID()+1)/Run->GetNumberOfEventToBeProcessed()*100.  
	   << "%), "
	   << (anEvent->GetEventID()+1)/(timeNow-fTimeStart) << " events/sec, "
	   << timeNow-fTimeStart << " sec elapsed." << std::flush;
    fTimeLast = timeNow;
  }else if(timeNow-fTimeLast<0){
    fTimeLast = timeNow;
  }
#if DEBUG
  std::cout<<"end BeginOfEventAction"<<std::endl;
#endif 
}
//____________________________________________________________________
void EventActionBasic::EndOfEventAction(const G4Event* anEvent)
{

  fSimDataManager->ConvertSimData();

  TTree* tree;
  gDirectory->GetObject(fRunSimParameter->fTreeName, tree);
  tree->Fill();

  if((anEvent->GetEventID()+1) % 10000 == 0){
    gFile->Write(0,TObject::kOverwrite);
  }

  // ------ clear data class -----
  fSimDataManager->ClearBuffer();

  // --- clear storage of trackingID ---
  G4RunManager *RunManager = G4RunManager::GetRunManager();
  TrackingActionBasic *TrackingAction = (TrackingActionBasic*)RunManager->GetUserTrackingAction();
  TrackingAction->Clear();
  //

}  
//____________________________________________________________________
