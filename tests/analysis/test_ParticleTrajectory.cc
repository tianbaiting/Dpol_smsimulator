#include <gtest/gtest.h>
#include "ParticleTrajectory.hh"
#include "MagneticField.hh"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TMath.h"

/**
 * @brief ParticleTrajectory 单元测试
 * 
 * 测试轨迹计算的准确性、边界条件和物理合理性
 */
class ParticleTrajectoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 加载实际的 1.2T 磁场文件
        std::string magFieldFile = std::string(getenv("SMSIMDIR")) + "/configs/simulation/geometry/filed_map/180626-1,20T-3000.root";
        
        magField = new MagneticField();
        bool loaded = magField->LoadFromROOTFile(magFieldFile, "MagField");
        
        if (!loaded) {
            std::cerr << "WARNING: Could not load magnetic field from " << magFieldFile << std::endl;
            std::cerr << "Some tests may fail or be skipped" << std::endl;
        } else {
            std::cout << "Loaded 1.2T magnetic field from: " << magFieldFile << std::endl;
            magField->PrintInfo();
        }
        
        trajectory = new ParticleTrajectory(magField);
    }

    void TearDown() override {
        delete trajectory;
        delete magField;
    }

    MagneticField* magField;
    ParticleTrajectory* trajectory;
};

/**
 * @brief 测试中性粒子轨迹 (应该是直线)
 */
TEST_F(ParticleTrajectoryTest, NeutralParticleStraightLine) {
    TVector3 initialPos(0, 0, 0);
    TVector3 initialMom(0, 0, 1000); // 1 GeV/c 沿 z 方向
    double energy = TMath::Sqrt(initialMom.Mag2() + 939.565*939.565); // 中子质量
    TLorentzVector initialP4(initialMom.X(), initialMom.Y(), initialMom.Z(), energy);
    
    double charge = 0.0; // 中性粒子
    double mass = 939.565; // MeV/c²
    
    auto traj = trajectory->CalculateTrajectory(initialPos, initialP4, charge, mass);
    
    // 应该只有两个点 (起点和终点)
    EXPECT_EQ(traj.size(), 2);
    
    if (traj.size() >= 2) {
        // 检查是否是直线
        TVector3 displacement = traj[1].position - traj[0].position;
        double expectedZ = trajectory->GetMaxDistance();
        
        EXPECT_NEAR(displacement.X(), 0.0, 1e-3);
        EXPECT_NEAR(displacement.Y(), 0.0, 1e-3);
        EXPECT_GT(displacement.Z(), 0.0);
    }
}

/**
 * @brief 测试带电粒子在均匀磁场中的圆周运动
 */
TEST_F(ParticleTrajectoryTest, ChargedParticleCircularMotion) {
    // 质子垂直于磁场运动
    TVector3 initialPos(0, 0, 0);
    TVector3 initialMom(1000, 0, 0); // 1 GeV/c 沿 x 方向
    double protonMass = 938.272; // MeV/c²
    double energy = TMath::Sqrt(initialMom.Mag2() + protonMass*protonMass);
    TLorentzVector initialP4(initialMom.X(), initialMom.Y(), initialMom.Z(), energy);
    
    double charge = 1.0; // 质子
    
    auto traj = trajectory->CalculateTrajectory(initialPos, initialP4, charge, protonMass);
    
    // 应该有多个点
    EXPECT_GT(traj.size(), 10);
    
    if (traj.size() > 10) {
        // 在均匀磁场中，垂直于场的运动应该是圆周运动
        // 检查 y 坐标基本不变 (磁场沿 y 方向)
        for (const auto& point : traj) {
            EXPECT_NEAR(point.position.Y(), 0.0, 10.0); // 允许小偏差
        }
        
        // 检查动量大小基本守恒 (磁场只改变方向)
        double initialP = initialMom.Mag();
        for (const auto& point : traj) {
            double p = point.momentum.Mag();
            EXPECT_NEAR(p, initialP, initialP * 0.05); // 5% 容差 (数值积分误差)
        }
    }
}

/**
 * @brief 测试洛伦兹力方向
 */
TEST_F(ParticleTrajectoryTest, LorentzForceDirection) {
    TVector3 position(0, 0, 0);
    TVector3 momentum(100, 0, 0); // 沿 x 方向
    double charge = 1.0;
    
    // 磁场沿 y 方向 (0, 1, 0)
    // F = q(v × B)，v 沿 x，B 沿 y，所以 F 应该沿 z 方向
    
    double protonMass = 938.272;
    double energy = TMath::Sqrt(momentum.Mag2() + protonMass*protonMass);
    
    TVector3 force = trajectory->CalculateForce(position, momentum, charge, energy);
    
    // 力应该主要沿 z 方向
    EXPECT_GT(TMath::Abs(force.Z()), TMath::Abs(force.X()));
    EXPECT_GT(TMath::Abs(force.Z()), TMath::Abs(force.Y()));
}

/**
 * @brief 测试能量守恒 (磁场不做功)
 */
