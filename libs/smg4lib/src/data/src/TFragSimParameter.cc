#include "TFragSimParameter.hh"

#include <iostream>

ClassImp(TFragSimParameter)
//____________________________________________________________________
TFragSimParameter::TFragSimParameter(TString name)
: TSimParameter(name), 
  fTargetThickness{0},
  fTargetPosition{0,0,0},
  fTargetAngle{0},

  fPDCAngle{0},
  fPDC1Position{0,0,0},
  fPDC2Position{0,0,0},

  fDumpAngle{0},
  fDumpPosition{0,0,0}
{;}
//____________________________________________________________________
TFragSimParameter::~TFragSimParameter()
{;}
//____________________________________________________________________
void TFragSimParameter::Print(Option_t*) const
{
  std::cout << GetName() <<": "
	    <<" TGT="
	    <<fTargetPosition.x()<<"mm "
	    <<fTargetPosition.y()<<"mm "
	    <<fTargetPosition.z()<<"mm "
		<<fTargetAngle<<"deg "
	    <<" PDC1="
		<<fPDCAngle<<"deg "
	    <<fPDC1Position.x()<<"mm "
	    <<fPDC1Position.y()<<"mm "
	    <<fPDC1Position.z()<<"mm "
	    <<" PDC2="
		<<fPDCAngle<<"deg "
	    <<fPDC2Position.x()<<"mm "
	    <<fPDC2Position.y()<<"mm "
	    <<fPDC2Position.z()<<"mm "
	    <<" Dump="
		<<fDumpAngle<<"deg "
	    <<fDumpPosition.x()<<"mm "
	    <<fDumpPosition.y()<<"mm "
	    <<fDumpPosition.z()<<"mm "
	    
	    << std::endl;
}
//____________________________________________________________________
