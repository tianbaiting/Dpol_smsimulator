#include "BeamSimDataInitializer.hh"

#include <iostream>

#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"

#include "TBeamSimData.hh"

//____________________________________________________________________
BeamSimDataInitializer::BeamSimDataInitializer(TString name)
  : SimDataInitializer(name)
{;}
//____________________________________________________________________
BeamSimDataInitializer::~BeamSimDataInitializer()
{;}
//____________________________________________________________________
int BeamSimDataInitializer::Initialize()
{
  fSimDataArray = 0;// not used
  if (gBeamSimDataArray==0) delete gBeamSimDataArray;
  gBeamSimDataArray = new TBeamSimDataArray;
  return 0;
}
//____________________________________________________________________
int BeamSimDataInitializer::DefineBranch(TTree* tree)
{
  if (fDataStore) tree->Branch("beam",gBeamSimDataArray);
  return 0;
}
//____________________________________________________________________
int BeamSimDataInitializer::AddParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int BeamSimDataInitializer::RemoveParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int BeamSimDataInitializer::PrintParameters(TFile* file)
{
  return 0;
}
//____________________________________________________________________
int BeamSimDataInitializer::ClearBuffer()
{
  gBeamSimDataArray->clear();
  return 0;
}
//____________________________________________________________________
