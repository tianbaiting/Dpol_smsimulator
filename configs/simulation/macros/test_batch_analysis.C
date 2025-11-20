#include "TSystem.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TCanvas.h"
#include <iostream>

#include "RecoEvent.hh"

// Simple test and validation script
void test_batch_analysis(const char* recoFile = "reco_output.root") {
    
    // 1. Open reconstructed results file
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
    
    // 3. Set branch address
    RecoEvent* recoEvent = nullptr;
    tree->SetBranchAddress("recoEvent", &recoEvent);
    
    // Check if original particle data branch exists
    bool hasOriginalData = (tree->GetBranch("originalData") != nullptr);
    
    // 4. Create histograms for statistics
    TH1F* h_pdcTracks = new TH1F("h_pdcTracks", "PDC Reconstructed Track Count;Tracks;Events", 10, 0, 10);
    TH1F* h_neutrons = new TH1F("h_neutrons", "NEBULA Reconstructed Neutron Count;Neutrons;Events", 10, 0, 10);
    TH1F* h_protonMomentum = new TH1F("h_protonMomentum", "Reconstructed Proton Momentum;|p| (MeV/c);Events", 100, 0, 2000);
    TH1F* h_neutronEnergy = new TH1F("h_neutronEnergy", "Reconstructed Neutron Energy;Energy (MeV);Events", 100, 0, 500);
    
    // If original data exists, create corresponding histograms
    TH1F* h_origProtonMom = nullptr;
    TH1F* h_origNeutronMom = nullptr;
    if (hasOriginalData) {
        h_origProtonMom = new TH1F("h_origProtonMom", "Original Proton Momentum;|p| (MeV/c);Events", 100, 0, 2000);
        h_origNeutronMom = new TH1F("h_origNeutronMom", "Original Neutron Momentum;|p| (MeV/c);Events", 100, 0, 2000);
    }
    
    // 5. Counters
    Long64_t nEvents = tree->GetEntries();
    int pdcHitEvents = 0;
    int nebulaHitEvents = 0;
    int totalTracks = 0;
    int totalNeutrons = 0;
    
    std::cout << "Starting analysis of " << nEvents << " reconstructed events..." << std::endl;
    
    // 6. Event loop
    for (Long64_t i = 0; i < nEvents; ++i) {
        if (i % 1000 == 0) {
            std::cout << "Analyzing event " << i << " / " << nEvents << " (" 
                      << (nEvents > 0 ? (i * 100.0 / nEvents) : 0.0) << "%)" << std::endl;
        }
        
        tree->GetEntry(i);
        
        if (!recoEvent) continue;
        
        // PDC statistics
        int nTracks = recoEvent->tracks.size();
        if (nTracks > 0) {
            pdcHitEvents++;
            totalTracks += nTracks;
            h_pdcTracks->Fill(nTracks);
            
            // Fill reconstructed track momenta (assumed protons)
            for (const auto& track : recoEvent->tracks) {
                TVector3 momentum = track.end - track.start;  // simplified momentum estimate
                h_protonMomentum->Fill(momentum.Mag());
            }
        }
        
        // NEBULA statistics
        int nNeutrons = recoEvent->neutrons.size();
        if (nNeutrons > 0) {
            nebulaHitEvents++;
            totalNeutrons += nNeutrons;
            h_neutrons->Fill(nNeutrons);
            
            // Fill reconstructed neutron energy
            for (const auto& neutron : recoEvent->neutrons) {
                h_neutronEnergy->Fill(neutron.energy);
            }
        }
    }
    
    // 7. Print summary
    std::cout << "\n=== Reconstruction Summary ===" << std::endl;
    std::cout << "Total events: " << nEvents << std::endl;
    std::cout << "PDC hit events: " << pdcHitEvents << " (" << (nEvents > 0 ? (pdcHitEvents*100.0/nEvents) : 0.0) << "%)" << std::endl;
    std::cout << "NEBULA hit events: " << nebulaHitEvents << " (" << (nEvents > 0 ? (nebulaHitEvents*100.0/nEvents) : 0.0) << "%)" << std::endl;
    std::cout << "Average PDC tracks: " << (pdcHitEvents > 0 ? totalTracks*1.0/pdcHitEvents : 0.0) << std::endl;
    std::cout << "Average neutrons: " << (nebulaHitEvents > 0 ? totalNeutrons*1.0/nebulaHitEvents : 0.0) << std::endl;
    
    // 8. Read and display run information
    TNamed* inputInfo = (TNamed*)file->Get("InputFile");
    TNamed* totalInfo = (TNamed*)file->Get("TotalEvents");
    TNamed* pdcInfo = (TNamed*)file->Get("PDCHitEvents");
    TNamed* nebulaInfo = (TNamed*)file->Get("NEBULAHitEvents");
    TNamed* origInfo = (TNamed*)file->Get("OriginalDataEvents");
    
    if (inputInfo) std::cout << "Original input file: " << inputInfo->GetTitle() << std::endl;
    if (totalInfo) std::cout << "Original total events: " << totalInfo->GetTitle() << std::endl;
    if (pdcInfo) std::cout << "Original PDC events: " << pdcInfo->GetTitle() << std::endl;
    if (nebulaInfo) std::cout << "Original NEBULA events: " << nebulaInfo->GetTitle() << std::endl;
    if (origInfo) std::cout << "Events containing original particle data: " << origInfo->GetTitle() << std::endl;
    
    // 9. Optional: create simple visualization
    TCanvas* c1 = new TCanvas("c1", "Reconstruction Summary", 800, 600);
    c1->Divide(2, 2);
    
    c1->cd(1);
    h_pdcTracks->Draw();
    
    c1->cd(2);
    h_neutrons->Draw();
    
    c1->cd(3);
    h_protonMomentum->Draw();
    
    c1->cd(4);
    h_neutronEnergy->Draw();
    
    c1->SaveAs("reco_analysis_plots.png");
    std::cout << "\nPlots saved to: reco_analysis_plots.png" << std::endl;
    
    file->Close();
}

// Quick test function
void quick_test() {
    // First run the batch analysis
    std::cout << "=== Starting batch analysis ===" << std::endl;
    gROOT->ProcessLine(".L batch_analysis.C");
    gROOT->ProcessLine("batch_analysis()");
    
    // Then analyze the results
    std::cout << "\n=== Starting result analysis ===" << std::endl;
    test_batch_analysis();
}