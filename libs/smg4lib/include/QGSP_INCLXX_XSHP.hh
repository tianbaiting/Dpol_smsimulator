#ifndef QGSP_INCLXX_XSHP_HH
#define QGSP_INCLXX_XSHP_HH

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class QGSP_INCLXX_XSHP: public G4VModularPhysicsList
{
public:
  QGSP_INCLXX_XSHP(G4int ver = 1, const G4String& type = "QGSP_INCLXX_XSHP");
  virtual ~QGSP_INCLXX_XSHP();

public:
  virtual void SetCuts();
};

#endif



