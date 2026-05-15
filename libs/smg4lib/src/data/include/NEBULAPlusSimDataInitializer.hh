#ifndef NEBULAPLUSIMDATAINITIALIZER_HH
#define NEBULAPLUSIMDATAINITIALIZER_HH

#include "SimDataInitializer.hh"

class TFile;
class TTree;

class NEBULAPlusSimDataInitializer : public SimDataInitializer
{
public:
  NEBULAPlusSimDataInitializer(TString name="NEBULAPlusSimDataInitializer");
  virtual ~NEBULAPlusSimDataInitializer();

  // called in RunActionBasic
  virtual int Initialize();
  virtual int DefineBranch(TTree* tree);
  virtual int AddParameters(TFile* file);
  virtual int RemoveParameters(TFile* file);
  virtual int PrintParameters(TFile* file);

  // called in EventActionBasic
  virtual int ClearBuffer();
};

#endif
