// Usage: root -l 'scripts/visualization/visualize_3d_field.C()'
/**
 * 三维磁场可视化宏
 * 功能：
 * 1. 可视化磁场强度的等值面
 * 2. 绘制磁力线（向量场）
 * 3. 交互式3D视图
 */

void visualize_3d_field(const char* mode = "magnitude") {
    // 加载库
    if (gSystem->Load("../sources/build/libPDCAnalysisTools.so") < 0) {
        Error("visualize_3d_field", "无法加载PDCAnalysisTools库");
        return;
    }
    
    // 创建磁场对象
    MagneticField* magField = new MagneticField();
    
    // 加载磁场文件
    TString smsDir = gSystem->Getenv("SMS_DIR");
    if (smsDir.IsNull()) smsDir = "/home/tian/workspace/dpol/smsimulator5.5";
    
    std::string fieldFile = Form("%s/d_work/geometry/filed_map/180626-1,20T-3000.table", smsDir.Data());
    
    if (!magField->LoadFieldMap(fieldFile)) {
        Error("visualize_3d_field", "无法加载磁场文件: %s", fieldFile.c_str());
        return;
    }
    
    // 设置磁场旋转角度
    magField->SetRotationAngle(30.0);
    
    std::cout << "磁场加载成功！开始可视化..." << std::endl;
    
    // 根据模式选择可视化方法
    TString modeStr(mode);
    modeStr.ToLower();
    
    if (modeStr.Contains("magnitude") || modeStr.Contains("mag")) {
        visualize_field_magnitude(magField);
    }
    else if (modeStr.Contains("vector") || modeStr.Contains("line")) {
        visualize_field_lines(magField);
    }
    else if (modeStr.Contains("both") || modeStr.Contains("all")) {
        visualize_field_combined(magField);
    }
    else {
        std::cout << "可用模式：" << std::endl;
        std::cout << "  magnitude - 磁场强度等值面" << std::endl;
        std::cout << "  vector    - 磁力线" << std::endl;
        std::cout << "  both      - 组合视图" << std::endl;
        visualize_field_magnitude(magField);
    }
}

// 可视化磁场强度等值面
void visualize_field_magnitude(MagneticField* magField) {
    std::cout << "绘制磁场强度等值面..." << std::endl;
    
    // 创建3D直方图用于存储磁场强度
    int nx = 60, ny = 40, nz = 60;
    double xmin = -1500, xmax = 1500;
    double ymin = -400, ymax = 400;
    double zmin = -1500, zmax = 1500;
    
    TH3F* h3_field = new TH3F("h3_field", "磁场强度 |B|;X [mm];Y [mm];Z [mm]", 
                              nx, xmin, xmax, ny, ymin, ymax, nz, zmin, zmax);
    
    // 填充3D直方图
    std::cout << "计算磁场强度分布..." << std::endl;
    int totalBins = nx * ny * nz;
    int processedBins = 0;
    
    for (int ix = 1; ix <= nx; ix++) {
        for (int iy = 1; iy <= ny; iy++) {
            for (int iz = 1; iz <= nz; iz++) {
                double x = h3_field->GetXaxis()->GetBinCenter(ix);
                double y = h3_field->GetYaxis()->GetBinCenter(iy);
                double z = h3_field->GetZaxis()->GetBinCenter(iz);
                
                TVector3 B = magField->GetField(x, y, z);
                double magnitude = B.Mag();
                
                if (magnitude > 0.01) { // 只显示有意义的磁场强度
                    h3_field->SetBinContent(ix, iy, iz, magnitude);
                }
                
                processedBins++;
                if (processedBins % 10000 == 0) {
                    std::cout << "进度: " << (100.0 * processedBins / totalBins) << "%" << std::endl;
                }
            }
        }
    }
    
    // 创建画布
    TCanvas* c1 = new TCanvas("c1", "磁场强度3D可视化", 1200, 900);
    
    // 设置等值面绘制
    h3_field->SetFillColor(kBlue);
    h3_field->SetFillColorAlpha(kBlue, 0.3);
    
    // 绘制等值面
    std::cout << "绘制3D图形..." << std::endl;
    h3_field->Draw("ISO");
    
    // 添加颜色标尺
    gPad->Update();
    TPaletteAxis* palette = (TPaletteAxis*)h3_field->GetListOfFunctions()->FindObject("palette");
    if (palette) {
        palette->SetX1NDC(0.85);
        palette->SetX2NDC(0.9);
        palette->SetY1NDC(0.1);
        palette->SetY2NDC(0.9);
    }
    
    // 设置视角
    gPad->SetTheta(30);
    gPad->SetPhi(45);
    
    c1->Update();
    
    // 保存图片
    c1->SaveAs("magnetic_field_3d_magnitude.png");
    c1->SaveAs("magnetic_field_3d_magnitude.pdf");
    
    std::cout << "磁场强度可视化完成！" << std::endl;
    std::cout << "文件已保存: magnetic_field_3d_magnitude.png/pdf" << std::endl;
}

