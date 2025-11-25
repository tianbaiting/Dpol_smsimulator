#ifndef QGSP_MENATE_R_HH
#define QGSP_MENATE_R_HH

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class QGSP_MENATE_R: public G4VModularPhysicsList
{
public:
  QGSP_MENATE_R(G4int ver = 1, const G4String& type = "QGSP_MENATE_R");
  virtual ~QGSP_MENATE_R();

public:
  virtual void SetCuts();
};

#endif



