// 磁场绘制函数（需要先加载库）
void plot_magnetic_field_func(double rotation_angle = 30.0) {
    std::cout << "\n======================================" << std::endl;
    std::cout << "  磁场分布可视化" << std::endl;
    std::cout << "  旋转角度: " << rotation_angle << " 度" << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    // 创建磁场对象
    MagneticField* magField = new MagneticField();
    
    // 检查是否存在保存的ROOT文件
    std::string rootFile = "/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_test.root";
    std::string originalFile = "/home/tian/workspace/dpol/smsimulator5.5/d_work/geometry/filed_map/180626-1,20T-3000.table";
    
    bool loaded_from_root = false;
    std::ifstream check_root(rootFile.c_str());
    if (check_root.good()) {
        check_root.close();
        std::cout << "从ROOT文件快速加载..." << std::endl;
        loaded_from_root = magField->LoadFromROOTFile(rootFile);
    }
    
    if (!loaded_from_root) {
        std::cout << "加载原始磁场文件（这可能需要几分钟）..." << std::endl;
        if (!magField->LoadFieldMap(originalFile)) {
            Error("plot_magnetic_field_func", "无法加载磁场文件");
            delete magField;
            return;
        }
        magField->SaveAsROOTFile(rootFile);
    }
    
    // 设置旋转角度
    magField->SetRotationAngle(rotation_angle);
    
    std::cout << "正在生成磁场分布图..." << std::endl;
    
    // 创建画布
    TCanvas* c1 = new TCanvas("c1", Form("磁场分布 (旋转%.1f度)", rotation_angle), 1200, 900);
    c1->Divide(2, 2);
    
    // 参数设置
    int nx = 80, ny = 60, nz = 80;  // 减少点数以加快绘制
    double xmin = -300, xmax = 300;
    double ymin = -200, ymax = 200;
    double zmin = -300, zmax = 300;
    
    // 1. XZ平面磁场强度分布 (Y=0)
    c1->cd(1);
    TH2F* h_Bmag_xz = new TH2F("h_Bmag_xz", Form("磁场强度 |B| (XZ平面, Y=0, 旋转%.1f°);X [mm];Z [mm]", rotation_angle), 
                               nx, xmin, xmax, nz, zmin, zmax);
    
    for (int i = 1; i <= nx; i++) {
        for (int j = 1; j <= nz; j++) {
            double x = h_Bmag_xz->GetXaxis()->GetBinCenter(i);
            double z = h_Bmag_xz->GetZaxis()->GetBinCenter(j);
            TVector3 B = magField->GetField(x, 0, z);
            h_Bmag_xz->SetBinContent(i, j, B.Mag());
        }
        if (i % 20 == 0) std::cout << "  XZ平面: " << i << "/" << nx << std::endl;
    }
    
    h_Bmag_xz->Draw("COLZ");
    h_Bmag_xz->GetZaxis()->SetTitle("|B| [T]");
    gPad->SetRightMargin(0.15);
    
    // 2. XY平面磁场强度分布 (Z=0)
    c1->cd(2);
    TH2F* h_Bmag_xy = new TH2F("h_Bmag_xy", Form("磁场强度 |B| (XY平面, Z=0, 旋转%.1f°);X [mm];Y [mm]", rotation_angle), 
                               nx, xmin, xmax, ny, ymin, ymax);
    
    for (int i = 1; i <= nx; i++) {
        for (int j = 1; j <= ny; j++) {
            double x = h_Bmag_xy->GetXaxis()->GetBinCenter(i);
            double y = h_Bmag_xy->GetYaxis()->GetBinCenter(j);
            TVector3 B = magField->GetField(x, y, 0);
            h_Bmag_xy->SetBinContent(i, j, B.Mag());
        }
        if (i % 20 == 0) std::cout << "  XY平面: " << i << "/" << nx << std::endl;
    }
    
    h_Bmag_xy->Draw("COLZ");
    h_Bmag_xy->GetZaxis()->SetTitle("|B| [T]");
    gPad->SetRightMargin(0.15);
    
    // 3. By分量分布 (XZ平面, Y=0)
    c1->cd(3);
    TH2F* h_By_xz = new TH2F("h_By_xz", Form("By分量 (XZ平面, Y=0, 旋转%.1f°);X [mm];Z [mm]", rotation_angle), 
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
    gPad->SetRightMargin(0.15);
    
    // 4. 沿X轴的磁场分布 (Y=0, Z=0)
    c1->cd(4);
    int n_points = 150;
    TGraph* g_Bx = new TGraph();
    TGraph* g_By = new TGraph();
    TGraph* g_Bz = new TGraph();
    TGraph* g_Bmag = new TGraph();
    
    g_Bx->SetLineColor(kRed);
    g_By->SetLineColor(kGreen);
    g_Bz->SetLineColor(kBlue);
    g_Bmag->SetLineColor(kBlack);
    
    g_Bx->SetLineWidth(2);
    g_By->SetLineWidth(2);
    g_Bz->SetLineWidth(2);
    g_Bmag->SetLineWidth(3);
    
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
    mg->SetTitle(Form("沿X轴磁场分布 (Y=0, Z=0, 旋转%.1f°);X [mm];B [T]", rotation_angle));
    
    // 添加图例
    TLegend* leg = new TLegend(0.65, 0.65, 0.89, 0.89);
    leg->AddEntry(g_Bx, "Bx", "l");
    leg->AddEntry(g_By, "By", "l");
    leg->AddEntry(g_Bz, "Bz", "l");
    leg->AddEntry(g_Bmag, "|B|", "l");
    leg->Draw();
    
    c1->Update();
    
    // 保存图像
    std::string pngFile = Form("/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_%.0fdeg.png", rotation_angle);
    std::string pdfFile = Form("/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_%.0fdeg.pdf", rotation_angle);
    
    c1->SaveAs(pngFile.c_str());
    c1->SaveAs(pdfFile.c_str());
    
    // 打印统计信息
    TVector3 B_center = magField->GetField(0, 0, 0);
    TVector3 B_center_raw = magField->GetFieldRaw(0, 0, 0);
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "磁场分布图已生成!" << std::endl;
    std::cout << "文件: " << pngFile << std::endl;
    std::cout << "文件: " << pdfFile << std::endl;
    std::cout << "======================================" << std::endl;
    
    std::cout << "\n磁场统计信息:" << std::endl;
    std::cout << Form("旋转角度: %.1f 度 (绕Y轴负方向)", magField->GetRotationAngle()) << std::endl;
    std::cout << Form("中心点(0,0,0)磁场(实验室系): (%.4f, %.4f, %.4f) T", B_center.X(), B_center.Y(), B_center.Z()) << std::endl;
    std::cout << Form("中心点磁场强度: %.4f T", B_center.Mag()) << std::endl;
    std::cout << Form("中心点磁场(磁铁系): (%.4f, %.4f, %.4f) T", B_center_raw.X(), B_center_raw.Y(), B_center_raw.Z()) << std::endl;
    std::cout << "======================================" << std::endl;
    
    // 清理
    delete magField;
}