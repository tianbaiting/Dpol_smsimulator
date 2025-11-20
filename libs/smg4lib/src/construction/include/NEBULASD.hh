#ifndef NEBULASD_HH
#define NEBULASD_HH
 
#include "G4VSensitiveDetector.hh"

class NEBULASD : public G4VSensitiveDetector
{
public:
  NEBULASD(G4String name);
  virtual ~NEBULASD();

public:
  void Initialize(G4HCofThisEvent* HCTE);
  G4bool ProcessHits(G4Step* aStep, G4TouchableHistory* ROhist);
  void EndOfEvent(G4HCofThisEvent* HCTE);
};

#endif