// 可视化磁力线
void visualize_field_lines(MagneticField* magField) {
    std::cout << "绘制磁力线..." << std::endl;
    
    // 创建画布
    TCanvas* c2 = new TCanvas("c2", "磁力线可视化", 1200, 900);
    
    // 创建3D视图
    TView3D* view = TView3D::CreateView(1);
    view->SetRange(-1500, -400, -1500, 1500, 400, 1500);
    
    // 绘制磁力线的起始点网格
    std::vector<TVector3> startPoints;
    
    // 在y=0平面上创建起始点网格
    for (double x = -1000; x <= 1000; x += 200) {
        for (double z = -1000; z <= 1000; z += 200) {
            TVector3 B = magField->GetField(x, 0, z);
            if (B.Mag() > 0.1) { // 只在有强磁场的地方开始磁力线
                startPoints.push_back(TVector3(x, 0, z));
            }
        }
    }
    
    std::cout << "绘制 " << startPoints.size() << " 条磁力线..." << std::endl;
    
    // 绘制每条磁力线
    for (size_t i = 0; i < startPoints.size(); i++) {
        drawFieldLine(magField, startPoints[i], i);
        
        if (i % 10 == 0) {
            std::cout << "进度: " << (100.0 * i / startPoints.size()) << "%" << std::endl;
        }
    }
    
    // 设置坐标轴
    TH3F* frame = new TH3F("frame", "磁力线;X [mm];Y [mm];Z [mm]", 
                           2, -1500, 1500, 2, -400, 400, 2, -1500, 1500);
    frame->Draw();
    
    // 设置视角
    gPad->SetTheta(30);
    gPad->SetPhi(45);
    
    c2->Update();
    
    // 保存图片
    c2->SaveAs("magnetic_field_lines.png");
    c2->SaveAs("magnetic_field_lines.pdf");
    
    std::cout << "磁力线可视化完成！" << std::endl;
    std::cout << "文件已保存: magnetic_field_lines.png/pdf" << std::endl;
}

// 绘制单条磁力线
void drawFieldLine(MagneticField* magField, const TVector3& startPoint, int lineIndex) {
    std::vector<double> x_points, y_points, z_points;
    
    TVector3 currentPos = startPoint;
    double stepSize = 10.0; // 10mm步长
    int maxSteps = 500;
    
    for (int step = 0; step < maxSteps; step++) {
        // 检查是否超出合理范围
        if (currentPos.Mag() > 2000) break;
        
        TVector3 B = magField->GetField(currentPos.X(), currentPos.Y(), currentPos.Z());
        if (B.Mag() < 0.01) break; // 磁场太弱则停止
        
        // 记录当前位置
        x_points.push_back(currentPos.X());
        y_points.push_back(currentPos.Y());
        z_points.push_back(currentPos.Z());
        
        // 沿磁场方向前进
        TVector3 direction = B.Unit();
        currentPos += direction * stepSize;
    }
    
    // 绘制磁力线
    if (x_points.size() > 2) {
        TPolyLine3D* line = new TPolyLine3D(x_points.size());
        for (size_t i = 0; i < x_points.size(); i++) {
            line->SetPoint(i, x_points[i], y_points[i], z_points[i]);
        }
        
        // 根据磁力线长度设置颜色
        int color = kBlue + (lineIndex % 7);
        line->SetLineColor(color);
        line->SetLineWidth(2);
        line->Draw("same");
    }
}

