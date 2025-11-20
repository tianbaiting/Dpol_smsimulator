// HOW TO USE
// root [0] .L macros/examples/GenerateInputTree_Trajectories.cc+
// root [1] GenerateInputTree_Trajectories()

#include "TBeamSimData.hh"

#include "TFile.h"
#include "TTree.h"
#include "TLorentzVector.h"

#include <iostream>

void GenerateInputTree_Trajectories(){
  // parameteres
  Int_t Z = 2;
  Int_t A = 6;
  Double_t AMU = 931.494;
  Double_t M = A*AMU;
  vector<Double_t> v_kine;
  v_kine.push_back(50);
  v_kine.push_back(100);
  v_kine.push_back(150);
  v_kine.push_back(200);
  v_kine.push_back(250);
  v_kine.push_back(300);

  TFile *file = new TFile("root/examples/inputtree_trajectories.root","recreate");
  TTree *tree = new TTree("tree_input","Input tree for simtrace");
  gBeamSimDataArray = new TBeamSimDataArray();
  tree->Branch("TBeamSimData", &gBeamSimDataArray);

  gBeamSimDataArray->clear();

  TVector3 pos_vec(0,0,-4000);

  for (int i=0;i<v_kine.size();++i){
    Double_t T = v_kine[i]*A;
    Double_t E = M + T;
    Double_t Pabs = sqrt(T*(T+2.*M));
    TLorentzVector P(0,0,Pabs,E);
    TBeamSimData particle(Z,A,P,pos_vec);
    particle.fPrimaryParticleID = i;
    gBeamSimDataArray->push_back(particle);
  }
    
  // Fill tree
  tree->Fill();
  file->Write();
}
