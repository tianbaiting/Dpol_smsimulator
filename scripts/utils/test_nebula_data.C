#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include <iostream>

void test_nebula_data() {
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir) {
        std::cout << "SMSIMDIR 环境变量未设置!" << std::endl;
        return;
    }
    
    const char* filename = Form("%s/d_work/output_tree/testry0000.root", smsDir);
    TFile* file = TFile::Open(filename);
    if (!file || file->IsZombie()) {
        std::cout << "无法打开文件: " << filename << std::endl;
        return;
    }
    
    TTree* tree = (TTree*)file->Get("tree");
    if (!tree) {
        std::cout << "无法找到 tree" << std::endl;
        file->Close();
        return;
    }
    
    // 检查分支
    TObjArray* branches = tree->GetListOfBranches();
    std::cout << "文件 " << filename << " 中的分支:" << std::endl;
    for (int i = 0; i < branches->GetEntriesFast(); ++i) {
        std::cout << "  " << branches->At(i)->GetName() << std::endl;
    }
    
    // 检查 NEBULAPla 分支
    TBranch* nebulaBranch = tree->GetBranch("NEBULAPla");
    if (nebulaBranch) {
        std::cout << "\n找到 NEBULAPla 分支!" << std::endl;
        
        TClonesArray* nebulaData = nullptr;
        tree->SetBranchAddress("NEBULAPla", &nebulaData);
        
        // 检查前几个事件
        Long64_t nentries = tree->GetEntries();
        std::cout << "总事件数: " << nentries << std::endl;
        
        for (int evt = 0; evt < std::min(10LL, nentries); ++evt) {
            tree->GetEntry(evt);
            int nhits = nebulaData ? nebulaData->GetEntriesFast() : 0;
            if (nhits > 0) {
                std::cout << "事件 " << evt << ": " << nhits << " 个 NEBULA 击中" << std::endl;
            }
        }
    } else {
        std::cout << "\n没有找到 NEBULAPla 分支!" << std::endl;
    }
    
    file->Close();
}