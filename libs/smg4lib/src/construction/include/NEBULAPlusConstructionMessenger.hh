#ifndef NEBULAPLUSCONSTRUCTIONMESSENGER_HH
#define NEBULAPLUSCONSTRUCTIONMESSENGER_HH

#include "G4UImessenger.hh"

class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithABool;

class NEBULAPlusConstructionMessenger : public G4UImessenger
{
public:
  NEBULAPlusConstructionMessenger();
  virtual ~NEBULAPlusConstructionMessenger();

  void SetNewValue(G4UIcommand*, G4String);

protected:
  G4UIdirectory* fNEBULAPlusDirectory;
  G4UIcmdWithAString* fParameterFileNameCmd;
  G4UIcmdWithAString* fDetectorParameterFileNameCmd;
  G4UIcmdWithABool* fNEBULAPlusStoreStepsCmd;
  G4UIcmdWithABool* fNEBULAPlusResolutionCmd;

};

#endif
