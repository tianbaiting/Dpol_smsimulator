// =========================================================================
// [EN] Generate ROOT tree for proton + neutron at fixed target position
// [CN] 在固定靶点位置生成质子+中子轨迹ROOT树
// =========================================================================
// Target position: (0, 0, -4000) mm = (0, 0, -400) cm
// Target angle: 0 deg (no rotation)
// Proton/Neutron momentum: (±100, 0, 627) and (±150, 0, 627) MeV/c
// Total: 4 protons + 4 neutrons = 8 tracks
// =========================================================================

#include <TFile.h>
#include <TTree.h>
#include <TLorentzVector.h>
#include <TVector3.h>
#include <iostream>
#include <cmath>
#include "TBeamSimData.hh"

void GenPNTree_0deg(const char* outputFile = "pn_8tracks_0deg.root")
{
    // [EN] Momenta: Px = ±100, ±150 MeV/c, Py = 0, Pz = 627 MeV/c
    // [CN] 动量: Px = ±100, ±150 MeV/c, Py = 0, Pz = 627 MeV/c
    const double Pz = 627.0;  // MeV/c
    const double Px_values[] = {100.0, -100.0, 150.0, -150.0};
    const int nTracks = 4;
    
    const double Mp = 938.272;  // [EN] Proton mass MeV/c^2 / [CN] 质子质量
    const double Mn = 939.565;  // [EN] Neutron mass MeV/c^2 / [CN] 中子质量
    
    // [EN] Position at origin - will be shifted by /samurai/geometry/Target/Position
    // [CN] 位置在原点 - 将被 /samurai/geometry/Target/Position 移动
    TVector3 position(0, 0, 0);
    
    TFile* f = new TFile(outputFile, "RECREATE");
    TTree* tree = new TTree("tree", "Proton and Neutron tracks at 0deg");
    
    gBeamSimDataArray = new TBeamSimDataArray();
    tree->Branch("TBeamSimData", &gBeamSimDataArray);
    
    gBeamSimDataArray->clear();
    
    int particleID = 0;
    
    // [EN] Add 4 protons / [CN] 添加4个质子
    std::cout << "=== Protons (Z=1, A=1) ===" << std::endl;
    for (int i = 0; i < nTracks; i++) {
        double Px = Px_values[i];
        double Py = 0.0;
        
        double P_total = std::sqrt(Px*Px + Py*Py + Pz*Pz);
        double E_total = std::sqrt(P_total*P_total + Mp*Mp);
        
        TLorentzVector momentum(Px, Py, Pz, E_total);
        
        TBeamSimData proton(1, 1, momentum, position);
        proton.fParticleName = "proton";
        proton.fPrimaryParticleID = particleID++;
        proton.fTime = 0.0;
        proton.fIsAccepted = true;
        
        gBeamSimDataArray->push_back(proton);
        
        std::cout << "Proton " << i << ": Px=" << Px << " MeV/c, |P|=" << P_total 
                  << " MeV/c, T=" << (E_total - Mp) << " MeV" << std::endl;
    }
    
    // [EN] Add 4 neutrons with same momenta / [CN] 添加4个相同动量的中子
    std::cout << "\n=== Neutrons (Z=0, A=1) ===" << std::endl;
    for (int i = 0; i < nTracks; i++) {
        double Px = Px_values[i];
        double Py = 0.0;
        
        double P_total = std::sqrt(Px*Px + Py*Py + Pz*Pz);
        double E_total = std::sqrt(P_total*P_total + Mn*Mn);
        
        TLorentzVector momentum(Px, Py, Pz, E_total);
        
        TBeamSimData neutron(0, 1, momentum, position);
        neutron.fParticleName = "neutron";
        neutron.fPrimaryParticleID = particleID++;
        neutron.fTime = 0.0;
        neutron.fIsAccepted = true;
        
        gBeamSimDataArray->push_back(neutron);
        
        std::cout << "Neutron " << i << ": Px=" << Px << " MeV/c, |P|=" << P_total 
                  << " MeV/c, T=" << (E_total - Mn) << " MeV" << std::endl;
    }
    
    tree->Fill();
    
    f->Write();
    f->Close();
    
    std::cout << "\n[OK] Generated: " << outputFile << std::endl;
    std::cout << "  Total tracks: 8 (4 protons + 4 neutrons)" << std::endl;
    std::cout << "  Target: (0, 0, -4m), Angle: 0 deg" << std::endl;
}
