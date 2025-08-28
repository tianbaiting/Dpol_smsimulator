#include <TFile.h>
#include <TTree.h>
#include <TClonesArray.h>
#include <TSystem.h>
#include <iostream>
#include "TArtNEBULAPla.hh"
#include "TBeamSimData.hh"

void reconstruct_one_event()
{
    gSystem->Load("/home/tbt/workspace/dpol/smsimulator5.5/smg4lib/lib/libsmdata.so");

    TFile* file = TFile::Open("/home/tbt/workspace/dpol/smsimulator5.5/d_work/output_tree/test0000.root");
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Cannot open ROOT file!" << std::endl;
        return;
    }

    TTree* tree = (TTree*)file->Get("tree");
    if (!tree) {
        std::cerr << "Error: Cannot find tree in ROOT file!" << std::endl;
        file->Close();
        return;
    }

    // 绑定 FragSimData 与 NEBULAPla（若分支存在）
    TClonesArray *fragArr = nullptr;
    TClonesArray *nebulaArr = nullptr;
    if (tree->GetBranch("FragSimData")) tree->SetBranchAddress("FragSimData", &fragArr);
    if (tree->GetBranch("NEBULAPla"))   tree->SetBranchAddress("NEBULAPla", &nebulaArr);
    Long64_t nentries = tree->GetEntries();
    std::cout << "Total events: " << nentries << std::endl;
    for (Long64_t i = 0; i < nentries; ++i) {
        tree->GetEntry(i);
        std::cout << "=== Event " << i << " ===\n";
        if (fragArr) {
            int nfrag = fragArr->GetEntriesFast();
            std::cout << "FragSimData entries: " << nfrag << std::endl;
            for (int j = 0; j < nfrag; ++j) {
                TObject *obj = fragArr->At(j);
                if (!obj) continue;
                // Print class name and try to use Print() if available
                std::cout << "  - Element[" << j << "] class=" << obj->IsA()->GetName();
                // If the class has a Print method, call it via RTTI
                if (obj->InheritsFrom("TNamed") || obj->InheritsFrom("TObject")) {
                    std::cout << " ";
                    obj->Print();
                } else {
                    std::cout << std::endl;
                }
            }
        } else {
            std::cout << "No FragSimData branch." << std::endl;
        }
        if (nebulaArr) {
            int nneb = nebulaArr->GetEntriesFast();
            std::cout << "NEBULAPla entries: " << nneb << std::endl;
            for (int k = 0; k < nneb; ++k) {
                auto *pla = (TArtNEBULAPla*)nebulaArr->At(k);
                if (!pla) continue;
                std::cout << " NEBULA ID=" << pla->GetID()
                          << " TAve=" << pla->GetTAveCal()
                          << " QAve=" << pla->GetQAveCal()
                          << " Pos=(" << pla->GetPos(0) << "," << pla->GetPos(1) << "," << pla->GetPos(2) << ")\n";
            }
        } else {
            std::cout << "No NEBULAPla branch." << std::endl;
        }


        // 只示例前几个事件
        if (i >= 4) break;
    }

    file->Close();
}