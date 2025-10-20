#ifndef QGSP_BERT_XS_HH
#define QGSP_BERT_XS_HH

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class QGSP_BERT_XS: public G4VModularPhysicsList
{
public:
  QGSP_BERT_XS(G4int ver = 1, const G4String& type = "QGSP_BERT_XS");
  virtual ~QGSP_BERT_XS();

public:
  virtual void SetCuts();
};

#endif



