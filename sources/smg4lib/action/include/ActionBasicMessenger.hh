#ifndef ACTIONBASICMESSENGER_HH
#define ACTIONBASICMESSENGER_HH

#include "G4UImessenger.hh"

class G4UIdirectory;
class G4UIcmdWithABool;
class G4UIcmdWithAString;
class G4UIcmdWithAnInteger;
class G4UIcmdWithADouble;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWith3VectorAndUnit;
class G4UIcmdWithoutParameter;
class PrimaryGeneratorActionBasic;

// singleton pattern
class ActionBasicMessenger : public G4UImessenger
{
public:
  static ActionBasicMessenger* GetInstance();
  virtual ~ActionBasicMessenger();

  void Register(PrimaryGeneratorActionBasic* primaryGeneratorActionBasic);

  void SetNewValue(G4UIcommand*, G4String);

protected:
  static ActionBasicMessenger* fActionBasicMessenger;

  PrimaryGeneratorActionBasic* fPrimaryGeneratorActionBasic;

  G4UIdirectory* fActionDir;
  G4UIdirectory* fActionFileDir;
  G4UIdirectory* fActionGunDir;
  G4UIdirectory* fActionTreeDir;
  G4UIdirectory* fActionDataDir;
  G4UIcmdWithAString* fRunNameCmd;
  G4UIcmdWithAString* fOutputSaveDirCmd;
  G4UIcmdWithAString* fOverWriteCmd;
  G4UIcmdWithAString* fOutputTreeNameCmd;
  G4UIcmdWithAString* fOutputTreeTitleCmd;

  G4UIcmdWithAString* fBeamTypeCmd;
  G4UIcmdWithAnInteger* fBeamACmd;
  G4UIcmdWithAnInteger* fBeamZCmd;
  G4UIcmdWithAString*   fSetBeamParticleNameCmd;
  G4UIcmdWithoutParameter* fSetBeamParticleCmd;
  G4UIcmdWithADoubleAndUnit* fBeamEnergyCmd;
  G4UIcmdWithADouble*        fBeamBrhoCmd;
  G4UIcmdWith3VectorAndUnit* fBeamPositionCmd;
  G4UIcmdWithADoubleAndUnit* fBeamPositionXSigmaCmd;
  G4UIcmdWithADoubleAndUnit* fBeamPositionYSigmaCmd;
  G4UIcmdWithADoubleAndUnit* fBeamAngleXCmd;
  G4UIcmdWithADoubleAndUnit* fBeamAngleYCmd;
  G4UIcmdWithADoubleAndUnit* fBeamAngleXSigmaCmd;
  G4UIcmdWithADoubleAndUnit* fBeamAngleYSigmaCmd;
  G4UIcmdWithAString*        fInputFileNameCmd;
  G4UIcmdWithAString*        fInputTreeNameCmd;
  G4UIcmdWithAnInteger*      fTreeBeamOnCmd;

  G4UIcmdWithABool* fSkipNeutronCmd;
  G4UIcmdWithABool* fSkipGammaCmd;
  G4UIcmdWithABool* fSkipHeavyIonCmd;

  G4UIcmdWithABool* fSkipLargeYAngCmd;

private: // hidden
  ActionBasicMessenger();
  ActionBasicMessenger(const ActionBasicMessenger& actionMessenger);
  ActionBasicMessenger& operator=(const ActionBasicMessenger& actionMessenger);
};

#endif

