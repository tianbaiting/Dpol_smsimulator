
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <TFile.h>
#include <TTree.h>
#include <TVector3.h>
#include <TLorentzVector.h>
#include "TBeamSimData.hh"

void convert_Pb208_g080() {
    std::string inputFile = "/home/tian/workspace/dpol/smsimulator5.5/d_work/temp_dat_files/Pb208_g080.dat";
    TVector3 position(0, 0, 0);

    std::ifstream fin(inputFile);
    if (!fin.is_open()) {
        std::cerr << "Cannot open input file: " << inputFile << std::endl;
        return;
    }

    TFile *f = new TFile("/home/tian/workspace/dpol/smsimulator5.5/d_work/rootfiles/ypol_sle_rotate_back/ypol_np_Pb208_g080.root", "RECREATE");
    TTree *tree = new TTree("tree", "Input tree for simultaneous n-p");

    gBeamSimDataArray = new TBeamSimDataArray();
    tree->Branch("TBeamSimData", &gBeamSimDataArray);

    std::string line;
    int eventCount = 0;

    // Skip header lines
    std::getline(fin, line);
    std::getline(fin, line);

    while (std::getline(fin, line)) {
        std::istringstream iss(line);
        int no;
        double pxp, pyp, pzp, pxn, pyn, pzn;
        if (!(iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn)) continue;

        gBeamSimDataArray->clear();

        // Proton
        double Mp = 938.27;
        double Ep = sqrt(pxp*pxp + pyp*pyp + pzp*pzp + Mp*Mp);
        TLorentzVector momentum_p(pxp, pyp, pzp, Ep);
        TBeamSimData proton(1, 1, momentum_p, position);
        proton.fParticleName = "proton";
        proton.fPrimaryParticleID = 0;
        proton.fTime = 0.0;
        proton.fIsAccepted = true;
        gBeamSimDataArray->push_back(proton);

        // Neutron
        double Mn = 939.57;
        double En = sqrt(pxn*pxn + pyn*pyn + pzn*pzn + Mn*Mn);
        TLorentzVector momentum_n(pxn, pyn, pzn, En);
        TBeamSimData neutron(0, 1, momentum_n, position);
        neutron.fParticleName = "neutron";
        neutron.fPrimaryParticleID = 1;
        neutron.fTime = 0.0;
        neutron.fIsAccepted = true;
        gBeamSimDataArray->push_back(neutron);

        tree->Fill();
        eventCount++;
    }

    f->Write();
    f->Close();
    fin.close();

    std::cout << "Converted " << eventCount << " events to " << "/home/tian/workspace/dpol/smsimulator5.5/d_work/rootfiles/ypol_sle_rotate_back/ypol_np_Pb208_g080.root" << std::endl;
}
