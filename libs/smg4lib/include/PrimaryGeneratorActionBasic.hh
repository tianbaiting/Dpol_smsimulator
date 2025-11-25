#ifndef PRIMARYGENERATORACTIONBASIC_HH
#define PRIMARYGENERATORACTIONBASIC_HH

#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"

#include "G4ThreeVector.hh"
#include "TString.h"

class G4ParticleGun;
class G4ParticleDefinition;
class G4ParticleTable;

class TRandom3;
class TLorentzVector;
class TFile;
class TTree;
class TBranch;

class ActionBasicMessenger;
class BeamSimDataMessenger;

class PrimaryGeneratorActionBasic : public G4VUserPrimaryGeneratorAction
{
public:
  PrimaryGeneratorActionBasic(G4int seed = 128);
  virtual ~PrimaryGeneratorActionBasic();

  virtual bool SetBeamType(G4String beamType);
  void GeneratePrimaries(G4Event* anEvent);
  virtual void SetPrimaryVertex(G4Event* anEvent);

//  virtual void AccumulateBeamData(G4Event* anEvent);

  bool SetBeamA(G4int A);
  bool SetBeamZ(G4int Z);
  bool SetBeamParticle();
  bool SetBeamParticle(G4String particleName);
  bool SetBeamEnergy(G4double energy);
  bool SetBeamBrho(double brho);
  bool SetBeamPosition(G4ThreeVector position);
  bool SetBeamPositionXSigma(G4double sigma);
  bool SetBeamPositionYSigma(G4double sigma);
  bool SetBeamAngleX(G4double theta);
  bool SetBeamAngleY(G4double theta);
  bool SetBeamAngleXSigma(G4double theta);
  bool SetBeamAngleYSigma(G4double theta);

  void BeamTypePencil(G4Event* anEvent);
  void BeamTypeGaus(G4Event* anEvent);
  void BeamTypeIsotropic(G4Event* anEvent);
  void BeamTypeTree(G4Event* anEvent);
  void BeamTypeGeantino(G4Event* anEvent);
  void SetInputTreeFileName(TString rootFile);
  void SetInputTreeName(TString rootFile);
  void TreeBeamOn(G4int num);

  void SetSkipNeutron(bool tf){fSkipNeutron=tf;}
  void SetSkipHeavyIon(bool tf){fSkipHeavyIon=tf;}
  void SetSkipGamma(bool tf){fSkipGamma=tf;}

  void SetSkipLargeYAng(bool tf){fSkipLargeYAng=tf;}

protected:
  G4ParticleGun* fParticleGun;

  TRandom3* fRandom;
  G4ParticleTable* fParticleTable;

  TString fBeamType;

  G4int    fBeamA;
  G4int    fBeamZ;
  G4double fBeamEnergy;
  G4double fBeamBrho;
  G4ThreeVector fBeamPosition;
  G4double fBeamPositionXSigma;
  G4double fBeamPositionYSigma;
  G4double fBeamAngleX;
  G4double fBeamAngleY;
  G4double fBeamAngleXSigma;
  G4double fBeamAngleYSigma;

  // for BeamTypeTree
  TFile   *fRootInputFile;
  TTree   *fInputTree;
  TString fInputTreeName;
  TBranch *fBranchInput;

  bool fSkipNeutron;
  bool fSkipHeavyIon;
  bool fSkipGamma;

  bool fSkipLargeYAng;

  void (PrimaryGeneratorActionBasic::*beamFunctionPtr)(G4Event*);

  ActionBasicMessenger* fActionBasicMessenger;
  BeamSimDataMessenger* fBeamSimDataMessenger;

};

#endif


