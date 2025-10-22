// 测试磁场旋转功能
void test_rotation() {
    // 加载库
    gSystem->Setenv("LD_LIBRARY_PATH", 
        (TString("/usr/lib/x86_64-linux-gnu:") + gSystem->Getenv("LD_LIBRARY_PATH")).Data());
    gSystem->AddIncludePath("-I/home/tian/workspace/dpol/smsimulator5.5/d_work/sources/include");
    gSystem->AddDynamicPath("/home/tian/workspace/dpol/smsimulator5.5/d_work/sources/build");
    
    if (gSystem->Load("libPDCAnalysisTools.so") < 0) {
        Error("test_rotation", "无法加载库");
        return;
    }
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "  测试磁场旋转功能" << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    // 创建磁场对象
    MagneticField* magField = new MagneticField();
    
    // 尝试从ROOT文件加载（如果存在）
    std::string rootFile = "/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_test.root";
    bool loaded = magField->LoadFromROOTFile(rootFile);
    
    if (!loaded) {
        std::cout << "ROOT文件不存在，需要先运行磁场加载测试" << std::endl;
        delete magField;
        return;
    }
    
    std::cout << "从ROOT文件加载成功!" << std::endl;
    
    // 测试不同角度
    std::vector<double> angles = {0, 15, 30, 45, 60, 90};
    TVector3 testPos(100, 0, 0);  // 测试点 (100, 0, 0)
    
    std::cout << "\n测试点: (" << testPos.X() << ", " << testPos.Y() << ", " << testPos.Z() << ") mm" << std::endl;
    std::cout << "旋转角度 [度] | Bx [T]   | By [T]   | Bz [T]   | |B| [T]" << std::endl;
    std::cout << "-------------|----------|----------|----------|----------" << std::endl;
    
    for (double angle : angles) {
        magField->SetRotationAngle(angle);
        TVector3 B = magField->GetField(testPos);
        
        std::cout << Form("%12.1f | %8.4f | %8.4f | %8.4f | %8.4f", 
                         angle, B.X(), B.Y(), B.Z(), B.Mag()) << std::endl;
    }
    
    std::cout << "\n原始磁场 (磁铁坐标系, 无旋转):" << std::endl;
    TVector3 B_raw = magField->GetFieldRaw(testPos);
    std::cout << Form("原始磁场: (%8.4f, %8.4f, %8.4f) T, |B| = %8.4f T", 
                     B_raw.X(), B_raw.Y(), B_raw.Z(), B_raw.Mag()) << std::endl;
    
    // 验证旋转一致性
    magField->SetRotationAngle(0);
    TVector3 B_0deg = magField->GetField(testPos);
    TVector3 B_0deg_raw = magField->GetFieldRaw(testPos);
    
    double diff = (B_0deg - B_0deg_raw).Mag();
    std::cout << "\n旋转验证:" << std::endl;
    std::cout << Form("0度旋转误差: %.10f T (应该接近0)", diff) << std::endl;
    
    if (diff < 1e-10) {
        std::cout << "✅ 旋转功能验证通过" << std::endl;
    } else {
        std::cout << "❌ 旋转功能可能有问题" << std::endl;
    }
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "  测试完成!" << std::endl;
    std::cout << "======================================" << std::endl;
    
    delete magField;
}