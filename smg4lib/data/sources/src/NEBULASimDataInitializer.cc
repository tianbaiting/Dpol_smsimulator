#include "NEBULASimDataInitializer.hh"
#include "SimDataManager.hh"
#include "TSimData.hh"

#include <iostream>

#include "TFile.h"
#include "TTree.h"

//____________________________________________________________________
NEBULASimDataInitializer::NEBULASimDataInitializer(TString name)
  : SimDataInitializer(name)
{
  fDataStore = false;// default
}
//____________________________________________________________________
NEBULASimDataInitializer::~NEBULASimDataInitializer()
{;}
//____________________________________________________________________
int NEBULASimDataInitializer::Initialize()
{
  SimDataManager *sman = SimDataManager::GetSimDataManager();
  fSimDataArray = sman->FindSimDataArray("NEBULASimData");

  if (fSimDataArray==0){
    fSimDataArray = new TClonesArray("TSimData",256);
    fSimDataArray->SetName("NEBULASimData");
    fSimDataArray->SetOwner();
  }

  return 0;
}
//____________________________________________________________________
int NEBULASimDataInitializer::DefineBranch(TTree* tree)
{
  if (fDataStore) tree->Branch(fSimDataArray->GetName(),&fSimDataArray);
  return 0;
}
//____________________________________________________________________
int NEBULASimDataInitializer::AddParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int NEBULASimDataInitializer::RemoveParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int NEBULASimDataInitializer::PrintParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int NEBULASimDataInitializer::ClearBuffer()
{
  fSimDataArray->Delete();
  return 0;
}
//____________________________________________________________________
