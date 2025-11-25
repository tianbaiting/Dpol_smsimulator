#ifndef FRAGMENTSD_HH
#define FRAGMENTSD_HH
 
#include "G4VSensitiveDetector.hh"

class FragmentSD : public G4VSensitiveDetector
{
public:
  FragmentSD(G4String name);
  virtual ~FragmentSD();

public:
  void Initialize(G4HCofThisEvent* HCTE);
  G4bool ProcessHits(G4Step* aStep, G4TouchableHistory* ROhist);
  void EndOfEvent(G4HCofThisEvent* HCTE);
};

#endif
