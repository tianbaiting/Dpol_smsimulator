// 磁场分布可视化宏
void plot_magnetic_field() {
    // 加载库
    static bool loaded = false;
    if (!loaded) {
        gSystem->Setenv("LD_LIBRARY_PATH", 
            (TString("/usr/lib/x86_64-linux-gnu:") + gSystem->Getenv("LD_LIBRARY_PATH")).Data());
        gSystem->AddIncludePath("-I/home/tian/workspace/dpol/smsimulator5.5/d_work/sources/include");
        gSystem->AddDynamicPath("/home/tian/workspace/dpol/smsimulator5.5/d_work/sources/build");
        
        if (gSystem->Load("libPDCAnalysisTools.so") < 0) {
            Error("plot_magnetic_field", "无法加载库");
            return;
        }
        loaded = true;
        Info("plot_magnetic_field", "库加载成功");
    }
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "  磁场分布可视化" << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    // 创建磁场对象
    MagneticField* magField = new MagneticField();
    
    // 检查是否存在保存的ROOT文件，如果有则直接加载
    std::string rootFile = "/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_test.root";
    std::string originalFile = "/home/tian/workspace/dpol/smsimulator5.5/d_work/geometry/filed_map/180626-1,20T-3000.table";
    
    bool loaded_from_root = false;
    std::ifstream check_root(rootFile.c_str());
    if (check_root.good()) {
        check_root.close();
        std::cout << "发现已保存的ROOT文件，正在加载..." << std::endl;
        loaded_from_root = magField->LoadFromROOTFile(rootFile);
    }
    
    if (!loaded_from_root) {
        std::cout << "加载原始磁场文件..." << std::endl;
        if (!magField->LoadFieldMap(originalFile)) {
            Error("plot_magnetic_field", "无法加载磁场文件");
            delete magField;
            return;
        }
        // 保存为ROOT文件以便下次快速加载
        magField->SaveAsROOTFile(rootFile);
    }
    
    // 设置旋转角度（默认30度，可以修改）
    double rotation_angle = 30.0;
    magField->SetRotationAngle(rotation_angle);
    
    std::cout << "磁场加载完成，旋转角度: " << rotation_angle << " 度" << std::endl;
    std::cout << "正在生成磁场分布图..." << std::endl;
    
    // 创建画布
    TCanvas* c1 = new TCanvas("c1", "磁场分布", 1200, 900);
    c1->Divide(2, 2);
    
    // 1. XZ平面磁场强度分布 (Y=0)
    c1->cd(1);
    int nx = 100, nz = 100;
    double xmin = -300, xmax = 300;
    double zmin = -300, zmax = 300;
    
    TH2F* h_Bmag_xz = new TH2F("h_Bmag_xz", "磁场强度 |B| (XZ平面, Y=0);X [mm];Z [mm]", 
                               nx, xmin, xmax, nz, zmin, zmax);
    
    for (int i = 1; i <= nx; i++) {
        for (int j = 1; j <= nz; j++) {
            double x = h_Bmag_xz->GetXaxis()->GetBinCenter(i);
            double z = h_Bmag_xz->GetZaxis()->GetBinCenter(j);
            TVector3 B = magField->GetField(x, 0, z);
            h_Bmag_xz->SetBinContent(i, j, B.Mag());
        }
        if (i % 20 == 0) std::cout << "XZ平面: " << i << "/" << nx << std::endl;
    }
    
    h_Bmag_xz->Draw("COLZ");
    h_Bmag_xz->GetZaxis()->SetTitle("|B| [T]");
    
    // 2. XY平面磁场强度分布 (Z=0)
    c1->cd(2);
    int ny = 100;
    double ymin = -200, ymax = 200;
    
    TH2F* h_Bmag_xy = new TH2F("h_Bmag_xy", "磁场强度 |B| (XY平面, Z=0);X [mm];Y [mm]", 
                               nx, xmin, xmax, ny, ymin, ymax);
    
    for (int i = 1; i <= nx; i++) {
        for (int j = 1; j <= ny; j++) {
            double x = h_Bmag_xy->GetXaxis()->GetBinCenter(i);
            double y = h_Bmag_xy->GetYaxis()->GetBinCenter(j);
            TVector3 B = magField->GetField(x, y, 0);
            h_Bmag_xy->SetBinContent(i, j, B.Mag());
        }
        if (i % 20 == 0) std::cout << "XY平面: " << i << "/" << nx << std::endl;
    }
    
    h_Bmag_xy->Draw("COLZ");
    h_Bmag_xy->GetZaxis()->SetTitle("|B| [T]");
    
    // 3. By分量分布 (XZ平面, Y=0)
    c1->cd(3);
    TH2F* h_By_xz = new TH2F("h_By_xz", "By分量 (XZ平面, Y=0);X [mm];Z [mm]", 
                             nx, xmin, xmax, nz, zmin, zmax);
    
    for (int i = 1; i <= nx; i++) {
        for (int j = 1; j <= nz; j++) {
            double x = h_By_xz->GetXaxis()->GetBinCenter(i);
            double z = h_By_xz->GetZaxis()->GetBinCenter(j);
            TVector3 B = magField->GetField(x, 0, z);
            h_By_xz->SetBinContent(i, j, B.Y());
        }
    }
    
    h_By_xz->Draw("COLZ");
    h_By_xz->GetZaxis()->SetTitle("By [T]");
    
    // 4. 沿X轴的磁场分布 (Y=0, Z=0)
    c1->cd(4);
    int n_points = 200;
    TGraph* g_Bx = new TGraph();
    TGraph* g_By = new TGraph();
    TGraph* g_Bz = new TGraph();
    TGraph* g_Bmag = new TGraph();
    
    g_Bx->SetLineColor(kRed);
    g_By->SetLineColor(kGreen);
    g_Bz->SetLineColor(kBlue);
    g_Bmag->SetLineColor(kBlack);
    g_Bmag->SetLineWidth(2);
    
    for (int i = 0; i < n_points; i++) {
        double x = xmin + (xmax - xmin) * i / (n_points - 1);
        TVector3 B = magField->GetField(x, 0, 0);
        
        g_Bx->SetPoint(i, x, B.X());
        g_By->SetPoint(i, x, B.Y());
        g_Bz->SetPoint(i, x, B.Z());
        g_Bmag->SetPoint(i, x, B.Mag());
    }
    
    // 创建多图
    TMultiGraph* mg = new TMultiGraph();
    mg->Add(g_Bx);
    mg->Add(g_By);
    mg->Add(g_Bz);
    mg->Add(g_Bmag);
    
    mg->Draw("AL");
    mg->SetTitle("沿X轴磁场分布 (Y=0, Z=0);X [mm];B [T]");
    
    // 添加图例
    TLegend* leg = new TLegend(0.7, 0.7, 0.9, 0.9);
    leg->AddEntry(g_Bx, "Bx", "l");
    leg->AddEntry(g_By, "By", "l");
    leg->AddEntry(g_Bz, "Bz", "l");
    leg->AddEntry(g_Bmag, "|B|", "l");
    leg->Draw();
    
    c1->Update();
    
    // 保存图像
    c1->SaveAs("/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_plot.png");
    c1->SaveAs("/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_plot.pdf");
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "磁场分布图已生成并保存:" << std::endl;
    std::cout << "- magnetic_field_plot.png" << std::endl;
    std::cout << "- magnetic_field_plot.pdf" << std::endl;
    std::cout << "======================================" << std::endl;
    
    // 打印一些统计信息
    TVector3 B_center = magField->GetField(0, 0, 0);
    std::cout << "\n磁场统计信息:" << std::endl;
    std::cout << Form("中心点(0,0,0)磁场: (%.3f, %.3f, %.3f) T", B_center.X(), B_center.Y(), B_center.Z()) << std::endl;
    std::cout << Form("中心点磁场强度: %.3f T", B_center.Mag()) << std::endl;
    std::cout << Form("旋转角度: %.1f 度", magField->GetRotationAngle()) << std::endl;
    
    // 清理
    delete magField;
}