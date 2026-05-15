#ifndef NEBULASPLUSSD_HH
#define NEBULASPLUSSD_HH

#include "G4VSensitiveDetector.hh"

class NEBULAPlusSD : public G4VSensitiveDetector
{
public:
  NEBULAPlusSD(G4String name);
  virtual ~NEBULAPlusSD();

public:
  void Initialize(G4HCofThisEvent* HCTE);
  G4bool ProcessHits(G4Step* aStep, G4TouchableHistory* ROhist);
  void EndOfEvent(G4HCofThisEvent* HCTE);
};

#endif
