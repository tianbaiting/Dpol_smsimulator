#ifndef PHYSICSLISTBASICMESSENGER_HH
#define PHYSICSLISTBASICMESSENGER_HH

#include "G4UImessenger.hh"

class G4VUserPhysicsList;
class G4UIdirectory;
class G4UIcommand;
class G4UIcmdWithADoubleAndUnit;

class PhysicsListBasicMessenger : public G4UImessenger
{
public:
  PhysicsListBasicMessenger();
  ~PhysicsListBasicMessenger();

  void Register(G4VUserPhysicsList* physicsList);

  void SetNewValue(G4UIcommand*, G4String);

protected:
  G4VUserPhysicsList *fPhysicsList;

  G4UIdirectory* fActionCutDir;

  G4UIcmdWithADoubleAndUnit* fCutGammaCmd;
  G4UIcmdWithADoubleAndUnit* fCutElectronCmd;
  G4UIcmdWithADoubleAndUnit* fCutPositronCmd;

};
#endif
