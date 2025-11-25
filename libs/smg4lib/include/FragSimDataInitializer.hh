#ifndef FRAGSIMDATAINITIALIZER_HH
#define FRAGSIMDATAINITIALIZER_HH

#include "SimDataInitializer.hh"

class FragSimDataInitializer : public SimDataInitializer
{
public:
  FragSimDataInitializer(TString name="FragSimDataInitializer");
  virtual ~FragSimDataInitializer();
  
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
