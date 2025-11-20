#include "TSystem.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h" 
#include "TCanvas.h"
#include "TStyle.h"
#include "TLegend.h"
#include <iostream>
#include "TLine.h"

#include "RecoEvent.hh"

// Original particle data structure (must match the definition in batch_analysis.C)
struct OriginalParticleData {
    bool hasProton;
    TLorentzVector protonMomentum;
    TVector3 protonPosition;
    
    bool hasNeutron;
    TLorentzVector neutronMomentum;  
    TVector3 neutronPosition;
    
    OriginalParticleData() : hasProton(false), hasNeutron(false), 
                            protonMomentum(0,0,0,0), neutronMomentum(0,0,0,0),
                            protonPosition(0,0,0), neutronPosition(0,0,0) {}
};

// Detailed comparison analysis script
void analyze_reco_performance(const char* recoFile = "test_reco_output.root") {
    
    gStyle->SetOptStat(1111);
    
    // 1. Open reconstruction result file
    TFile* file = TFile::Open(recoFile);
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Unable to open file " << recoFile << std::endl;
        return;
    }
    
    // 2. Get reconstructed event tree
    TTree* tree = (TTree*)file->Get("recoTree");
    if (!tree) {
        std::cerr << "Error: recoTree not found" << std::endl;
        return;
    }
    
    // 3. Set branch addresses
    RecoEvent* recoEvent = nullptr;
    OriginalParticleData* originalData = nullptr;
    
    tree->SetBranchAddress("recoEvent", &recoEvent);
    
    // Check if original particle data branch exists
    bool hasOriginalData = (tree->GetBranch("originalData") != nullptr);
    if (hasOriginalData) {
        tree->SetBranchAddress("originalData", &originalData);
        std::cout << "Found original particle data branch, will perform comparison analysis" << std::endl;
    } else {
        std::cout << "No original particle data branch found, analyzing reconstruction results only" << std::endl;
    }
    
    // 4. Create analysis histograms
    // Original momentum distributions
    TH1F* h_origProtonPx = new TH1F("h_origProtonPx", "original proton Px;Px (MeV/c);events", 100, -1000, 1000);
    TH1F* h_origProtonPy = new TH1F("h_origProtonPy", "original proton Py;Py (MeV/c);events", 100, -1000, 1000);
    TH1F* h_origProtonPz = new TH1F("h_origProtonPz", "original proton Pz;Pz (MeV/c);events", 100, 0, 2000);
    TH1F* h_origProtonP = new TH1F("h_origProtonP", "original proton momentum;|p| (MeV/c);events", 100, 0, 2000);
    
    TH1F* h_origNeutronPx = new TH1F("h_origNeutronPx", "original neutron Px;Px (MeV/c);events", 100, -1000, 1000);
    TH1F* h_origNeutronPy = new TH1F("h_origNeutronPy", "original neutron Py;Py (MeV/c);events", 100, -1000, 1000);
    TH1F* h_origNeutronPz = new TH1F("h_origNeutronPz", "original neutron Pz;Pz (MeV/c);events", 100, 0, 2000);
    TH1F* h_origNeutronP = new TH1F("h_origNeutronP", "original neutron momentum;|p| (MeV/c);events", 100, 0, 2000);
    
    // Reconstruction result distributions
    TH1F* h_recoNeutronE = new TH1F("h_recoNeutronE", "Reconstructed neutron energy;Energy (MeV);events", 100, 0, 500);
    TH1F* h_recoNeutronBeta = new TH1F("h_recoNeutronBeta", "Reconstructed neutron #beta;#beta;events", 100, 0, 1);
    TH1F* h_recoNeutronTOF = new TH1F("h_recoNeutronTOF", "Reconstructed neutron time-of-flight;TOF (ns);events", 100, 0, 200);
    TH1F* h_recoNeutronP = new TH1F("h_recoNeutronP", "Reconstructed neutron momentum;|p| (MeV/c);events", 100, 0, 2000);
    
    // PDC reconstruction statistics
    TH1F* h_pdcHits = new TH1F("h_pdcHits", "PDC original hits;hits;events", 20, 0, 20);
    TH1F* h_pdcSmearedHits = new TH1F("h_pdcSmearedHits", "PDC smeared hits;hits;events", 20, 0, 20);
    TH1F* h_pdcTracks = new TH1F("h_pdcTracks", "PDC reconstructed tracks;tracks;events", 10, 0, 10);
    
    // Comparison histograms (if original data exists)
    TH2F* h_neutronP_compare = nullptr;
    TH1F* h_neutronP_resolution = nullptr;
    if (hasOriginalData) {
        h_neutronP_compare = new TH2F("h_neutronP_compare", 
            "Neutron momentum comparison;Original |p| (MeV/c);Reconstructed |p| (MeV/c)", 
            50, 0, 2000, 50, 0, 2000);
        h_neutronP_resolution = new TH1F("h_neutronP_resolution", 
            "Neutron momentum resolution;(reco-original)/original;events", 100, -2, 2);
    }
    
    // 5. Counters
    Long64_t nEvents = tree->GetEntries();
    int nOrigProtons = 0, nOrigNeutrons = 0;
    int nRecoTracks = 0, nRecoNeutrons = 0;
    int nMatchedNeutrons = 0;
    
    std::cout << "Starting analysis of " << nEvents << " reconstructed events..." << std::endl;
    
    // 6. Loop over events
    for (Long64_t i = 0; i < nEvents; ++i) {
        if (i % 1000 == 0 && i > 0) {
            std::cout << "Processing event " << i << " / " << nEvents << " (" 
                      << (i * 100.0 / nEvents) << "%)" << std::endl;
        }
        
        tree->GetEntry(i);
        
        if (!recoEvent) continue;
        
        // Analyze original particle data
        if (hasOriginalData && originalData) {
            if (originalData->hasProton) {
                nOrigProtons++;
                TLorentzVector p = originalData->protonMomentum;
                h_origProtonPx->Fill(p.Px());
                h_origProtonPy->Fill(p.Py());
                h_origProtonPz->Fill(p.Pz());
                h_origProtonP->Fill(p.Vect().Mag());
            }
            
            if (originalData->hasNeutron) {
                nOrigNeutrons++;
                TLorentzVector p = originalData->neutronMomentum;
                h_origNeutronPx->Fill(p.Px());
                h_origNeutronPy->Fill(p.Py());
                h_origNeutronPz->Fill(p.Pz());
                h_origNeutronP->Fill(p.Vect().Mag());
            }
        }
        
        // Analyze PDC reconstruction results
        h_pdcHits->Fill(recoEvent->rawHits.size());
        h_pdcSmearedHits->Fill(recoEvent->smearedHits.size());
        h_pdcTracks->Fill(recoEvent->tracks.size());
        nRecoTracks += recoEvent->tracks.size();
        
        // Analyze NEBULA reconstruction results
        for (const auto& neutron : recoEvent->neutrons) {
            nRecoNeutrons++;
            h_recoNeutronE->Fill(neutron.energy);
            h_recoNeutronBeta->Fill(neutron.beta);
            h_recoNeutronTOF->Fill(neutron.timeOfFlight);
            
            TVector3 recoP = neutron.GetMomentum();
            h_recoNeutronP->Fill(recoP.Mag());
            
            // If original data exists, perform comparison
            if (hasOriginalData && originalData && originalData->hasNeutron) {
                double origPMag = originalData->neutronMomentum.Vect().Mag();
                double recoPMag = recoP.Mag();
                
                h_neutronP_compare->Fill(origPMag, recoPMag);
                
                if (origPMag > 0) {
                    double resolution = (recoPMag - origPMag) / origPMag;
                    h_neutronP_resolution->Fill(resolution);
                    nMatchedNeutrons++;
                }
            }
        }
    }
    
    // 7. Print statistics
    std::cout << "\n=== Reconstruction Performance Analysis Results ===" << std::endl;
    std::cout << "Total events: " << nEvents << std::endl;
    
    if (hasOriginalData) {
        std::cout << "Events containing original proton: " << nOrigProtons << " (" << (nOrigProtons*100.0/nEvents) << "%)" << std::endl;
        std::cout << "Events containing original neutron: " << nOrigNeutrons << " (" << (nOrigNeutrons*100.0/nEvents) << "%)" << std::endl;
    }
    
    std::cout << "Total PDC reconstructed tracks: " << nRecoTracks << std::endl;
    std::cout << "Total NEBULA reconstructed neutrons: " << nRecoNeutrons << std::endl;
    
    if (hasOriginalData && nOrigNeutrons > 0) {
        std::cout << "Neutron reconstruction efficiency: " << (nRecoNeutrons*100.0/nOrigNeutrons) << "%" << std::endl;
        std::cout << "Number of matched neutron comparisons: " << nMatchedNeutrons << std::endl;
    }
    
    // 8. Create visualization plots
    TCanvas* c1 = new TCanvas("c1", "Original Particle Momentum Distributions", 1200, 800);
    c1->Divide(2, 2);
    
    c1->cd(1);
    h_origProtonP->SetLineColor(kBlue);
    h_origProtonP->Draw();
    
    c1->cd(2);
    h_origNeutronP->SetLineColor(kRed);
    h_origNeutronP->Draw();
    
    c1->cd(3);
    h_origProtonPz->SetLineColor(kBlue);
    h_origProtonPz->Draw();
    
    c1->cd(4);
    h_origNeutronPz->SetLineColor(kRed);
    h_origNeutronPz->Draw();
    
    c1->SaveAs("original_momentum_distributions.png");
    
    // Reconstruction result plots
    TCanvas* c2 = new TCanvas("c2", "Reconstruction Result Distributions", 1200, 800);
    c2->Divide(2, 2);
    
    c2->cd(1);
    h_pdcTracks->Draw();
    
    c2->cd(2);
    h_recoNeutronE->Draw();
    
    c2->cd(3);
    h_recoNeutronBeta->Draw();
    
    c2->cd(4);
    h_recoNeutronP->SetLineColor(kGreen+2);
    h_recoNeutronP->Draw();
    
    c2->SaveAs("reconstruction_distributions.png");
    
    // Comparison plots (if original data exists)
    if (hasOriginalData && h_neutronP_compare && h_neutronP_resolution) {
        TCanvas* c3 = new TCanvas("c3", "Original vs Reconstructed Comparison", 1200, 600);
        c3->Divide(2, 1);
        
        c3->cd(1);
        h_neutronP_compare->Draw("COLZ");
        
        // Add ideal line (y = x)
        TLine* idealLine = new TLine(0, 0, 2000, 2000);
        idealLine->SetLineColor(kRed);
        idealLine->SetLineWidth(2);
        idealLine->Draw("same");
        
        c3->cd(2);
        h_neutronP_resolution->Draw();
        
        c3->SaveAs("momentum_comparison.png");
        
        // Print momentum resolution statistics
        if (h_neutronP_resolution->GetEntries() > 0) {
            double meanRes = h_neutronP_resolution->GetMean();
            double rmsRes = h_neutronP_resolution->GetRMS();
            std::cout << "Neutron momentum resolution statistics:" << std::endl;
            std::cout << "  Mean relative deviation: " << meanRes << std::endl;
            std::cout << "  RMS: " << rmsRes << std::endl;
        }
    }
    
    std::cout << "\nPlots saved:" << std::endl;
    std::cout << "  - original_momentum_distributions.png (Original momentum distributions)" << std::endl;
    std::cout << "  - reconstruction_distributions.png (Reconstruction result distributions)" << std::endl;
    if (hasOriginalData) {
        std::cout << "  - momentum_comparison.png (Original vs Reconstructed comparison)" << std::endl;
    }
    
    file->Close();
}