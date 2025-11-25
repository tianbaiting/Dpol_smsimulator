#ifndef _SIMDATACONVERTER_HH_
#define _SIMDATACONVERTER_HH_

#include "TString.h"

class TTree;

class SimDataConverter
{
public:
  SimDataConverter(TString name) 
    : fName(name),
      fDataStore(true)
  {;}
  virtual ~SimDataConverter(){;}
  
  // called in RunActionBasic from SimDataManager
  virtual int Initialize() = 0;
  virtual int DefineBranch(TTree* tree) = 0;
  virtual int ConvertSimData() = 0;

  // called in EventActionBasic from SimDataManager
  virtual int ClearBuffer() = 0;

  void SetName(TString name){fName = name;}
  TString GetName(){return fName;}

  void SetDataStore(bool tf){fDataStore = tf;}
  bool GetDataStore(){return fDataStore;}

protected:
  TString fName;
  bool fDataStore;

};
#endif
