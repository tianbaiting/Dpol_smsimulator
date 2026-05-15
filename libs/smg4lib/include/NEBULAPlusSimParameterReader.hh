#ifndef NEBULAPLUSSIMPARAMETERREADER_HH
#define NEBULAPLUSSIMPARAMETERREADER_HH

#include "SimDataManager.hh"
#include "TNEBULAPlusSimParameter.hh"
#include "TDetectorSimParameter.hh"
#include "TSimParameter.hh"

#include "TString.h"

#include <vector>

class NEBULAPlusSimParameterReader
{
public:
  NEBULAPlusSimParameterReader();
  ~NEBULAPlusSimParameterReader();

  void ReadNEBULAPlusParameters(const char* PrmFileName);
  void ReadNEBULAPlusDetectorParameters(const char* PrmFileName);
  void CSVToVector(const char* PrmFileName,
		   std::vector<std::vector<TString> >* PrmArray);

public:
  SimDataManager *fSimDataManager;
  TNEBULAPlusSimParameter* fNEBULAPlusSimParameter;
};
#endif
