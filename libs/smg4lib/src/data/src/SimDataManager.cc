#include "SimDataManager.hh"
#include "TSimParameter.hh"

#include "SimDataInitializer.hh"
#include "SimDataConverter.hh"
#include "TSimData.hh"

#include "TFile.h"
#include "TTree.h"
#include "TString.h"

#include <iostream>
//____________________________________________________________________
SimDataManager* SimDataManager::fSimDataManager(0);
//____________________________________________________________________
SimDataManager* SimDataManager::GetSimDataManager()
{
  if (fSimDataManager ==0) fSimDataManager = new SimDataManager;
  return fSimDataManager;
}
//____________________________________________________________________
SimDataManager::~SimDataManager()
{
  RemoveAllInitializer();
  RemoveAllConverter();
  fSimDataManager = 0;
}
//____________________________________________________________________
void SimDataManager::RegistInitializer(SimDataInitializer* initializer)
{
  fInitializerArray.push_back(initializer);
}
//____________________________________________________________________
void SimDataManager::RegistConverter(SimDataConverter* converter)
{
  fConverterArray.push_back(converter);
}
//____________________________________________________________________
int SimDataManager::Initialize()
{
  int ierr=0;
  int n=fInitializerArray.size();
  for (int i=0;i<n;++i){
    SimDataInitializer *initializer = fInitializerArray[i];
    if (initializer->Initialize()!=0) ierr=1;
    else {
      TClonesArray *arr = initializer->GetSimDataArray();
      if (arr!=0) 
	fSimDataArrayPointers.insert(std::pair<TString,TClonesArray*>(arr->GetName(),arr));
    }
  }

  int n_con=fConverterArray.size();
  for (int i=0;i<n_con;++i){
    SimDataConverter *converter = fConverterArray[i];
    if (converter->Initialize()!=0) ierr=1;
  }

  return ierr;
}
//____________________________________________________________________
int SimDataManager::DefineBranch(TTree* tree)
{
  int ierr=0;
  int n=fInitializerArray.size();
  for (int i=0;i<n;++i){
    SimDataInitializer *initializer = fInitializerArray[i];
    if (initializer->DefineBranch(tree)!=0) ierr=1;
  }

  int n_con=fConverterArray.size();
  for (int i=0;i<n_con;++i){
    SimDataConverter *converter = fConverterArray[i];
    if (converter->DefineBranch(tree)!=0) ierr=1;
  }

  return ierr;
}
//____________________________________________________________________
int SimDataManager::RemoveParameters(TFile* file)
{
  std::map<TString,TSimParameter*>::iterator it = fParameterMap.begin();
  while(it!=fParameterMap.end()){
    file->Remove(it->second);
    it++;
  }
  return 0;
}
//____________________________________________________________________
int SimDataManager::ClearBuffer()
{
  int ierr=0;
  int n_ini=fInitializerArray.size();

  for (int i=0;i<n_ini;++i){
    SimDataInitializer *initializer = fInitializerArray[i];
    if (initializer->ClearBuffer()!=0) ierr=1;
  }

  int n_con=fConverterArray.size();
  for (int i=0;i<n_con;++i){
    SimDataConverter *converter = fConverterArray[i];
    if (converter->ClearBuffer()!=0) ierr=1;
  }

  return ierr;
}
//____________________________________________________________________
int SimDataManager::ConvertSimData()
{
  int ierr=0;
  int n_con=fConverterArray.size();
  for (int i=0;i<n_con;++i){
    SimDataConverter *converter = fConverterArray[i];
    if (! converter->GetDataStore()) continue;
    if (converter->ConvertSimData()!=0) ierr=1;
  }
  return ierr;
}
//____________________________________________________________________
void SimDataManager::RemoveAllInitializer()
{
  int n=fInitializerArray.size();
  for (int i=0;i<n;++i){
    SimDataInitializer *initializer = fInitializerArray[i];
    delete initializer;
  }
}
//____________________________________________________________________
void SimDataManager::RemoveAllConverter()
{
  int n=fConverterArray.size();
  for (int i=0;i<n;++i){
    SimDataConverter *converter = fConverterArray[i];
    delete converter;
  }
}
//____________________________________________________________________
void SimDataManager::AddParameter(TSimParameter* sim)
{
  if (FindParameter(sim->GetName())!=0){
    std::cout<<"\x1b[33m SimDataManager::AddSimParameter : "
	     <<"Warning, "<<sim->GetName()<<" had already added."<<std::endl;
    std::cout<<"\x1b[0m";
  }
  fParameterMap.insert(std::pair<TString,TSimParameter*>(sim->GetName(),sim));
}
//____________________________________________________________________
void SimDataManager::AddParameters(TFile *file)
{
  std::map<TString,TSimParameter*>::iterator it = fParameterMap.begin();
  while(it!=fParameterMap.end()){
    file->Append(it->second,kTRUE);
    it++;
  }
}
//____________________________________________________________________
TSimParameter* SimDataManager::FindParameter(TString prmName)
{
  std::map<TString,TSimParameter*>::iterator it;
  it = fParameterMap.find(prmName);
  if (it!=fParameterMap.end()) return it->second;
  else                         return 0;
}
//____________________________________________________________________
SimDataInitializer* SimDataManager::FindInitializer(TString name)
{
  Int_t n_ini = fInitializerArray.size();
  for (int i=0;i<n_ini;++i){
    SimDataInitializer *initializer = fInitializerArray[i];
    if (initializer->GetName()==name) return initializer;
  }
  return 0;
}
//____________________________________________________________________
SimDataConverter* SimDataManager::FindConverter(TString name)
{
  Int_t n_con = fConverterArray.size();
  for (int i=0;i<n_con;++i){
    SimDataConverter *converter = fConverterArray[i];
    if (converter->GetName()==name) return converter;
  }
  return 0;
}
//____________________________________________________________________
TClonesArray* SimDataManager::FindSimDataArray(TString name)
{
  std::map<TString,TClonesArray*>::iterator it = fSimDataArrayPointers.find(name);
  if(it!=fSimDataArrayPointers.end()) return it->second;
  return 0;
}
//____________________________________________________________________
void SimDataManager::PrintParameters()
{
  std::map<TString,TSimParameter*>::iterator it = fParameterMap.begin();
  while(it!=fParameterMap.end()){
    (it->second)->Print();
    it++;
  }
}
//____________________________________________________________________
void SimDataManager::PrintInitializers()
{
  Int_t n = fInitializerArray.size();
  for (Int_t i=0;i<n;++i){
    SimDataInitializer *ini = fInitializerArray[i];
    std::cout<<ini->GetName().Data()<<std::endl;
  }
}
//____________________________________________________________________
void SimDataManager::PrintConverters()
{
  Int_t n = fConverterArray.size();
  for (Int_t i=0;i<n;++i){
    SimDataConverter *conv = fConverterArray[i];
    std::cout<<conv->GetName().Data()<<std::endl;
  }
}
//____________________________________________________________________
//____________________________________________________________________
//____________________________________________________________________
SimDataManager::SimDataManager()
  : fHeader("")
{
  std::cout<<"SimDataManager"<<std::endl;

  fInitializerArray.clear();
  fConverterArray.clear();

}
//____________________________________________________________________
