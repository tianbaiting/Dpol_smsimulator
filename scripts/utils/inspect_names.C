#include <TFile.h>
#include <TTree.h>
#include <TClonesArray.h>
#include <TSystem.h>
#include <TString.h>
#include <TSimData.hh>
#include <iostream>
#include <set>

void inspect_names() {
    gSystem->Load("libsmdata.so");
    TFile* file = TFile::Open("/home/tbt/workspace/dpol/smsimulator5.5/d_work/output_tree/test0000.root");
    if (!file || file->IsZombie()) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    TTree* tree = (TTree*)file->Get("tree");
    if (!tree) {
        std::cerr << "Error finding tree!" << std::endl;
        return;
    }

    TClonesArray* fragSimData = nullptr;
    tree->SetBranchAddress("FragSimData", &fragSimData);
    tree->GetEntry(0);

    std::cout << "--- Decoding PDC Hit IDs in Event 0 ---" << std::endl;
    for (int i = 0; i < fragSimData->GetEntries(); ++i) {
        TSimData* hit = (TSimData*)fragSimData->At(i);
        if (hit && hit->fDetectorName.Contains("PDC")) {
            std::cout << "Hit[" << i << "]: "
                      << "fID=" << hit->fID 
                      << ", fModuleName='" << hit->fModuleName.Data() << "'"
                      << ", fPrePosition.Z=" << hit->fPrePosition.Z() 
                      << std::endl;
        }
    }
    file->Close();
}
