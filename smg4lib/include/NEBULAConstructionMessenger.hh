#ifndef NEBULACONSTRUCTIONMESSENGER_HH
#define NEBULACONSTRUCTIONMESSENGER_HH

#include "G4UImessenger.hh"

class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithABool;

class NEBULAConstructionMessenger : public G4UImessenger
{
public:
  NEBULAConstructionMessenger();
  virtual ~NEBULAConstructionMessenger();

  void SetNewValue(G4UIcommand*, G4String);

protected:
  G4UIdirectory* fNEBULADirectory;
  G4UIcmdWithAString* fParameterFileNameCmd;
  G4UIcmdWithAString* fDetectorParameterFileNameCmd;
  G4UIcmdWithABool* fNEBULAStoreStepsCmd;
  G4UIcmdWithABool* fNEBULAResolutionCmd;

};

#endif