TEST_F(ParticleTrajectoryTest, EnergyConservation) {
    TVector3 initialPos(0, 0, 0);
    TVector3 initialMom(500, 300, 200); // 任意方向
    double protonMass = 938.272;
    double initialEnergy = TMath::Sqrt(initialMom.Mag2() + protonMass*protonMass);
    TLorentzVector initialP4(initialMom.X(), initialMom.Y(), initialMom.Z(), initialEnergy);
    
    double charge = 1.0;
    
    auto traj = trajectory->CalculateTrajectory(initialPos, initialP4, charge, protonMass);
    
    ASSERT_GT(traj.size(), 10);
    
    // 检查每个点的能量 (应该守恒)
    for (const auto& point : traj) {
        double energy = TMath::Sqrt(point.momentum.Mag2() + protonMass*protonMass);
        EXPECT_NEAR(energy, initialEnergy, initialEnergy * 0.02); // 2% 容差
    }
}

/**
 * @brief 测试低动量阈值停止条件
 */
TEST_F(ParticleTrajectoryTest, MinimumMomentumThreshold) {
    trajectory->SetMinMomentum(100.0); // 设置最小动量为 100 MeV/c
    
    TVector3 initialPos(0, 0, 0);
    TVector3 initialMom(0, 0, 50); // 低于阈值
    double protonMass = 938.272;
    double energy = TMath::Sqrt(initialMom.Mag2() + protonMass*protonMass);
    TLorentzVector initialP4(initialMom.X(), initialMom.Y(), initialMom.Z(), energy);
    
    double charge = 1.0;
    
    auto traj = trajectory->CalculateTrajectory(initialPos, initialP4, charge, protonMass);
    
    // 应该很快停止
    EXPECT_LT(traj.size(), 10);
}

/**
 * @brief 测试时间限制停止条件
 */
TEST_F(ParticleTrajectoryTest, MaxTimeLimit) {
    trajectory->SetMaxTime(10.0); // 设置最大时间为 10 ns
    
    TVector3 initialPos(0, 0, 0);
    TVector3 initialMom(0, 0, 100); // 低动量，运动缓慢
    double protonMass = 938.272;
    double energy = TMath::Sqrt(initialMom.Mag2() + protonMass*protonMass);
    TLorentzVector initialP4(initialMom.X(), initialMom.Y(), initialMom.Z(), energy);
    
    double charge = 1.0;
    
    auto traj = trajectory->CalculateTrajectory(initialPos, initialP4, charge, protonMass);
    
    ASSERT_GT(traj.size(), 0);
    
    // 检查最后一个点的时间
    EXPECT_LE(traj.back().time, 10.0 * 1.1); // 允许小超出
}

/**
 * @brief 测试距离限制停止条件
 */
TEST_F(ParticleTrajectoryTest, MaxDistanceLimit) {
    trajectory->SetMaxDistance(1000.0); // 设置最大距离为 1000 mm
    
    TVector3 initialPos(0, 0, 0);
    TVector3 initialMom(0, 0, 1000); // 高速沿 z 方向
    double protonMass = 938.272;
    double energy = TMath::Sqrt(initialMom.Mag2() + protonMass*protonMass);
    TLorentzVector initialP4(initialMom.X(), initialMom.Y(), initialMom.Z(), energy);
    
    double charge = 0.0; // 中性粒子，直线运动
    
    auto traj = trajectory->CalculateTrajectory(initialPos, initialP4, charge, protonMass);
    
    ASSERT_GT(traj.size(), 0);
    
    // 检查最后一个点的距离
    double distance = traj.back().position.Mag();
    EXPECT_LE(distance, 1000.0 * 1.2); // 允许小超出
}

/**
 * @brief 测试轨迹点提取功能
 */
TEST_F(ParticleTrajectoryTest, GetTrajectoryPoints) {
    TVector3 initialPos(0, 0, 0);
    TVector3 initialMom(0, 0, 500);
    double protonMass = 938.272;
    double energy = TMath::Sqrt(initialMom.Mag2() + protonMass*protonMass);
    TLorentzVector initialP4(initialMom.X(), initialMom.Y(), initialMom.Z(), energy);
    
    auto traj = trajectory->CalculateTrajectory(initialPos, initialP4, 1.0, protonMass);
    
    std::vector<double> x, y, z;
    trajectory->GetTrajectoryPoints(traj, x, y, z);
    
    EXPECT_EQ(x.size(), traj.size());
    EXPECT_EQ(y.size(), traj.size());
    EXPECT_EQ(z.size(), traj.size());
    
    // 验证坐标值
    for (size_t i = 0; i < traj.size(); i++) {
        EXPECT_DOUBLE_EQ(x[i], traj[i].position.X());
        EXPECT_DOUBLE_EQ(y[i], traj[i].position.Y());
        EXPECT_DOUBLE_EQ(z[i], traj[i].position.Z());
    }
}

/**
 * @brief 性能测试：计算大量轨迹点
 */
TEST_F(ParticleTrajectoryTest, PerformanceTest) {
    trajectory->SetMaxTime(100.0);
    trajectory->SetStepSize(0.1); // 小步长，更多点
    
    TVector3 initialPos(0, 0, 0);
    TVector3 initialMom(500, 300, 200);
    double protonMass = 938.272;
    double energy = TMath::Sqrt(initialMom.Mag2() + protonMass*protonMass);
    TLorentzVector initialP4(initialMom.X(), initialMom.Y(), initialMom.Z(), energy);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    auto traj = trajectory->CalculateTrajectory(initialPos, initialP4, 1.0, protonMass);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Calculated " << traj.size() << " trajectory points in " 
              << duration.count() << " ms" << std::endl;
    
    // 性能基准：应该在合理时间内完成
    EXPECT_LT(duration.count(), 1000); // 应该在 1 秒内完成
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
