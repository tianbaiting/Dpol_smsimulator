// =========================================================================
// [EN] Generate ROOT tree for proton trajectory visualization
// [CN] 生成用于质子轨迹可视化的ROOT树
// =========================================================================
// Proton momentum: (±100, 0, 627) MeV/c and (±150, 0, 627) MeV/c
// 4 protons total per event
// =========================================================================

#include <TFile.h>
#include <TTree.h>
#include <TLorentzVector.h>
#include <TVector3.h>
#include <iostream>
#include <cmath>

// [EN] Include TBeamSimData if available, otherwise define minimal structure
// [CN] 包含TBeamSimData，如果不可用则定义最小结构
#ifdef __CINT__
#include "TBeamSimData.hh"
#endif

void GenProtonTree(
    const char* outputFile,
    double targetX_mm,   // [EN] Target X position in mm / [CN] 靶点X位置(mm)
    double targetY_mm,   // [EN] Target Y position in mm / [CN] 靶点Y位置(mm)
    double targetZ_mm    // [EN] Target Z position in mm / [CN] 靶点Z位置(mm)
)
{
    // [EN] Proton momenta: Px = ±100, ±150 MeV/c, Py = 0, Pz = 627 MeV/c
    // [CN] 质子动量: Px = ±100, ±150 MeV/c, Py = 0, Pz = 627 MeV/c
    const double Pz = 627.0;  // MeV/c
    const double Px_values[] = {100.0, -100.0, 150.0, -150.0};
    const int nProtons = 4;
    
    const double Mp = 938.272;  // [EN] Proton mass in MeV/c^2 / [CN] 质子质量
    
    TVector3 position(targetX_mm, targetY_mm, targetZ_mm);
    
    TFile* f = new TFile(outputFile, "RECREATE");
    TTree* tree = new TTree("tree", "Proton tracks for visualization");
    
    // [EN] Create TBeamSimDataArray for storing multiple particles
    // [CN] 创建TBeamSimDataArray存储多粒子
    gBeamSimDataArray = new TBeamSimDataArray();
    tree->Branch("TBeamSimData", &gBeamSimDataArray);
    
    // [EN] Fill one event with 4 protons
    // [CN] 一个事件填充4个质子
    gBeamSimDataArray->clear();
    
    for (int i = 0; i < nProtons; i++) {
        double Px = Px_values[i];
        double Py = 0.0;
        
        // [EN] Calculate total momentum and energy
        // [CN] 计算总动量和能量
        double P_total = std::sqrt(Px*Px + Py*Py + Pz*Pz);
        double E_total = std::sqrt(P_total*P_total + Mp*Mp);
        
        TLorentzVector momentum(Px, Py, Pz, E_total);
        
        // [EN] Create proton data (Z=1, A=1)
        // [CN] 创建质子数据
        TBeamSimData proton(1, 1, momentum, position);
        proton.fParticleName = "proton";
        proton.fPrimaryParticleID = i;
        proton.fTime = 0.0;
        proton.fIsAccepted = true;
        
        gBeamSimDataArray->push_back(proton);
        
        std::cout << "Proton " << i << ": Px=" << Px << " MeV/c, |P|=" << P_total 
                  << " MeV/c, E=" << E_total << " MeV" << std::endl;
    }
    
    tree->Fill();
    
    f->Write();
    f->Close();
    
    std::cout << "\n[EN] Generated: " << outputFile << std::endl;
    std::cout << "[CN] 已生成: " << outputFile << std::endl;
    std::cout << "  Position: (" << targetX_mm << ", " << targetY_mm << ", " << targetZ_mm << ") mm" << std::endl;
    std::cout << "  4 protons: Px = ±100, ±150 MeV/c" << std::endl;
}

// [EN] Wrapper function with field and angle parameters
// [CN] 带磁场和角度参数的包装函数
void GenProtonTree_ByConfig(const char* field, double angle)
{
    // [EN] Target positions from target_summary files
    // [CN] 靶点位置来自target_summary文件
    double targetX, targetY, targetZ;
    
    // [EN] Hardcoded target positions for each field strength at angle=6.0 deg (example)
    // [CN] 各磁场强度在angle=6.0度时的靶点位置(示例)
    // Use generate_all_trees.sh for all configurations
    
    TString outputDir = gSystem->Getenv("SMSIMDIR");
    outputDir += "/configs/simulation/DbeamTest/track_vis_useTree/rootfiles/";
    
    TString outputFile = outputDir + Form("protons_B%sT_%.1fdeg.root", field, angle);
    
    std::cout << "Please use generate_all_trees.sh to generate trees with correct target positions" << std::endl;
}