// 组合视图：磁场强度 + 磁力线
void visualize_field_combined(MagneticField* magField) {
    std::cout << "绘制组合视图..." << std::endl;
    
    // 创建画布
    TCanvas* c3 = new TCanvas("c3", "磁场组合可视化", 1400, 1000);
    c3->Divide(2, 1);
    
    // 左侧：磁场强度切片
    c3->cd(1);
    draw_field_slice(magField, "XZ", 0); // y=0切片
    
    // 右侧：向量场
    c3->cd(2);
    draw_vector_field(magField);
    
    c3->Update();
    
    // 保存图片
    c3->SaveAs("magnetic_field_combined.png");
    c3->SaveAs("magnetic_field_combined.pdf");
    
    std::cout << "组合视图完成！" << std::endl;
    std::cout << "文件已保存: magnetic_field_combined.png/pdf" << std::endl;
}

// 绘制磁场强度切片
void draw_field_slice(MagneticField* magField, const char* plane, double position) {
    int nx = 100, ny = 100;
    double xmin = -1500, xmax = 1500;
    double ymin = -1500, ymax = 1500;
    
    TH2F* h2 = nullptr;
    
    if (TString(plane) == "XZ") {
        h2 = new TH2F("h2_xz", Form("磁场强度 (Y = %.0f mm);X [mm];Z [mm]", position),
                      nx, xmin, xmax, ny, ymin, ymax);
        
        for (int ix = 1; ix <= nx; ix++) {
            for (int iy = 1; iy <= ny; iy++) {
                double x = h2->GetXaxis()->GetBinCenter(ix);
                double z = h2->GetYaxis()->GetBinCenter(iy);
                
                TVector3 B = magField->GetField(x, position, z);
                h2->SetBinContent(ix, iy, B.Mag());
            }
        }
    }
    
    if (h2) {
        h2->SetStats(0);
        h2->Draw("COLZ");
        
        // 添加等高线
        h2->SetContour(20);
        h2->Draw("CONT3 same");
    }
}

// 绘制向量场
void draw_vector_field(MagneticField* magField) {
    // 创建2D向量场显示（y=0平面）
    int nx = 20, nz = 20;
    double xmin = -1000, xmax = 1000;
    double zmin = -1000, zmax = 1000;
    
    TH2F* frame = new TH2F("frame_vec", "磁场向量 (Y = 0);X [mm];Z [mm]",
                           2, xmin, xmax, 2, zmin, zmax);
    frame->SetStats(0);
    frame->Draw();
    
    // 绘制向量箭头
    double dx = (xmax - xmin) / nx;
    double dz = (zmax - zmin) / nz;
    
    for (int ix = 0; ix < nx; ix++) {
        for (int iz = 0; iz < nz; iz++) {
            double x = xmin + (ix + 0.5) * dx;
            double z = zmin + (iz + 0.5) * dz;
            
            TVector3 B = magField->GetField(x, 0, z);
            if (B.Mag() < 0.01) continue;
            
            // 归一化向量用于显示
            TVector3 Bnorm = B.Unit();
            double scale = 50; // 箭头长度缩放
            
            TArrow* arrow = new TArrow(x, z, 
                                     x + Bnorm.X() * scale, 
                                     z + Bnorm.Z() * scale, 
                                     0.01, "|>");
            
            // 根据磁场强度设置颜色
            int color = kRed;
            if (B.Mag() > 1.0) color = kRed;
            else if (B.Mag() > 0.5) color = kOrange;
            else color = kYellow;
            
            arrow->SetLineColor(color);
            arrow->SetFillColor(color);
            arrow->Draw();
        }
    }
}

// 主函数调用示例
void run_visualization() {
    std::cout << "磁场3D可视化工具" << std::endl;
    std::cout << "==================" << std::endl;
    
    // 可视化磁场强度
    visualize_3d_field("magnitude");
    
    // 等待用户输入继续
    std::cout << "按回车键继续绘制磁力线..." << std::endl;
    getchar();
    
    // 可视化磁力线
    visualize_3d_field("vector");
    
    // 等待用户输入继续
    std::cout << "按回车键继续绘制组合视图..." << std::endl;
    getchar();
    
    // 组合视图
    visualize_3d_field("both");
    
    std::cout << "所有可视化完成！" << std::endl;
}