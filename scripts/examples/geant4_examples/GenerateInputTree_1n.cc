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

void GenerateInputTree_1n(Double_t E_in=200){
  // parameteres
//  const int neve = 10000;
  const int neve = 100000;
  const Int_t MaxTheta = 60;//mrad

  const Double_t cosMaxTheta = cos(MaxTheta*0.001);
  const Double_t Mn = 939.565330;// neutron mass
  const Double_t P_abs = sqrt(E_in*(E_in+2.0*Mn));

  TFile *file = new TFile("root/examples/inputtree_1n_example.root","recreate");
  TTree *tree = new TTree("tree_input","Input tree for 1n simulation");
  gBeamSimDataArray = new TBeamSimDataArray();
  tree->Branch("TBeamSimData", gBeamSimDataArray);

  // for check
  TH1* he_gen = new TH1D("he_gen","En generated",150,150,300);
  TH1* htheta = new TH1D("htheta","Theta generated",100,0,100);
  TH1* hxypos = new TH2D("hxypos","XY pos (mm)",100,-50,50, 100,-50,50);
  TH1* hzxpos = new TH2D("hzxpos","ZX pos (mm)",100,-5000,-3000, 100,-50,50);
  TH1* hxyang = new TH2D("hxyang","XY ang (mard)",100,-100,100, 100,-100,100);

  for (int i=0;i<neve;++i){
    gBeamSimDataArray->clear();

    // target position without spread
    Double_t Zpos_target = -4500.;
    TVector3 pos_vec(0,0,Zpos_target); 

    // angular spread xy: +-10mrad
    Double_t cosTheta = gRandom->Uniform(cosMaxTheta,1);
    Double_t Theta = acos(cosTheta);//rad
    Double_t Phi = gRandom->Uniform(0,2.*TMath::Pi());
    htheta->Fill(Theta*1000.);

    TLorentzVector P(P_abs*sin(Theta)*cos(Phi), 
		     P_abs*sin(Theta)*sin(Phi), 
		     P_abs*cosTheta, 
		     E_in+Mn);

    TBeamSimData particle(1,0,P,pos_vec);
    particle.fParticleName="neutron";
    particle.fPrimaryParticleID = 1;
    gBeamSimDataArray->push_back(particle);
    

    he_gen->Fill(P.E()-P.M());
    hxypos->Fill(pos_vec.x(), pos_vec.y());
    hzxpos->Fill(pos_vec.z(), pos_vec.x());
    hxyang->Fill(P.Px()/P.Pz()*1000., P.Py()/P.Pz()*1000.);

    // Fill tree
    tree->Fill();

  }
  file->Write();
}
