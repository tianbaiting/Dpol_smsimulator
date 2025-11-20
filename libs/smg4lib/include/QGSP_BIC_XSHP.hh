#ifndef QGSP_BIC_XSHP_HH
#define QGSP_BIC_XSHP_HH

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class QGSP_BIC_XSHP: public G4VModularPhysicsList
{
public:
  QGSP_BIC_XSHP(G4int ver = 1, const G4String& type = "QGSP_BIC_XSHP");
  virtual ~QGSP_BIC_XSHP();

public:
  virtual void SetCuts();
};

#endif



