#ifndef RUNACTIONBASIC_HH
#define RUNACTIONBASIC_HH

#include "G4UserRunAction.hh"

class G4Run;
class SimDataManager;
class TRunSimParameter;
class TFragSimParameter;

class TFile;

class RunActionBasic : public G4UserRunAction
{
public:
  RunActionBasic();
  virtual ~RunActionBasic();

  void BeginOfRunAction(const G4Run*);
  void EndOfRunAction(const G4Run*);

protected:
  SimDataManager* fSimDataManager;
  TRunSimParameter* fRunSimParameter;
  TFragSimParameter* fFragSimParameter;

  TFile* fFileOut;
};

#endif

