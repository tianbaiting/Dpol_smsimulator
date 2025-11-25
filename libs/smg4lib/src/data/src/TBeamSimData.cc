#include "TBeamSimData.hh"

#include <iostream>
#include <algorithm>
#include <iterator>

ClassImp(TBeamSimData)
TBeamSimDataArray* gBeamSimDataArray = 0;
//____________________________________________________________________
std::ostream& operator<<(std::ostream& out, const TBeamSimData& data)
{
  out << data.fPrimaryParticleID << " ";
  out << "\""<<data.fParticleName.Data() << "\" ";
  out << data.fZ << " ";
  out << data.fA << " ";
  out << data.fPDGCode << " ";
  out << data.fCharge << " ";
  out << data.fMomentum.M() << " ";
  out << data.fMomentum.Px() << " ";
  out << data.fMomentum.Py() << " ";
  out << data.fMomentum.Pz() << " ";
  out << data.fPosition.X() << " ";
  out << data.fPosition.Y() << " ";
  out << data.fPosition.Z() << " ";
  out << data.fTime << " ";
  out << data.fIsAccepted;
  if(! data.fUserDouble.empty()){
    for(int i=0; i<(int)data.fUserDouble.size(); ++i) out << " " << data.fUserDouble[i];
  }
  if(!data.fUserInt.empty()){
    for(int i=0; i<(int)data.fUserInt.size(); ++i) out << " " << data.fUserInt[i];
  }

  return out;
}
//____________________________________________________________________
TBeamSimData::TBeamSimData() 
  : TObject(),
    fPrimaryParticleID(0),
    fZ(-9999),
    fA(-9999),
    fPDGCode(0),
    fParticleName(""),
    fCharge(-9999),
    fMass(-9999),
    fMomentum(0,0,0,0),
    fPosition(0,0,0),
    fTime(0),
    fIsAccepted(true)
{;}
//____________________________________________________________________
TBeamSimData::TBeamSimData(TString name, TLorentzVector mom, TVector3 pos) 
  : TObject(),
    fPrimaryParticleID(0),
    fZ(-9999),
    fA(-9999),
    fPDGCode(0),
    fParticleName(name),
    fCharge(-9999),
    fMass(mom.M()),
    fMomentum(mom),
    fPosition(pos),
    fTime(0),
    fIsAccepted(true)
{;}
//____________________________________________________________________
TBeamSimData::TBeamSimData(Int_t z, Int_t a, TLorentzVector mom, TVector3 pos) 
  : TObject(),
    fPrimaryParticleID(0),
    fZ(z),
    fA(a),
    fPDGCode(0),
    fParticleName(""),
    fCharge(z),
    fMass(mom.M()),
    fMomentum(mom),
    fPosition(pos),
    fTime(0),
    fIsAccepted(true)
{;}
//____________________________________________________________________
TBeamSimData::~TBeamSimData()
{;}
//____________________________________________________________________
