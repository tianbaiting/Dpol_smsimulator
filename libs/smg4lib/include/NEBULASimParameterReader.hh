#ifndef NEBULASIMPARAMETERREADER_HH
#define NEBULASIMPARAMETERREADER_HH

#include "SimDataManager.hh"
#include "TNEBULASimParameter.hh"
#include "TDetectorSimParameter.hh"
#include "TSimParameter.hh"

#include "TString.h"

#include <vector>

class NEBULASimParameterReader
{
public:
  NEBULASimParameterReader();
  ~NEBULASimParameterReader();

  void ReadNEBULAParameters(const char* PrmFileName);
  void ReadNEBULADetectorParameters(const char* PrmFileName);
  void CSVToVector(const char* PrmFileName,
		   std::vector<std::vector<TString> >* PrmArray);

public:
  SimDataManager *fSimDataManager;
  TNEBULASimParameter* fNEBULASimParameter;
};
#endif
