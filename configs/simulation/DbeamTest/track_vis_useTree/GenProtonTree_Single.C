// =========================================================================
// [EN] Generate single ROOT tree for proton trajectory visualization
// [CN] 生成单个质子轨迹ROOT树（位置和角度由Geant4宏控制）
// =========================================================================
// Proton momentum: (±100, 0, 627) MeV/c and (±150, 0, 627) MeV/c
// Position: (0, 0, 0) - actual position set by Target/Position in macro
// Direction: along Z - actual direction rotated by Target/Angle in macro
// =========================================================================

#include <TFile.h>
#include <TTree.h>
#include <TLorentzVector.h>
#include <TVector3.h>
#include <iostream>
#include <cmath>
#include "TBeamSimData.hh"

void GenProtonTree_Single(const char* outputFile = "protons_4tracks.root")
{
    // [EN] Proton momenta: Px = ±100, ±150 MeV/c, Py = 0, Pz = 627 MeV/c
    // [CN] 质子动量: Px = ±100, ±150 MeV/c, Py = 0, Pz = 627 MeV/c
    const double Pz = 627.0;  // MeV/c
    const double Px_values[] = {100.0, -100.0, 150.0, -150.0};
    const int nProtons = 4;
    
    const double Mp = 938.272;  // MeV/c^2
    
    // [EN] Position at origin - actual position set by /samurai/geometry/Target/Position
    // [CN] 位置在原点 - 实际位置由 /samurai/geometry/Target/Position 设置
    TVector3 position(0, 0, 0);
    
    TFile* f = new TFile(outputFile, "RECREATE");
    TTree* tree = new TTree("tree", "Proton tracks for visualization");
    
    gBeamSimDataArray = new TBeamSimDataArray();
    tree->Branch("TBeamSimData", &gBeamSimDataArray);
    
    // [EN] Fill one event with 4 protons
    // [CN] 一个事件填充4个质子
    gBeamSimDataArray->clear();
    
    for (int i = 0; i < nProtons; i++) {
        double Px = Px_values[i];
        double Py = 0.0;
        
        double P_total = std::sqrt(Px*Px + Py*Py + Pz*Pz);
        double E_total = std::sqrt(P_total*P_total + Mp*Mp);
        
        // [EN] Momentum in beam frame - will be rotated by Target/Angle
        // [CN] 束流坐标系中的动量 - 将被 Target/Angle 旋转
        TLorentzVector momentum(Px, Py, Pz, E_total);
        
        TBeamSimData proton(1, 1, momentum, position);
        proton.fParticleName = "proton";
        proton.fPrimaryParticleID = i;
        proton.fTime = 0.0;
        proton.fIsAccepted = true;
        
        gBeamSimDataArray->push_back(proton);
        
        std::cout << "Proton " << i << ": Px=" << Px << " MeV/c, |P|=" << P_total 
                  << " MeV/c, T=" << (E_total - Mp) << " MeV" << std::endl;
    }
    
    tree->Fill();
    
    f->Write();
    f->Close();
    
    std::cout << "\n[OK] Generated: " << outputFile << std::endl;
    std::cout << "  Position: origin (set by /samurai/geometry/Target/Position)" << std::endl;
    std::cout << "  Direction: along Z (rotated by /samurai/geometry/Target/Angle)" << std::endl;
}
