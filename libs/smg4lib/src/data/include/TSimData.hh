#ifndef TSIMDATA_HH
#define TSIMDATA_HH

#include "TObject.h"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TString.h"
#include <vector>

class TSimData : public TObject
{
public:
  TSimData();
  virtual ~TSimData();

  friend std::ostream& operator<<(std::ostream& out,  const TSimData& fragSimData);

public:
  //Int_t          fPrimaryParticleID;// modified from smsimulator4.3
  Int_t          fParentID;         // 0 for primary particle
  Int_t          fTrackID;          //
  Int_t          fStepNo;           // in chronological order
  Int_t          fZ;                //     
  Int_t          fA;                //     
  Int_t          fPDGCode;          //     
  std::vector<Int_t> fModuleID;     // 0:self, 1:mother, 2:grandmother, ....
  Int_t          fID;               // 
  TString        fParticleName;     //     
  TString        fDetectorName;     //     
  TString        fModuleName;       // name of module
  TString        fProcessName;      //
  Double_t       fCharge;           //     
  Double_t       fMass;             // MeV 
  Double_t       fPreKineticEnergy; // MeV
  Double_t       fPostKineticEnergy;// MeV
  Double_t       fEnergyDeposit;    // MeV
  TLorentzVector fPreMomentum;      // MeV (not per nucleon)
  TLorentzVector fPostMomentum;     // MeV (not per nucleon)
  TVector3       fPrePosition;      // mm
  TVector3       fPostPosition;     // mm
  Double_t       fPreTime;          // ns
  Double_t       fPostTime;         // ns
  Double_t       fFlightLength;      // mm
  Bool_t         fIsAccepted;       //

  std::vector<Double_t> fUserDouble;
  std::vector<Int_t> fUserInt;

  ClassDef(TSimData, 1)
};

#endif

