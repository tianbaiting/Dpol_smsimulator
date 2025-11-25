#ifndef QGSP_INCLXX_XS_HH
#define QGSP_INCLXX_XS_HH

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class QGSP_INCLXX_XS: public G4VModularPhysicsList
{
public:
  QGSP_INCLXX_XS(G4int ver = 1, const G4String& type = "QGSP_INCLXX_XS");
  virtual ~QGSP_INCLXX_XS();

public:
  virtual void SetCuts();
};

#endif



