/*
  Simple analysis example
  root[0] .L macros/examples/analysis_crosstalk_example.cc+g
  root[1] analysis_crosstalk_example()
*/
#include <iostream>

#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TH2.h"
#include "TClonesArray.h"

#include "TBeamSimData.hh"
#include "TFragSimData.hh"
#include "TNEBULASimData.hh"
#include "TNeuLANDSimData.hh"
#include "TArtNEBULAPla.hh"

#include "TRunSimParameter.hh"
#include "TFragSimParameter.hh"

#include "TArtCrosstalkAnalysis.hh"
#include "TArtCrosstalkAnalysis_DoNothing.hh"
#include "TArtCrosstalkAnalysis_NEBULANeuLAND_Example.hh"

void analysis_1n_crosstalk_example(TString filename="root/examples/nebula_neuland_1_1n_0000.root")
{

  TFile *file = new TFile(filename.Data(),"readonly");

  TRunSimParameter *RunPrm = 0;
  file->GetObject("RunParameter",RunPrm);
  RunPrm->Print();

  TFragSimParameter *FragPrm = 0;
  file->GetObject("FragParameter",FragPrm);
  FragPrm->Print();

  TTree *tree = 0;
  file->GetObject(RunPrm->fTreeName.Data(),tree);

  TClonesArray *NEBULAPlaArray = new TClonesArray("TArtNEBULAPla",605);

  tree->SetBranchAddress("beam",&gBeamSimDataArray);
  tree->SetBranchAddress("fragment",&gFragSimDataArray);
//  tree->SetBranchAddress("nebula",&gNEBULASimDataArray);
//  tree->SetBranchAddress("neuland",&gNeuLANDSimDataArray);
  tree->SetBranchAddress("NEBULAPla",&NEBULAPlaArray);


//  TArtCrosstalkAnalysis *ctana = new TArtCrosstalkAnalysis_DoNothing;
  TArtCrosstalkAnalysis_NEBULANeuLAND_Example *ctana = new TArtCrosstalkAnalysis_NEBULANeuLAND_Example;
  ctana->SetNEBULAPlaArray(NEBULAPlaArray);
  ctana->SetNeutThreshold(6);
  ctana->SetVetoThreshold(1);

  TH1* hbeamposxy = new TH2D("hbeamposxy","Beam Pos XY (mm)",200,-50,50,200,-50,50);
  TH1* hmulti     = new TH1I("hmulti","NEBULA Multiplicity",10,-0.5,9.5);
  TH1* hmulti_tv  = new TH1I("hmulti_tv","NEBULA Multiplicity Thre,VETO",10,-0.5,9.5);
  TH1* hmulti_cta = new TH1I("hmulti_cta","NEBULA Multiplicity cta",10,-0.5,9.5);
  TH1* hidq = new TH2D("hidq","ID Q",605,0.5,605.5, 200,0,200);
  TH1* hidq_tv = new TH2D("hidq_tv","ID Q Thre,VETO",605,0.5,605.5, 200,0,200);
  TH1* hidx = new TH2D("hidx","ID X(mm)",605,0.5,605.5, 200,-2000,2000);
  TH1* hidy = new TH2D("hidy","ID Y(mm)",605,0.5,605.5, 200,-2000,2000);
  TH1* hidz = new TH2D("hidz","ID Z(mm)",605,0.5,605.5, 200,10000,15000);
  TH1* hnebulatq = new TH2D("hnebulatq","NEBULA TOF(ns) Q(MeVee)",200,0,200,200,0,200);
  TH1* hnebulaxy = new TH2D("hnebulaxy","NEBULA Pos XY (mm)",200,-2000,2000,200,-2000,2000);
  TH1* hnebulazx = new TH2D("hnebulazx","NEBULA Pos ZX (mm)",200,10000,15000,200,-2000,2000);

  TH1* hid1id2     = new TH2I("hid1id2","ID1 ID2",605,0.5,605.5, 605,0.5,605.5);
  TH1* hid1id2_cta = new TH2I("hid1id2_cta","ID1 ID2 cta",605,0.5,605.5, 605,0.5,605.5);
  TH1* hdtdr_s     = new TH2D("hdtdr_s","dt dr SAME",200,0,10, 200,0,1000);
  TH1* hdtdr_s_cta = new TH2D("hdtdr_s_cta","dt dr SAME cta",200,0,10, 200,0,1000);
  TH1* hb0112q2_s     = new TH2D("hb0112q2_s","beta01/beta12 Q2 SAME",200,-4,4, 200,0,150);
  TH1* hb0112q2_s_cta = new TH2D("hb0112q2_s_cta","beta01/beta12 Q2 SAME cta",200,-4,4, 200,0,150);

  TH1* hb12q1_d     = new TH2D("hb12q1_d","beta12 Q1 DIFF",200,-2,2, 200,0,150);
  TH1* hb12q1_d_cta = new TH2D("hb12q1_d_cta","beta12 Q1 DIFF cta",200,-2,2, 200,0,150);
  TH1* hb12q2_d     = new TH2D("hb12q2_d","beta12 Q2 DIFF",200,-2,2, 200,0,150);
  TH1* hb12q2_d_cta = new TH2D("hb12q2_d_cta","beta12 Q2 DIFF cta",200,-2,2, 200,0,150);
  TH1* hb0112q1_d     = new TH2D("hb0112q1_d","beta01/beta12 Q1 DIFF",200,-4,4, 200,0,150);
  TH1* hb0112q1_d_cta = new TH2D("hb0112q1_d_cta","beta01/beta12 Q1 DIFF cta",200,-4,4, 200,0,150);

  Int_t neve = tree->GetEntries();
  for (Int_t ieve=0;ieve<neve;++ieve){

    if (ieve%1000==0){
      cout<<"\r events: "<<ieve<<" / "<<neve
	  <<" ("<<100.*ieve/neve<<"%)"
	  <<flush;
    }

    gBeamSimDataArray->clear();
    gFragSimDataArray->clear();
//    gNEBULASimDataArray->clear();
//    gNeuLANDSimDataArray->clear();
    NEBULAPlaArray->Delete();

    tree->GetEntry(ieve);

    //___________________________________________________
    // Get beam data
    Int_t nbeam = gBeamSimDataArray->size();
    for (Int_t ibeam=0;ibeam<nbeam;++ibeam){
      TBeamSimData beam = (*gBeamSimDataArray)[ibeam];
      hbeamposxy->Fill(beam.fPosition.x(), beam.fPosition.y());
    }

    //___________________________________________________
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

    //___________________________________________________
    // Get NEBULA data
    Int_t npla = NEBULAPlaArray->GetEntries();
    hmulti->Fill(npla);
    for (Int_t ipla=0;ipla<npla;++ipla){
      TArtNEBULAPla *pla = (TArtNEBULAPla*)NEBULAPlaArray->At(ipla);

      //add dist. between TGT-Magnet
      Double_t z_new = pla->GetPos(2) - FragPrm->fTargetPosition.z();
      pla->SetPos(z_new,2);

      hidq->Fill(pla->GetID(), pla->GetQAveCal());
      hidx->Fill(pla->GetID(), pla->GetPos(0));
      hidy->Fill(pla->GetID(), pla->GetPos(1));
      hidz->Fill(pla->GetID(), pla->GetPos(2));

      hnebulatq->Fill(pla->GetTAveSlw(),pla->GetQAveCal());
      hnebulaxy->Fill(pla->GetPos(0),pla->GetPos(1));
      hnebulazx->Fill(pla->GetPos(2),pla->GetPos(0));
    }

    //___________________________________________________
    // before crosstalk cut
    ctana->Clear();
    ctana->Threshold();
    ctana->Veto();
    ctana->RemoveVeto();
    ctana->CalculateDifference();

    Int_t multi_tv = NEBULAPlaArray->GetEntries();
    hmulti_tv->Fill(multi_tv);
    for (Int_t ipla=0;ipla<multi_tv;++ipla){
      TArtNEBULAPla *pla = (TArtNEBULAPla*)NEBULAPlaArray->At(ipla);
      hidq_tv->Fill(pla->GetID(), pla->GetQAveCal());
    }
    hid1id2->Fill(ctana->GetID1(), ctana->GetID2());
    if (ctana->GetDLayer()==0){// SAME Wall 
      hdtdr_s->Fill(ctana->GetDt(), ctana->GetDPos());
      hb0112q2_s->Fill(ctana->GetBeta01()*ctana->GetBinv12(),ctana->GetQ2());
    }else{
      hb12q1_d->Fill(ctana->GetBeta12(), ctana->GetQ1());
      hb12q2_d->Fill(ctana->GetBeta12(), ctana->GetQ2());
      hb0112q1_d->Fill(ctana->GetBeta01()*ctana->GetBinv12(), ctana->GetQ1());
    }

    //___________________________________________________
    // crosstalk cut
    ctana->KillInvalidHit();
    NEBULAPlaArray->Sort();
    ctana->CheckCrosstalk();
    ctana->KillCrosstalk();
    ctana->Clear();//for initialization of id1, id2, beta12, ...
    ctana->CalculateDifference();

    Int_t multi_cta = NEBULAPlaArray->GetEntries();
    hmulti_cta->Fill(multi_cta);

    hid1id2_cta->Fill(ctana->GetID1(), ctana->GetID2());
    if (ctana->GetDLayer()==0){// SAME Wall 
      hdtdr_s_cta->Fill(ctana->GetDt(), ctana->GetDPos());
      hb0112q2_s_cta->Fill(ctana->GetBeta01()*ctana->GetBinv12(),ctana->GetQ2());
    }else{
      hb12q1_d_cta->Fill(ctana->GetBeta12(), ctana->GetQ1());
      hb12q2_d_cta->Fill(ctana->GetBeta12(), ctana->GetQ2());
      hb0112q1_d_cta->Fill(ctana->GetBeta01()*ctana->GetBinv12(), ctana->GetQ1());
    }

    //___________________________________________________

  }//for (Int_t ieve=0;ieve<neve;++ieve){

}
