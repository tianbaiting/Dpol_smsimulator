// #ifdef __CINT__
// #pragma link C++ class TBeamSimData+;
// #pragma link C++ class TSimData+;
// #pragma link C++ class TFragSimParameter+;
// #pragma link C++ class TSimParameter+;
// #pragma link C++ class TNEBULASimParameter+;
// #pragma link C++ class pair<int, TDetectorSimParameter>+;
// #pragma link C++ class TDetectorSimParameter+;
// #pragma link C++ class TRunSimParameter+;
// #endif

#include "TBeamSimData.hh"

void check_file() {
    // gSystem->Load("/home/tbt/workspace/dpol/smsimulator5.5/smg4lib/lib/libsmdata.so");

    TFile* file = TFile::Open("/home/tbt/workspace/dpol/smsimulator5.5/d_work/output_tree/d+Pb208E190g050xyz_np_sametime0000.root");
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Cannot open ROOT file!" << std::endl;
        return;
    }

    // 获取树
    TTree* tree = (TTree*)file->Get("tree");
    if (!tree) {
        std::cerr << "Error: Cannot find tree in ROOT file!" << std::endl;
        file->Close();
        return;
    }

    tree->Print();
    file->ls();
    file->Close();
}