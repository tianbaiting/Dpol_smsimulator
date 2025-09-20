#include <TFile.h>
#include <TTree.h>
#include <TClonesArray.h>
#include <TString.h>
#include <iostream>

// Include the header for the custom data class
// The setup.sh script should configure ROOT_INCLUDE_PATH correctly
#include "TSimData.hh"

// Define the function that will be executed by ROOT
void extract_pdc_data() {
    // --- Configuration ---
    const char* filePath = "/home/tbt/workspace/dpol/smsimulator5.5/d_work/output_tree/test0000.root";
    const char* treeName = "tree"; // Default tree name, change if necessary
    TString targetDetector = "PDC";
    // ---

    // Load the shared library that defines TSimData
    // This ensures ROOT knows about the class structure.
    gSystem->Load("libsmdata.so");

    TFile *file = TFile::Open(filePath);
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
        return;
    }

    TTree *tree = (TTree*)file->Get(treeName);
    if (!tree) {
        std::cerr << "Error: Could not find TTree with name " << treeName << std::endl;
        file->Close();
        return;
    }

    std::cout << "Successfully opened file: " << filePath << std::endl;
    std::cout << "Reading TTree: " << treeName << std::endl;

    // Prepare to read the FragSimData branch
    TClonesArray *fragSimDataArray = nullptr;
    tree->SetBranchAddress("FragSimData", &fragSimDataArray);

    Long64_t nEntries = tree->GetEntries();
    std::cout << "--- Starting Event Loop (" << nEntries << " events) ---" << std::endl;

    // Loop over all events in the TTree
    for (Long64_t i = 0; i < nEntries; ++i) {
        tree->GetEntry(i);
        std::cout << "\n--- Event " << i << " ---" << std::endl;

        if (!fragSimDataArray) continue;

        Int_t numHits = fragSimDataArray->GetEntries();
        if (numHits == 0) {
            std::cout << "  No hits in this event." << std::endl;
            continue;
        }

        int pdcHitCounter = 0;
        // Loop over all hits in the current event
        for (Int_t j = 0; j < numHits; ++j) {
            TSimData *hit = (TSimData*)fragSimDataArray->At(j);
            if (!hit) continue;

            // Check if the hit is in the detector we are interested in
            if (hit->fDetectorName.Contains(targetDetector)) {
                pdcHitCounter++;
                
                // Extract position
                TVector3 position = hit->fPrePosition;

                // Calculate deposited energy
                // E_kin = E_total - mass
                TLorentzVector preMom = hit->fPreMomentum;
                TLorentzVector postMom = hit->fPostMomentum;

                Double_t mass = preMom.M(); // Mass is invariant
                Double_t preKinE = preMom.E() - mass;
                Double_t postKinE = postMom.E() - mass;
                Double_t energyDeposited = preKinE - postKinE;

                // Print the information
                std::cout << "  Hit " << j << " in " << hit->fDetectorName.Data() << ":" << std::endl;
                printf("    Position (x,y,z) [mm]: (%.2f, %.2f, %.2f)\n", position.X(), position.Y(), position.Z());
                printf("    Energy Deposited [MeV]: %.4f\n", energyDeposited);
            }
        }
        if (pdcHitCounter == 0){
            std::cout << "  No hits found in '" << targetDetector.Data() << "'." << std::endl;
        }
    }

    // Clean up
    file->Close();
    delete file;
}
