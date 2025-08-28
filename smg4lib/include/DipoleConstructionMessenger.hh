#ifndef DIPOLECONSTRUCTIONMESSENGER_HH
#define DIPOLECONSTRUCTIONMESSENGER_HH

#include "G4UImessenger.hh"

class DipoleConstruction;
class G4UIdirectory;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithAString;
class G4UIcmdWithADouble;
class G4UIcmdWithoutParameter;

class DipoleConstructionMessenger : public G4UImessenger
{
public:
  DipoleConstructionMessenger(DipoleConstruction*);
  virtual ~DipoleConstructionMessenger();

  void SetNewValue(G4UIcommand*, G4String);

protected:
  DipoleConstruction* fDipoleConstruction;

  G4UIdirectory* fDipoleDirectory;

  G4UIcmdWithADoubleAndUnit* fDipoleAngleCmd;
  G4UIcmdWithAString* fDipoleFieldFileCmd;
  G4UIcmdWithADouble* fDipoleFieldFactorCmd;
  G4UIcmdWithoutParameter* fDipolePlotFieldCmd;

};

#endif

