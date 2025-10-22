void test_trajectory() {
    gSystem->Load("libPDCAnalysisTools.so");
    
    // 测试单独加载轨迹功能
    GeometryManager geo;
    geo.LoadGeometry("/home/tian/workspace/dpol/smsimulator5.5/d_work/geometry/5deg_1.2T.mac");

    EventDataReader reader("/home/tian/workspace/dpol/smsimulator5.5/d_work/output_tree/testry0000.root");
    if (!reader.IsOpen()) {
        Error("test_trajectory", "无法打开数据文件!");
        return;
    }

    // 加载磁场
    MagneticField magField;
    if (!magField.LoadFromROOTFile("/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_test.root")) {
        Error("test_trajectory", "磁场文件加载失败!");
        return;
    }
    magField.SetRotationAngle(30.0);

    // 读取事件0的beam数据
    if (reader.GoToEvent(0)) {
        const std::vector<TBeamSimData>* beamData = reader.GetBeamData();
        if (beamData && !beamData->empty()) {
            std::cout << "Beam数据成功读取，包含 " << beamData->size() << " 个粒子:" << std::endl;
            
            for (size_t i = 0; i < beamData->size(); i++) {
                const TBeamSimData& particle = (*beamData)[i];
                std::cout << "粒子 " << i << ":" << std::endl;
                std::cout << "  粒子名称: " << particle.fParticleName.Data() << std::endl;
                std::cout << "  PDG代码: " << particle.fPDGCode << std::endl;
                std::cout << "  电荷: " << particle.fCharge << std::endl;
                std::cout << "  质量: " << particle.fMass << " MeV" << std::endl;
                std::cout << "  动量: (" << particle.fMomentum.Px() << ", " 
                         << particle.fMomentum.Py() << ", " 
                         << particle.fMomentum.Pz() << ") MeV/c" << std::endl;
                std::cout << "  位置: (" << particle.fPosition.X() << ", " 
                         << particle.fPosition.Y() << ", " 
                         << particle.fPosition.Z() << ") mm" << std::endl;
                std::cout << std::endl;
            }
        } else {
            std::cout << "Beam数据为空" << std::endl;
        }
    }
    
    std::cout << "测试完成" << std::endl;
}