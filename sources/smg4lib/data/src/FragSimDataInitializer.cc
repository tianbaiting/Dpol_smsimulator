#include "FragSimDataInitializer.hh"
#include "SimDataManager.hh"

#include <iostream>

#include "TFile.h"
#include "TTree.h"

//____________________________________________________________________
FragSimDataInitializer::FragSimDataInitializer(TString name)
  : SimDataInitializer(name)
{;}
//____________________________________________________________________
FragSimDataInitializer::~FragSimDataInitializer()
{;}
//____________________________________________________________________
int FragSimDataInitializer::Initialize()
{
  SimDataManager *sman = SimDataManager::GetSimDataManager();
  fSimDataArray = sman->FindSimDataArray("FragSimData");

  if (fSimDataArray==0){
    fSimDataArray = new TClonesArray("TSimData",256);
    fSimDataArray->SetName("FragSimData");
    fSimDataArray->SetOwner();
  }

  return 0;
}
//____________________________________________________________________
int FragSimDataInitializer::DefineBranch(TTree* tree)
{
  if (fDataStore) tree->Branch(fSimDataArray->GetName(),&fSimDataArray);
  return 0;
}
//____________________________________________________________________
int FragSimDataInitializer::AddParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int FragSimDataInitializer::RemoveParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int FragSimDataInitializer::PrintParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int FragSimDataInitializer::ClearBuffer()
{
  fSimDataArray->Delete();
  return 0;
}
//____________________________________________________________________
