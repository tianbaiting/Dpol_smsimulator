#ifndef TBEAMSIMDATAINITIALIZER_HH
#define TBEAMSIMDATAINITIALIZER_HH

#include "SimDataInitializer.hh"

class BeamSimDataMessenger;

class BeamSimDataInitializer : public SimDataInitializer
{
public:
  BeamSimDataInitializer(TString name="BeamSimDataInitializer");
  virtual ~BeamSimDataInitializer();
  
  // called in RunActionBasic
  virtual int Initialize();
  virtual int DefineBranch(TTree* tree);
  virtual int AddParameters(TFile* file);
  virtual int RemoveParameters(TFile* file);
  virtual int PrintParameters(TFile* file);

  // called in EventActionBasic
  virtual int ClearBuffer();

private:
  BeamSimDataMessenger* fMessenger;

};

#endif
