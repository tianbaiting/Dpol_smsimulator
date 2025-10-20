#include "TSimData.hh"

#include <iostream>
#include <algorithm>
#include <iterator>

ClassImp(TSimData)
//____________________________________________________________________
std::ostream& operator<<(std::ostream& out, const TSimData& data)
{
  //out << data.fPrimaryParticleID << " ";
  out << data.fParentID << " ";
  out << data.fTrackID << " ";
  out << data.fStepNo << " ";
  out << "\""<<data.fParticleName.Data() << "\" ";
  out << "\""<<data.fDetectorName.Data() << "\" ";
  out << data.fZ << " ";
  out << data.fA << " ";
  out << data.fPDGCode << " ";
  out << data.fCharge << " ";
  out << data.fPreMomentum.M() << " ";
  out << data.fPreMomentum.Px() << " ";
  out << data.fPreMomentum.Py() << " ";
  out << data.fPreMomentum.Pz() << " ";
  out << data.fPrePosition.X() << " ";
  out << data.fPrePosition.Y() << " ";
  out << data.fPrePosition.Z() << " ";
  out << data.fPreTime << " ";
  out << data.fFlightLength << " ";
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
TSimData::TSimData() 
  : TObject(),
    //fPrimaryParticleID(0),
    fParentID(0),
    fTrackID(0),
    fStepNo(0),
    fZ(-9999),
    fA(-9999),
    fPDGCode(0),
    fParticleName(""),
    fDetectorName(""),
    fModuleName(""),
    fProcessName(""),
    fCharge(-9999),
    fMass(-9999),
    fPreKineticEnergy(0),
    fPostKineticEnergy(0),
    fEnergyDeposit(0),
    fPreMomentum(0,0,0,0),
    fPostMomentum(0,0,0,0),
    fPrePosition(0,0,0),
    fPostPosition(0,0,0),
    fPreTime(0),
    fPostTime(0),
    fFlightLength(0),
    fIsAccepted(true)
{;}
//____________________________________________________________________
TSimData::~TSimData()
{;}
//____________________________________________________________________
