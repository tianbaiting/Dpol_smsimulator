// 测试 MagneticField 类的功能（需要先加载库）
void test_magnetic_field_func() {
    std::cout << "\n======================================" << std::endl;
    std::cout << "  测试 MagneticField 类" << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    // 创建 MagneticField 对象
    MagneticField* magField = new MagneticField();
    
    // 磁场文件路径
    std::string fieldFile = "/home/tian/workspace/dpol/smsimulator5.5/d_work/geometry/filed_map/180626-1,20T-3000.table";
    
    std::cout << "正在加载磁场文件..." << std::endl;
    std::cout << "文件路径: " << fieldFile << std::endl;
    
    // 加载磁场文件
    bool success = magField->LoadFieldMap(fieldFile);
    
    if (!success) {
        Error("test_magnetic_field_func", "无法加载磁场文件!");
        delete magField;
        return;
    }
    
    std::cout << "\n磁场加载成功!" << std::endl;
    
    // 测试几个点的磁场值
    std::vector<TVector3> testPoints = {
        TVector3(0, 0, 0),         // 原点
        TVector3(100, -200, 50),   // 任意点1
        TVector3(-50, 100, 150),   // 任意点2
        TVector3(200, 300, 250)    // 任意点3
    };
    
    std::cout << "\n=== 磁场测试结果 ===" << std::endl;
    
    for (size_t i = 0; i < testPoints.size(); i++) {
        TVector3 pos = testPoints[i];
        TVector3 field = magField->GetField(pos);
        
        std::cout << Form("点 %zu: (%.1f, %.1f, %.1f) mm", i+1, pos.X(), pos.Y(), pos.Z()) << std::endl;
        
        if (magField->IsInRange(pos)) {
            std::cout << Form("  磁场: (%.6f, %.6f, %.6f) T", field.X(), field.Y(), field.Z()) << std::endl;
            std::cout << Form("  磁场强度: %.6f T", field.Mag()) << std::endl;
        } else {
            std::cout << "  位置超出磁场范围!" << std::endl;
        }
        std::cout << std::endl;
    }
    
    // 保存为ROOT文件（可选）
    std::string rootFile = "/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_test.root";
    std::cout << "正在保存磁场数据到 ROOT 文件: " << rootFile << std::endl;
    magField->SaveAsROOTFile(rootFile);
    
    // 测试从ROOT文件加载
    std::cout << "\n测试从 ROOT 文件加载磁场..." << std::endl;
    MagneticField* magField2 = new MagneticField();
    if (magField2->LoadFromROOTFile(rootFile)) {
        std::cout << "从 ROOT 文件加载成功!" << std::endl;
        
        // 验证数据一致性
        TVector3 testPos(0, 0, 0);
        TVector3 field1 = magField->GetField(testPos);
        TVector3 field2 = magField2->GetField(testPos);
        
        if ((field1 - field2).Mag() < 1e-10) {
            std::cout << "数据一致性验证: ✅ 通过" << std::endl;
        } else {
            std::cout << "数据一致性验证: ❌ 失败" << std::endl;
        }
    }
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "  测试完成!" << std::endl;
    std::cout << "======================================" << std::endl;
    
    // 清理
    delete magField;
    delete magField2;
}