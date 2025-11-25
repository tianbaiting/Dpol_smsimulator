#ifndef NEBULASIMDATAINITIALIZER_HH
#define NEBULASIMDATAINITIALIZER_HH

#include "SimDataInitializer.hh"

class TFile;
class TTree;

class NEBULASimDataInitializer : public SimDataInitializer
{
public:
  NEBULASimDataInitializer(TString name="NEBULASimDataInitializer");
  virtual ~NEBULASimDataInitializer();
  
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
