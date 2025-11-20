/*
  Simple analysis example
  root[0] .L macros/examples/analysis_example.cc+g
  root[1] analysis_example()
*/
#include <iostream>

#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TH2.h"

#include "TBeamSimData.hh"
#include "TFragSimData.hh"

#include "TRunSimParameter.hh"


void analysis_example()
{

  TFile *file = new TFile("root/examples/example_tree0000.root","readonly");
  TRunSimParameter *RunPrm = 0;
  file->GetObject("RunParameter",RunPrm);

  RunPrm->Print();
  TTree *tree = 0;
  file->GetObject(RunPrm->fTreeName.Data(),tree);

  Double_t fdc2x,fdc2y;
  tree->SetBranchAddress("beam",&gBeamSimDataArray);
  tree->SetBranchAddress("fragment",&gFragSimDataArray);
  tree->SetBranchAddress("fdc2x",&fdc2x);
  tree->SetBranchAddress("fdc2y",&fdc2y);
  
  TH1* hbeamposxy = new TH2D("beamposxy","Beam Pos XY",200,-50,50,200,-50,50);
  TH1* hfdc2xy = new TH2D("hfdc2y","FDC2 XY",200,-1000,1000,200,-500,500);
  TH1* hfdc2xy_acc = new TH2D("hfdc2y_acc","FDC2 XY accepted",200,-1000,1000,200,-500,500);
  Int_t neve = tree->GetEntries();
  for (Int_t ieve=0;ieve<neve;++ieve){
    gBeamSimDataArray->clear();
    gFragSimDataArray->clear();
    tree->GetEntry(ieve);
    
    // Get beam data
    Int_t nbeam = gBeamSimDataArray->size();
    for (Int_t ibeam=0;ibeam<nbeam;++ibeam){
      TBeamSimData beam = (*gBeamSimDataArray)[ibeam];
      hbeamposxy->Fill(beam.fPosition.fX, beam.fPosition.fY);
    }

    // Get fragment data
    Bool_t OK_FDC1    = false;
    Bool_t OK_WinHole = false;
    Bool_t OK_FDC2    = false;
    Bool_t OK_HOD     = false;
    Int_t nfrag = gFragSimDataArray->size();
    for (Int_t ifrag=0;ifrag<nfrag;++ifrag){
      TFragSimData frag = (*gFragSimDataArray)[ifrag];
      if (frag.fDetectorName == "FDC1") OK_FDC1 = true;
      if (frag.fDetectorName == "WindowHole") OK_WinHole = true;
      if (frag.fDetectorName == "FDC2") OK_FDC2 = true;
      if (frag.fDetectorName == "HOD") OK_HOD = true;
    }


    hfdc2xy->Fill(fdc2x,fdc2y);
    if (OK_FDC1 && OK_WinHole && OK_FDC2 && OK_HOD)
      hfdc2xy_acc->Fill(fdc2x,fdc2y);      
  }

}
