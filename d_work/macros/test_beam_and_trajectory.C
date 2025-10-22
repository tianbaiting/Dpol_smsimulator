#include "TSystem.h"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include "EventDataReader.hh"

void test_beam_and_trajectory() {
    // 加载库
    gSystem->Load("sources/build/libPDCAnalysisTools.so");
    
    // 1. 检查实际的beam数据
    cout << "=== 检查实际beam数据 ===" << endl;
    EventDataReader reader("output_tree/testry0000.root");
    if (reader.IsOpen() && reader.GoToEvent(0)) {
        const std::vector<TBeamSimData>* beam = reader.GetBeamData();
        if (beam && !beam->empty()) {
            cout << "Found " << beam->size() << " beam particles:" << endl;
            for (size_t i = 0; i < beam->size(); i++) {
                const TBeamSimData& particle = (*beam)[i];
                cout << "Particle " << i << ":" << endl;
                cout << "  Name: " << particle.fParticleName.Data() << endl;
                cout << "  PDG: " << particle.fPDGCode << endl;
                cout << "  Charge: " << particle.fCharge << " e" << endl;
                cout << "  Mass: " << particle.fMass << " MeV/c²" << endl;
                cout << "  Position: (" << particle.fPosition.X() << ", " 
                     << particle.fPosition.Y() << ", " << particle.fPosition.Z() << ") mm" << endl;
                cout << "  Momentum: (" << particle.fMomentum.Px() << ", " 
                     << particle.fMomentum.Py() << ", " << particle.fMomentum.Pz() << ") MeV/c" << endl;
                cout << "  |p|: " << particle.fMomentum.P() << " MeV/c" << endl;
                cout << "  Energy: " << particle.fMomentum.E() << " MeV" << endl;
                cout << endl;
            }
        }
    }
    
    // 2. 检查磁场在粒子路径上的强度
    cout << "=== 检查磁场强度 ===" << endl;
    MagneticField magField;
    if (magField.LoadFromROOTFile("magnetic_field_test.root")) {
        magField.SetRotationAngle(30.0);
        
        // 测试一些关键位置的磁场
        vector<TVector3> testPositions = {
            TVector3(0, 0, 0),        // 起始点
            TVector3(100, 0, 500),    // 轨迹中点
            TVector3(200, 0, 1000),   // 轨迹后段
            TVector3(0, 0, 1500),     // 磁铁中心区域
            TVector3(500, 0, 1500),   // 磁铁边缘
        };
        
        for (const auto& pos : testPositions) {
            TVector3 B = magField.GetField(pos);
            cout << "Position (" << pos.X() << ", " << pos.Y() << ", " << pos.Z() 
                 << ") mm -> B = (" << B.X() << ", " << B.Y() << ", " << B.Z() 
                 << ") T, |B| = " << B.Mag() << " T" << endl;
        }
    }
    
    // 3. 测试轨迹计算设置
    cout << "\n=== 测试轨迹计算 ===" << endl;
    ParticleTrajectory trajectory(&magField);
    trajectory.SetStepSize(1.0);       // 更小的步长
    trajectory.SetMaxTime(100.0);      // 更长时间
    trajectory.SetMaxDistance(5000.0); // 更远距离
    trajectory.SetMinMomentum(1.0);    // 更低动量阈值
    
    // 使用实际的质子数据
    if (reader.IsOpen() && reader.GoToEvent(0)) {
        const std::vector<TBeamSimData>* beam = reader.GetBeamData();
        if (beam && !beam->empty()) {
            const TBeamSimData& proton = (*beam)[0]; // 假设第一个是质子
            
            cout << "计算质子轨迹:" << endl;
            cout << "  起始位置: (" << proton.fPosition.X() << ", " 
                 << proton.fPosition.Y() << ", " << proton.fPosition.Z() << ") mm" << endl;
            cout << "  起始动量: (" << proton.fMomentum.Px() << ", " 
                 << proton.fMomentum.Py() << ", " << proton.fMomentum.Pz() << ") MeV/c" << endl;
            cout << "  电荷: " << proton.fCharge << " e" << endl;
            cout << "  质量: " << proton.fMass << " MeV/c²" << endl;
            
            auto traj = trajectory.CalculateTrajectory(
                proton.fPosition, 
                proton.fMomentum, 
                proton.fCharge, 
                proton.fMass
            );
            
            if (traj.size() > 1) {
                cout << "轨迹计算成功，" << traj.size() << " 个点" << endl;
                cout << "起始点: (" << traj[0].position.X() << ", " 
                     << traj[0].position.Y() << ", " << traj[0].position.Z() << ") mm" << endl;
                cout << "终点: (" << traj.back().position.X() << ", " 
                     << traj.back().position.Y() << ", " << traj.back().position.Z() << ") mm" << endl;
                     
                // 计算偏转
                TVector3 displacement = traj.back().position - traj[0].position;
                cout << "总位移: (" << displacement.X() << ", " 
                     << displacement.Y() << ", " << displacement.Z() << ") mm" << endl;
                cout << "横向偏转 (X): " << displacement.X() << " mm" << endl;
                cout << "纵向偏转 (Y): " << displacement.Y() << " mm" << endl;
            } else {
                cout << "轨迹计算失败!" << endl;
            }
        }
    }
}