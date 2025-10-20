#ifndef TBEAMSIMDATA_HH
#define TBEAMSIMDATA_HH

#include "TObject.h"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TString.h"
#include <vector>

class TBeamSimData;
typedef std::vector<TBeamSimData> TBeamSimDataArray;
extern TBeamSimDataArray* gBeamSimDataArray;

class TBeamSimData : public TObject
{
public:
  TBeamSimData();
  TBeamSimData(TString name, TLorentzVector mom, TVector3 pos);
  TBeamSimData(Int_t z, Int_t a, TLorentzVector mom, TVector3 pos);
  virtual ~TBeamSimData();

  friend std::ostream& operator<<(std::ostream& out,  const TBeamSimData& beamSimData);

public:
  Int_t          fPrimaryParticleID;//     for output
  Int_t          fZ;                //     for input
  Int_t          fA;                //     for input
  Int_t          fPDGCode;          //     for output
  TString        fParticleName;     //     for input/outpu
  Double_t       fCharge;           //     for output
  Double_t       fMass;             // MeV for input/output
  TLorentzVector fMomentum;         // MeV (not per nucleon) for input/output
  TVector3       fPosition;         // mm  for output
  Double_t       fTime;             // ns  for output
  Bool_t         fIsAccepted;

  std::vector<Double_t> fUserDouble;
  std::vector<Int_t> fUserInt;

  ClassDef(TBeamSimData, 1)
};

#endif

