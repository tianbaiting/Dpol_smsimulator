#ifndef SIMDATAMANAGER_HH
#define SIMDATAMANAGER_HH

#include <vector>
#include <map>
#include "TString.h"

class SimDataInitializer;
class SimDataConverter;
class TSimParameter;
class TFile;
class TTree;
class TClonesArray;

class SimDataManager
{
public:
  static SimDataManager* GetSimDataManager();// singleton
  virtual ~SimDataManager();

  void RegistInitializer(SimDataInitializer* initializer);
  void RegistConverter(SimDataConverter* converter);
  int  Initialize();
  int  DefineBranch(TTree *tree);
  int  RemoveParameters(TFile* file);
  int  ClearBuffer();
  int  ConvertSimData();
  void RemoveAllInitializer();
  void RemoveAllConverter();

  void AddParameter(TSimParameter* prm);
  void AddParameters(TFile *file);
  TSimParameter* FindParameter(TString name);
  SimDataInitializer* FindInitializer(TString name);
  SimDataConverter* FindConverter(TString name);
  TClonesArray* FindSimDataArray(TString name);
  void  PrintParameters();
  void  PrintInitializers();
  void  PrintConverters();

  void AppendHeader(TString str){fHeader += " " + str;}
  TString GetHeader(){return fHeader;}
  void ClearHeader(){fHeader=" ";}

protected:
  static SimDataManager* fSimDataManager;
  std::vector<SimDataInitializer*> fInitializerArray;
  std::vector<SimDataConverter*> fConverterArray;
  std::map<TString,TSimParameter*> fParameterMap;
  std::map<TString,TClonesArray*> fSimDataArrayPointers;

  TString fHeader;

private:
  SimDataManager();
  SimDataManager(const SimDataManager& simdataManager);
  SimDataManager& operator=(const SimDataManager& simdataManager);
  
};

#endif
