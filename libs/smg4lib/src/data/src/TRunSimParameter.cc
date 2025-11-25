#include "TRunSimParameter.hh"

#include <iostream>

ClassImp(TRunSimParameter)
//____________________________________________________________________
TRunSimParameter::TRunSimParameter(TString name)
: TSimParameter(name), fRunName("run"), fSaveDir("root/"), fOverWrite("ask"),
  fHeader(""), fTreeName("tree"), fTreeTitle("")
{;}
//____________________________________________________________________
TRunSimParameter::~TRunSimParameter()
{;}
//____________________________________________________________________
void TRunSimParameter::Print(Option_t*) const
{
  std::cout << GetName() <<": "
	    << " RunName="<< fRunName.Data() << " "
	    << " SaveDir=" << fSaveDir.Data() << " "
	    << " OverWrite=" << fOverWrite.Data() << " "
	    << " Header=" << fHeader.Data() << " "
	    << " TreeName=" << fTreeName.Data() << " "
	    << " TreeTitle=" << fTreeTitle.Data() << " "
	    << std::endl;
}
//____________________________________________________________________
