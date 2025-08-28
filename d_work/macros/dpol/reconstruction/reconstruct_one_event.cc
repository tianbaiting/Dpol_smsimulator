void reconstruct_one_event()
{
    gSystem->Load("/home/tbt/workspace/dpol/smsimulator5.5/smg4lib/lib/libsmdata.so");
    
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

    // 读取事件
    for (Long64_t i = 0; i < tree->GetEntries(); i++) {
        tree->GetEntry(i);
        // 处理事件数据

    }

    file->Close();
}