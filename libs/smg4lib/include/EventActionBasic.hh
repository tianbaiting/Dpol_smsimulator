#ifndef EVENTACTIONBASIC_HH
#define EVENTACTIONBASIC_HH
 
#include "G4UserEventAction.hh"
#include "globals.hh"

#include <ctime>

class G4Event;
class OutputFileParameter;
class TRunSimParameter;
class SimDataManager;

class EventActionBasic : public G4UserEventAction
{
public:
  EventActionBasic();
  virtual ~EventActionBasic();

  void BeginOfEventAction(const G4Event*);
  void EndOfEventAction(const G4Event*);

protected:
  SimDataManager* fSimDataManager;
  TRunSimParameter* fRunSimParameter;

  time_t fTimeLast, fTimeStart;
};

#endif


