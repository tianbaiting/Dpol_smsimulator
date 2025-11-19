#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TCanvas.h"
#include "TNamed.h"
#include "TLorentzVector.h"
#include "TVector3.h"
#include <iostream>

// Same structure as used when writing the file
struct OriginalParticleData {
    bool hasProton;
    TLorentzVector protonMomentum;
    TVector3 protonPosition;
    
    bool hasNeutron;
    TLorentzVector neutronMomentum;
    TVector3 neutronPosition;
    
    std::vector<TLorentzVector> reconstructedProtonMomenta;
    
    OriginalParticleData() : hasProton(false), hasNeutron(false) {}
};


static const std::string smsim_dir = []() {
    const char* v = std::getenv("SMSIMDIR");
    if (v) return std::string(v);
    std::cerr << "Warning: SMSIMDIR not set; using current directory.\n";
    return std::string(".");
}();


void analyze_proton_momentum(const char* recoFile = (smsim_dir + "/d_work/reconp_originnp/Pb208_g050_ypol_np_Pb208_g0500000_reco.root").c_str()) {

    TFile* f = TFile::Open(recoFile);
    if (!f || f->IsZombie()) {
        std::cerr << "Unable to open file: " << recoFile << std::endl;
        return;
    }

    TTree* tree = (TTree*)f->Get("recoTree");
    if (!tree) {
        std::cerr << "recoTree not found in " << recoFile << std::endl;
        f->Close();
        return;
    }

    RecoEvent* recoEvent = nullptr;
    OriginalParticleData* originalData = nullptr;

    tree->SetBranchAddress("recoEvent", &recoEvent);
    if (tree->GetBranch("originalData")) tree->SetBranchAddress("originalData", &originalData);

    Int_t nEntries = tree->GetEntries();
    std::cout << "Number of entries: " << nEntries << std::endl;

    TH1F* h_origP = new TH1F("h_origP", "Original proton momentum |p|;|p| (MeV/c);Events", 200, 0, 2000);
    TH1F* h_recoP = new TH1F("h_recoP", "Reconstructed proton momentum |p|;|p| (MeV/c);Entries", 200, 0, 2000);
    TH1F* h_deltaP = new TH1F("h_deltaP", "Reconstructed - Original momentum difference #Delta p;#Delta p (MeV/c);Events", 200, -1000, 1000);
    TH1F* h_deltaPref = new TH1F("h_deltaPref", "Relative momentum difference (p_reco-p_orig)/p_orig;(p_reco-p_orig)/p_orig;Events", 200, -1.0, 1.0);
    TH2F* h_orig_vs_reco = new TH2F("h_orig_vs_reco", "Original vs Reconstructed momentum;Original |p| (MeV/c);Reconstructed |p| (MeV/c)", 100, 0, 2000, 100, 0, 2000);

    int nMatched = 0;
    int nNoOriginal = 0;
    int nNoReco = 0;

    for (Int_t i = 0; i < nEntries; ++i) {
        tree->GetEntry(i);
        if (!originalData) {
            nNoOriginal++;
            continue;
        }
        if (!originalData->hasProton) {
            nNoOriginal++;
            continue;
        }
        double origP = originalData->protonMomentum.Vect().Mag();
        if (origP <= 0) {
            nNoOriginal++;
            continue;
        }
        h_origP->Fill(origP);

        if (originalData->reconstructedProtonMomenta.empty()) {
            nNoReco++;
            continue;
        }

        // Match by closest momentum magnitude
        double bestDelta = 1e9;
        double bestRecoP = 0;
        for (const auto& rp : originalData->reconstructedProtonMomenta) {
            double rmag = rp.Vect().Mag();
            double delta = rmag - origP;
            if (std::abs(delta) < std::abs(bestDelta)) {
                bestDelta = delta;
                bestRecoP = rmag;
            }
        }
        // Fill histograms
        h_recoP->Fill(bestRecoP);
        h_deltaP->Fill(bestDelta);
        if (origP != 0) h_deltaPref->Fill(bestDelta / origP);
        h_orig_vs_reco->Fill(origP, bestRecoP);
        nMatched++;
    }

    std::cout << "Total entries: " << nEntries << std::endl;
    std::cout << "Events with original proton but no reconstruction: " << nNoReco << std::endl;
    std::cout << "Events without original proton: " << nNoOriginal << std::endl;
    std::cout << "Matched events: " << nMatched << std::endl;

    // Save histograms to file and PNG images
    TFile* out = new TFile("proton_momentum_comparison.root", "RECREATE");
    h_origP->Write();
    h_recoP->Write();
    h_deltaP->Write();
    h_deltaPref->Write();
    h_orig_vs_reco->Write();
    out->Close();

    TCanvas* c1 = new TCanvas("c1", "Momentum distributions", 1200, 500);
    c1->Divide(2,1);
    c1->cd(1); h_origP->SetLineColor(kBlue); h_origP->Draw();
    c1->cd(2); h_recoP->SetLineColor(kRed); h_recoP->Draw();
    c1->SaveAs("proton_p_distributions.png");

    TCanvas* c2 = new TCanvas("c2", "DeltaP", 800, 600);
    h_deltaP->Draw();
    c2->SaveAs("proton_deltaP.png");

    TCanvas* c3 = new TCanvas("c3", "DeltaP_relative", 800, 600);
    h_deltaPref->Draw();
    c3->SaveAs("proton_deltaP_relative.png");

    TCanvas* c4 = new TCanvas("c4", "orig_vs_reco", 800, 800);
    h_orig_vs_reco->Draw("COLZ");
    c4->SaveAs("proton_orig_vs_reco.png");

    std::cout << "Output written to proton_momentum_comparison.root and PNG images created." << std::endl;
    f->Close();
}
