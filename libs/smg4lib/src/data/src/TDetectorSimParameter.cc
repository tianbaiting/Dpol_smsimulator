#include "TDetectorSimParameter.hh"

#include <iostream>

ClassImp(TDetectorSimParameter)
//____________________________________________________________________
TDetectorSimParameter::TDetectorSimParameter(TString name)
: TSimParameter(name)
{;}
//____________________________________________________________________
TDetectorSimParameter::~TDetectorSimParameter()
{;}
//____________________________________________________________________
std::ostream& operator<<(std::ostream& out,  const TDetectorSimParameter& prm)
{
  out << prm.GetName() <<": "
      << "fPosition=" <<prm.fPosition.x()<<" "<<prm.fPosition.y()<<" "<<prm.fPosition.z()<<" "
      << "fSize="     <<prm.fSize.x() <<" "<<prm.fSize.y() <<" "<<prm.fSize.z() <<" "
      << "fAngle="    <<prm.fAngle.x()<<" "<<prm.fAngle.y()<<" "<<prm.fAngle.z()<<" ";
  return out;
}
//____________________________________________________________________
void TDetectorSimParameter::Print(Option_t*) const
{
  std::cout << GetName() <<": "
	    << "fPosition=" <<fPosition.x()<<" "<<fPosition.y()<<" "<<fPosition.z()<<" "
	    << "fSize="     <<fSize.x() <<" "<<fSize.y() <<" "<<fSize.z() <<" "
	    << "fAngle="    <<fAngle.x()<<" "<<fAngle.y()<<" "<<fAngle.z()<<" "
	    << std::endl;
}
//____________________________________________________________________
