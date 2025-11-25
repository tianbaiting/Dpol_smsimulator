#ifndef QGSP_BERT_XSHP_HH
#define QGSP_BERT_XSHP_HH

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class QGSP_BERT_XSHP: public G4VModularPhysicsList
{
public:
  QGSP_BERT_XSHP(G4int ver = 1, const G4String& type = "QGSP_BERT_XSHP");
  virtual ~QGSP_BERT_XSHP();

public:
  virtual void SetCuts();
};

#endif



