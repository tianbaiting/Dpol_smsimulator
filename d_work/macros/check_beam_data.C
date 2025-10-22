// 检查beam数据的简单测试
void check_beam_data() {
    gSystem->Load("sources/build/libPDCAnalysisTools.so");
    
    EventDataReader reader("/home/tian/workspace/dpol/smsimulator5.5/d_work/output_tree/testry0000.root");
    if (!reader.IsOpen()) {
        std::cout << "无法打开文件" << std::endl;
        return;
    }
    
    reader.GoToEvent(0);
    
    // 检查beam数据
    const std::vector<TBeamSimData>* beam = reader.GetBeamData();
    if (beam) {
        std::cout << "找到beam数据，粒子数量: " << beam->size() << std::endl;
        for (size_t i = 0; i < beam->size(); i++) {
            const TBeamSimData& particle = (*beam)[i];
            std::cout << "粒子 " << i << ":" << std::endl;
            std::cout << "  名称: " << particle.fParticleName.Data() << std::endl;
            std::cout << "  PDG: " << particle.fPDGCode << std::endl;
            std::cout << "  电荷: " << particle.fCharge << std::endl;
            std::cout << "  质量: " << particle.fMass << " MeV" << std::endl;
            std::cout << "  动量: (" << particle.fMomentum.X() << ", " 
                      << particle.fMomentum.Y() << ", " << particle.fMomentum.Z() << ") MeV" << std::endl;
            std::cout << "  位置: (" << particle.fPosition.X() << ", " 
                      << particle.fPosition.Y() << ", " << particle.fPosition.Z() << ") mm" << std::endl;
        }
    } else {
        std::cout << "没有找到beam数据" << std::endl;
    }
}