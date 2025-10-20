#ifndef DEUTPRIMARYGENERATORACTION_HH
#define DEUTPRIMARYGENERATORACTION_HH

#include "PrimaryGeneratorActionBasic.hh"
#include "G4ThreeVector.hh"
#include "TString.h"

class DeutPrimaryGeneratorAction : public PrimaryGeneratorActionBasic
{
public:
  DeutPrimaryGeneratorAction(G4int seed = 128);
  virtual ~DeutPrimaryGeneratorAction();

  void SetPrimaryVertex(G4Event* anEvent) override;

  inline void SetUseTargetParameters(bool tf){ fUseTargetParameters = tf; }

private:
  G4bool fUseTargetParameters;
};

#endif