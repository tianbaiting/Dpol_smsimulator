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

Double_t CalcErel(TLorentzVector P24o, TClonesArray *PlaArray);

void analysis_28o_crosstalk_example(TString filename="root/examples/nebula_neuland_1_28o_psdecay_0000.root",
				    Bool_t DoKillCrosstalk=true)
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
  TArtCrosstalkAnalysis *ctana = new TArtCrosstalkAnalysis_NEBULANeuLAND_Example;
  ctana->SetNEBULAPlaArray(NEBULAPlaArray);
  ctana->SetNeutThreshold(6);
  ctana->SetVetoThreshold(1);
  ctana->SetDoKillCrosstalk(DoKillCrosstalk);

  std::cout<<std::endl
	   <<"DoKillCorsstalk = "<<DoKillCrosstalk
	   <<std::endl;

  //-----------------------------------
  TH1* hbeamposxy = new TH2D("hbeamposxy","Beam Pos XY (mm)",200,-50,50,200,-50,50);
  TH1* hmulti     = new TH1I("hmulti","NEBULA Multiplicity",15,-0.5,14.5);
  TH1* hmulti_cta = new TH1I("hmulti_cta","NEBULA Multiplicity cta",15,-0.5,14.5);
  TH1* hidq = new TH2D("hidq","ID Q",605,0.5,605.5, 200,0,200);
  TH1* hidq_cta = new TH2D("hidq_cta","ID Q Thre,VETO",605,0.5,605.5, 200,0,200);
  TH1* hidx = new TH2D("hidx","ID X(mm)",605,0.5,605.5, 200,-2000,2000);
  TH1* hidy = new TH2D("hidy","ID Y(mm)",605,0.5,605.5, 200,-2000,2000);
  TH1* hidz = new TH2D("hidz","ID Z(mm)",605,0.5,605.5, 200,10000,15000);
  TH1* hnebulatq = new TH2D("hnebulatq","NEBULA TOF(ns) Q(MeVee)",200,0,200,200,0,200);
  TH1* hnebulaxy = new TH2D("hnebulaxy","NEBULA Pos XY (mm)",200,-2000,2000,200,-2000,2000);
  TH1* hnebulazx = new TH2D("hnebulazx","NEBULA Pos ZX (mm)",200,10000,15000,200,-2000,2000);
  TH1* hnebulazy = new TH2D("hnebulazy","NEBULA Pos ZY (mm)",200,10000,15000,200,-2000,2000);

  TH1* hthtxthty = new TH2D("hthtxthty","NEBULA Theta_x Theta_y (mrad)",100,-200,200, 100,-200,200);
  TH1* hthtxthty_cta = new TH2D("hthtxthty_cta","NEBULA Theta_x Theta_y (mrad) cta",100,-200,200, 100,-200,200);

  TH1* hid1id2     = new TH2I("hid1id2","ID1 ID2",605,0.5,605.5, 605,0.5,605.5);
  TH1* hdtdr_s     = new TH2D("hdtdr_s","dt dr SAME",200,0,10, 200,0,1000);
  TH1* hb0112q2_s     = new TH2D("hb0112q2_s","beta01/beta12 Q2 SAME",200,-4,4, 200,0,150);

  TH1* hb12invq1_d     = new TH2D("hb12invq1_d","binv12 Q1 DIFF",200,-6.,6., 200,0,150);
  TH1* hb12invq2_d     = new TH2D("hb12invq2_d","binv12 Q2 DIFF",200,-6.,6., 200,0,150);
  TH1* hb0112q1_d     = new TH2D("hb0112q1_d","beta01/beta12 Q1 DIFF",200,-4,4, 200,0,150);

  TH1* herel     = new TH1D("herel","Erel (MeV)",200,0,10);

  TH1* hnyacc = new TH1I("hnyacc","Yacc for neutrons",2,-0.5,1.5);

  //-----------------------------------
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

    //-----------------------------------
    // Get beam data
    Int_t nbeam = gBeamSimDataArray->size();
    for (Int_t ibeam=0;ibeam<nbeam;++ibeam){
      TBeamSimData beam = (*gBeamSimDataArray)[ibeam];
      hbeamposxy->Fill(beam.fPosition.fX, beam.fPosition.fY);
    }
    // check y acceptance of neutrons
    Int_t OK_nyacc=1;
    for (Int_t ibeam=0;ibeam<nbeam;++ibeam){
      TBeamSimData beam = (*gBeamSimDataArray)[ibeam];
      if (beam.fZ!=0) continue;
      Double_t Theta_y = beam.fMomentum.Py()/beam.fMomentum.Pz()*1000.;// mrad
      if (fabs(Theta_y)>60.) OK_nyacc = 0;
    }
    hnyacc->Fill(OK_nyacc);

    //-----------------------------------
    // get fragment data
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

    //-----------------------------------
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
      hnebulazy->Fill(pla->GetPos(2),pla->GetPos(1));

      Double_t Theta_x = pla->GetPos(0)/pla->GetPos(2)*1000.;
      Double_t Theta_y = pla->GetPos(1)/pla->GetPos(2)*1000.;
      hthtxthty->Fill(Theta_x, Theta_y);
    }

    //-----------------------------------
    // crosstalk cut
    ctana->Analyze();
    Int_t multi_cta = NEBULAPlaArray->GetEntries();
    hmulti_cta->Fill(multi_cta);

    hid1id2->Fill(ctana->GetID1(), ctana->GetID2());
    if (ctana->GetDLayer()==0){// SAME Wall 
      hdtdr_s->Fill(ctana->GetDt(), ctana->GetDPos());
      hb0112q2_s->Fill(ctana->GetBeta01()*ctana->GetBinv12(),ctana->GetQ2());
    }else{                     // DIFF wall
      hb12invq1_d->Fill(ctana->GetBinv12(), ctana->GetQ1());
      hb12invq2_d->Fill(ctana->GetBinv12(), ctana->GetQ2());
      hb0112q1_d->Fill(ctana->GetBeta01()*ctana->GetBinv12(), ctana->GetQ1());
    }

    TLorentzVector P24o;
    // momentum of 24O
    for (Int_t ibeam=0;ibeam<nbeam;++ibeam){
      TBeamSimData beam = (*gBeamSimDataArray)[ibeam];
      if (beam.fZ==8) P24o = beam.fMomentum;
    }
    Double_t Erel = CalcErel(P24o,NEBULAPlaArray);
    herel->Fill(Erel);

    for (Int_t ipla=0;ipla<multi_cta;++ipla){
      TArtNEBULAPla *pla = (TArtNEBULAPla*)NEBULAPlaArray->At(ipla);
      Double_t Theta_x = pla->GetPos(0)/pla->GetPos(2)*1000.;
      Double_t Theta_y = pla->GetPos(1)/pla->GetPos(2)*1000.;
      hthtxthty_cta->Fill(Theta_x, Theta_y);
    }

    
    
    //-----------------------------------

  }//for (Int_t ieve=0;ieve<neve;++ieve){

}
//________________________________________________________
Double_t CalcErel(TLorentzVector P24o, TClonesArray *PlaArray){

  if (PlaArray->GetEntries()<4) return -1000;

  // momentum of 24O
  TLorentzVector P[5];
  P[0] = P24o;
  
  // momentum of neutrons
  for (int i=0;i<4;++i){
    TArtNEBULAPla *pla = (TArtNEBULAPla*)PlaArray->At(i);
    Double_t t = pla->GetTAveSlw();
    Double_t x = pla->GetPos(0);
    Double_t y = pla->GetPos(1);
    Double_t z = pla->GetPos(2);
    Double_t FL = sqrt( x*x + y*y + z*z );
    Double_t c_light = 299.792458; // mm/ns
    Double_t beta = FL/t/c_light;
    Double_t gamma = 1./sqrt((1.0+beta)*(1.0-beta));
    Double_t Mn = 939.565;      // MeV
    Double_t Pabs = Mn*beta*gamma;
    
    P[i+1].SetPxPyPzE(Pabs*x/FL,
		      Pabs*y/FL,
		      Pabs*z/FL,
		      Mn*gamma);
  }
  Double_t Mtot=0;
  TLorentzVector Ptot(0,0,0,0);
  for (int i=0;i<5;++i){
    Mtot += P[i].M();
    Ptot += P[i];
  }
      
  Double_t Erel = Ptot.M() - Mtot;

  return Erel;
}
//________________________________________________________
