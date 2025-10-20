#ifndef QGSP_BIC_XS_HH
#define QGSP_BIC_XS_HH

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class QGSP_BIC_XS: public G4VModularPhysicsList
{
public:
  QGSP_BIC_XS(G4int ver = 1, const G4String& type = "QGSP_BIC_XS");
  virtual ~QGSP_BIC_XS();

public:
  virtual void SetCuts();
};

#endif



