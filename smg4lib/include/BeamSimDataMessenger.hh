#ifndef BEAMSIMDATAMESSENGER_HH
#define BEAMSIMDATAMESSENGER_HH

#include "G4UImessenger.hh"

class G4UIdirectory;
class G4UIcmdWithABool;

class BeamSimDataMessenger : public G4UImessenger
{
public:
  BeamSimDataMessenger();
  virtual ~BeamSimDataMessenger();
  void SetNewValue(G4UIcommand*, G4String);

protected:
  G4UIdirectory* fActionBeamDataDir;
  G4UIcmdWithABool* fStoreBeamDataCmd;

};

#endif

