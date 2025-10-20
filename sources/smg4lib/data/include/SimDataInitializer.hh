#ifndef SIMDATAINITIALIZER_HH
#define SIMDATAINITIALIZER_HH

#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "TClonesArray.h"

class SimDataInitializer
{
public:
  SimDataInitializer(TString name)
    :  fName(name), fDataStore(true)
  {;}
  virtual ~SimDataInitializer(){;}
  
  // called in RunActionBasic from SimDataManager
  virtual int Initialize() = 0;
  virtual int DefineBranch(TTree* tree) = 0;
  virtual int AddParameters(TFile* file) = 0;
  virtual int RemoveParameters(TFile* file) = 0;
  virtual int PrintParameters(TFile* file) = 0;

  // called in EventActionBasic from SimDataManager
  virtual int ClearBuffer() = 0;

  TString GetName(){return fName;}
  void SetDataStore(bool tf){fDataStore = tf;}
  bool GetDataStore(){return fDataStore;}
  TClonesArray* GetSimDataArray(){return fSimDataArray;}

protected:
  TString fName;
  bool    fDataStore;
  TClonesArray *fSimDataArray;

};

#endif
