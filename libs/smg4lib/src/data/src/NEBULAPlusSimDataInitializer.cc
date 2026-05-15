#include "NEBULAPlusSimDataInitializer.hh"
#include "SimDataManager.hh"
#include "TSimData.hh"

#include <iostream>

#include "TFile.h"
#include "TTree.h"

//____________________________________________________________________
NEBULAPlusSimDataInitializer::NEBULAPlusSimDataInitializer(TString name)
  : SimDataInitializer(name)
{
  fDataStore = false;// default
}
//____________________________________________________________________
NEBULAPlusSimDataInitializer::~NEBULAPlusSimDataInitializer()
{;}
//____________________________________________________________________
int NEBULAPlusSimDataInitializer::Initialize()
{
  SimDataManager *sman = SimDataManager::GetSimDataManager();
  fSimDataArray = sman->FindSimDataArray("NEBULAPlusSimData");

  if (fSimDataArray==0){
    fSimDataArray = new TClonesArray("TSimData",256);
    fSimDataArray->SetName("NEBULAPlusSimData");
    fSimDataArray->SetOwner();
  }

  return 0;
}
//____________________________________________________________________
int NEBULAPlusSimDataInitializer::DefineBranch(TTree* tree)
{
  if (fDataStore) tree->Branch(fSimDataArray->GetName(),&fSimDataArray);
  return 0;
}
//____________________________________________________________________
int NEBULAPlusSimDataInitializer::AddParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int NEBULAPlusSimDataInitializer::RemoveParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int NEBULAPlusSimDataInitializer::PrintParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int NEBULAPlusSimDataInitializer::ClearBuffer()
{
  fSimDataArray->Delete();
  return 0;
}
//____________________________________________________________________
