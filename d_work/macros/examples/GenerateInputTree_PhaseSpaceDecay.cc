// HOW TO USE
// root [0] .L macros/examples/GenerateInputTree_PhaseSpaceDecay.cc+
// root [1] GenerateInputTree_PhaseSpaceDecay()

#include "TBeamSimData.hh"

#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TH2.h"
#include "TGenPhaseSpace.h"
#include "TRandom3.h"
#include "TLorentzVector.h"
#include "TVector3.h"

#include <iostream>

void GenerateInputTree_PhaseSpaceDecay(Double_t Erel_in=1.0, 
				       Double_t E_in=200){
  // parameteres
  const int neve = 10000;

  const Int_t Abeam = 26;
  const Int_t Afrag = 24;
  const Int_t Zfrag = 8;
  const Int_t Ndecay = Abeam - Afrag + 1;

  const Int_t    Z[Ndecay]={Zfrag,0,0};
  const Int_t    A[Ndecay]={Afrag,1,1};
  const Double_t Masses[Ndecay] = {22370.8, 939.565, 939.565};

  // Init Phase Space Decay
  Double_t Mbeam = Erel_in;
  for (Int_t i=0;i<Ndecay;++i) Mbeam+=Masses[i];
  Double_t Tbeam = Abeam*E_in;
  Double_t Ebeam = Mbeam + Tbeam;
  Double_t Pbeam_abs = sqrt(Tbeam*(Tbeam+2.0*Mbeam));
  TGenPhaseSpace Decay;

  TFile *file = new TFile("root/examples/inputtree_example.root","recreate");
  TTree *tree = new TTree("tree_input","Input tree for simtrace");
  gBeamSimDataArray = new TBeamSimDataArray();
  tree->Branch("TBeamSimData", &gBeamSimDataArray);

  // for check
  TH1* herel_gen = new TH1D("herel_gen","Erel generated",200,0,10);
  TH1* hxypos = new TH2D("hxypos","XY pos (mm)",100,-50,50, 100,-50,50);
  TH1* hzxpos = new TH2D("hzxpos","ZX pos (mm)",100,-50,50, 100,-50,50);
  TH1* hxyang = new TH2D("hxyang","XY ang (mard)",100,-50,50, 100,-50,50);

  for (int i=0;i<neve;++i){
    gBeamSimDataArray->clear();

    TLorentzVector Pbeam(0, 0, Pbeam_abs, Ebeam);

    // position spread: uniform, +-20mm for XY, +-5mm for Z
    Double_t Zpos_target = -4000.;
    TVector3 pos_vec(gRandom->Uniform(-20,20),
                     gRandom->Uniform(-20,20),
                     gRandom->Uniform(-5,5)+Zpos_target); 

    // angular spread xy: +-10mrad
    Double_t xang = gRandom->Gaus(0,0.01);
    Double_t yang = gRandom->Gaus(0,0.01);
    Pbeam.RotateY(xang);
    Pbeam.RotateX(-yang);
    Decay.SetDecay(Pbeam,Ndecay,Masses);

    // Generate decayed events
    double weight = Decay.Generate();
    while (gRandom->Uniform()>weight) weight = Decay.Generate();

    TLorentzVector P[Ndecay];
    for (int i=0;i<Ndecay;++i){
      P[i] = *(Decay.GetDecay(i));
      TBeamSimData particle(Z[i],A[i],P[i],pos_vec);
      if (Z[i]==0) particle.fParticleName="neutron";
      particle.fPrimaryParticleID = i;
      gBeamSimDataArray->push_back(particle);
    }
    
    // checking input
    TLorentzVector Ptot(0,0,0,0);
    Double_t Mtot=0;
    for (Int_t i=0;i<Ndecay;i++) {
      Ptot += P[i];
      Mtot += P[i].M();
    }
    Double_t Erel_gen = Ptot.M() - Mtot;
    herel_gen->Fill(Erel_gen);
    hxypos->Fill(pos_vec.x(), pos_vec.y());
    hzxpos->Fill(pos_vec.z(), pos_vec.x());
    hxyang->Fill(Ptot.Px()/Ptot.Pz()*1000., Ptot.Py()/Ptot.Pz()*1000.);

    // Fill tree
    tree->Fill();

  }
  file->Write();
}
