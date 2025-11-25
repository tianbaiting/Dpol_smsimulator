#ifndef QGSP_MENATE_R_INEL_HH
#define QGSP_MENATE_R_INEL_HH

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class QGSP_MENATE_R_inel: public G4VModularPhysicsList
{
public:
  QGSP_MENATE_R_inel(G4int ver = 1, const G4String& type = "QGSP_MENATE_R_inel");
  virtual ~QGSP_MENATE_R_inel();

public:
  virtual void SetCuts();
};

#endif



