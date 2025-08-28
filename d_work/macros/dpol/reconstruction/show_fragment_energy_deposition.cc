void show_fragment_energy_deposition()
{
    gSystem->Load("/home/tbt/workspace/dpol/smsimulator5.5/smg4lib/lib/libsmdata.so");
    // 打开ROOT文件
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

    std::cout << "=== 分析第一个事件的能量沉积点信息 ===" << std::endl;

    // 定义变量以存储分支数据
    Int_t FragSimData_; // 条目数
    Double_t fEnergyDeposit[1000]; // 假设最大条目数为1000
    TVector3 fPostPosition[1000];

    // 设置分支地址
    tree->SetBranchAddress("FragSimData", &FragSimData_);
    tree->SetBranchAddress("FragSimData.fEnergyDeposit", fEnergyDeposit);
    tree->SetBranchAddress("FragSimData.fPostPosition", fPostPosition);

    // 读取第一个事件
    if (tree->GetEntry(0) <= 0) {
        std::cerr << "Error: Failed to read entry!" << std::endl;
        file->Close();
        return;
    }

    // 打印能量沉积点的位置和能量
    std::cout << "=== 能量沉积点信息 ===" << std::endl;
    for (Int_t i = 0; i < FragSimData_; ++i) {
        const TVector3& pos = fPostPosition[i];
        std::cout << "位置: (" << pos.X() << ", " << pos.Y() << ", " << pos.Z() << ")"
                  << " 能量: " << fEnergyDeposit[i] << " MeV" << std::endl;
    }

    file->Close();
    std::cout << "\n分析完成!" << std::endl;
}