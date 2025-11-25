#include <gtest/gtest.h>
#include "TargetReconstructor.hh"
#include "MagneticField.hh"
#include "ReconstructionVisualizer.hh"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TMath.h"
#include <cstdlib>
#include <iomanip>

/**
 * @brief TargetReconstructor 单元测试
 * 
 * 测试重建算法的准确性和收敛性
 * 在可视化模式下生成优化函数图和轨迹图
 */
class TargetReconstructorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 加载实际的 1.2T 磁场文件
        std::string magFieldFile = std::string(getenv("SMSIMDIR")) + "/configs/simulation/geometry/filed_map/180626-1,20T-3000.root";
        
        magField = new MagneticField();
        bool loaded = magField->LoadFromROOTFile(magFieldFile, "MagField");
        
        if (!loaded) {
            std::cerr << "ERROR: Could not load magnetic field from " << magFieldFile << std::endl;
            std::cerr << "Tests will be skipped" << std::endl;
            GTEST_SKIP() << "Magnetic field file not found";
        } else {
            std::cout << "Loaded 1.2T magnetic field from: " << magFieldFile << std::endl;
            magField->PrintInfo();
        }
        
        reconstructor = new TargetReconstructor(magField);
        
        // 检查是否启用可视化模式
        const char* vizEnv = std::getenv("SM_TEST_VISUALIZATION");
        enableVisualization = (vizEnv != nullptr && std::string(vizEnv) == "ON");
        
        if (enableVisualization) {
            std::cout << "\n=== Visualization Mode ENABLED ===" << std::endl;
            std::cout << "Test output will include plots and detailed data" << std::endl;
            visualizer = new ReconstructionVisualizer();
        } else {
            std::cout << "\n=== Performance Mode (Visualization OFF) ===" << std::endl;
            std::cout << "Set SM_TEST_VISUALIZATION=ON to enable visualization" << std::endl;
            visualizer = nullptr;
        }
    }

    void TearDown() override {
        if (visualizer) {
            delete visualizer;
        }
        delete reconstructor;
        delete magField;
    }

    MagneticField* magField;
    TargetReconstructor* reconstructor;
    ReconstructionVisualizer* visualizer;
    bool enableVisualization;
    
    /**
     * @brief 创建模拟的重建轨迹
     * 
     * 从已知目标点和动量出发，正向传播到探测器位置，
     * 然后用这些探测器点作为重建输入
     */
    RecoTrack CreateSimulatedTrack(const TVector3& targetPos,
                                  const TLorentzVector& trueMomentum,
                                  double charge, double mass) {
        ParticleTrajectory traj(magField);
        
        auto trajectory = traj.CalculateTrajectory(targetPos, trueMomentum, charge, mass);
        
        EXPECT_GT(trajectory.size(), 2) << "Simulated trajectory too short";
        
        RecoTrack track;
        // 取轨迹后半段的两个较近的点作为 PDC 测量点
        
        size_t idx1 = trajectory.size() *0.85; 
        size_t idx2 = trajectory.size() * 0.88;  
        
        track.start = trajectory[idx1].position;
        track.end = trajectory[idx2].position;
        
        return track;
    }
};

/**
 * @brief 测试基本重建功能
 */
TEST_F(TargetReconstructorTest, BasicReconstruction) {
    TVector3 targetPos(0, 0, -500); // 靶点在 z=-500 mm
    TVector3 trueMom(200, 0, 1000); // 真实动量
    double protonMass = 938.272;
    double energy = TMath::Sqrt(trueMom.Mag2() + protonMass*protonMass);
    TLorentzVector trueP4(trueMom.X(), trueMom.Y(), trueMom.Z(), energy);
    
    std::cout << "\n=== Test: Basic Reconstruction ===" << std::endl;
    std::cout << "True momentum: (" << trueMom.X() << ", " << trueMom.Y() 
              << ", " << trueMom.Z() << ") MeV/c" << std::endl;
    
    // 创建模拟轨迹
    RecoTrack track = CreateSimulatedTrack(targetPos, trueP4, 1.0, protonMass);
    
    // 重建
    TLorentzVector recoP4 = reconstructor->ReconstructAtTarget(track, targetPos);
    
    // 验证结果
    double pDiff = (recoP4.Vect() - trueMom).Mag();
    double pTrue = trueMom.Mag();
    double relativeError = pDiff / pTrue;
    
    std::cout << "Reconstructed momentum: |p|=" << recoP4.P() << " MeV/c" << std::endl;
    std::cout << "True momentum: |p|=" << pTrue << " MeV/c" << std::endl;
    std::cout << "Relative error: " << (relativeError * 100.0) << "%" << std::endl;
    
    EXPECT_LT(relativeError, 0.10); // 相对误差应小于 10%
    
    std::cout << "===================================\n" << std::endl;
}

/**
 * @brief 测试带详细信息的重建 (包括可视化)
 */
TEST_F(TargetReconstructorTest, ReconstructWithVisualization) {
    TVector3 targetPos(0, 0, -500); // 靶点在 z=-500 mm
    TVector3 trueMom(200, 0, 1000); // 真实动量
    double protonMass = 938.272;
    double energy = TMath::Sqrt(trueMom.Mag2() + protonMass*protonMass);
    TLorentzVector trueP4(trueMom.X(), trueMom.Y(), trueMom.Z(), energy);
    
    std::cout << "\n=== Test: Reconstruction with Visualization ===" << std::endl;
    std::cout << "True momentum: (" << trueMom.X() << ", " << trueMom.Y() 
              << ", " << trueMom.Z() << ") MeV/c" << std::endl;
    std::cout << "Target position: (" << targetPos.X() << ", " << targetPos.Y() 
              << ", " << targetPos.Z() << ") mm" << std::endl;
    
    // 创建模拟轨迹
    RecoTrack track = CreateSimulatedTrack(targetPos, trueP4, 1.0, protonMass);
    
    std::cout << "PDC1: (" << track.start.X() << ", " << track.start.Y() 
              << ", " << track.start.Z() << ") mm" << std::endl;
    std::cout << "PDC2: (" << track.end.X() << ", " << track.end.Y() 
              << ", " << track.end.Z() << ") mm" << std::endl;
    
    // 带可视化的重建
    bool saveTrajectories = enableVisualization;
    TargetReconstructionResult result = reconstructor->ReconstructAtTargetWithDetails(
        track, targetPos, saveTrajectories, 50.0, 3000.0, 1.0, 5
    );
    
    std::cout << "Reconstruction success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "Final distance: " << result.finalDistance << " mm" << std::endl;
    std::cout << "Reconstructed momentum: (" << result.bestMomentum.Px() << ", " 
              << result.bestMomentum.Py() << ", " << result.bestMomentum.Pz() 
              << ") MeV/c" << std::endl;
    
    // 验证精度
    double pDiff = (result.bestMomentum.Vect() - trueMom).Mag();
    double pTrue = trueMom.Mag();
    double relativeError = pDiff / pTrue;
    
    std::cout << "Relative error: " << (relativeError * 100.0) << "%" << std::endl;
    
    EXPECT_LT(result.finalDistance, 5.0); // 距离应小于 5 mm
    EXPECT_LT(relativeError, 0.05); // 相对误差应小于 5%
    
    // 如果启用可视化，绘制图像
    if (enableVisualization && visualizer) {
        std::cout << "\nGenerating visualization plots..." << std::endl;
        
        // 1. 计算全局函数值网格
        auto grid = visualizer->CalculateGlobalGrid(
            reconstructor, track, targetPos, 50.0, 3000.0, 200
        );
        
        // 2. 绘制全局函数图
        auto* cFunc = visualizer->PlotOptimizationFunction1D(
            grid, "Target Reconstruction - Global Function"
        );
        
        // 3. 绘制试探轨迹
        if (!result.trialTrajectories.empty()) {
            std::cout << "Plotting " << result.trialTrajectories.size() 
                      << " trial trajectories..." << std::endl;
            
            auto* cMultiTraj = visualizer->PlotMultipleTrajectories(
                result.trialTrajectories, result.trialMomenta, targetPos,
                "Trial Trajectories Comparison"
            );
        }
        
        // 4. 绘制最佳轨迹
        if (!result.bestTrajectory.empty()) {
            auto* cBestTraj = visualizer->PlotTrajectory3D(
                result.bestTrajectory, targetPos, "Best Reconstructed Trajectory"
            );
        }
        
        // 5. 保存所有图像
        visualizer->SavePlots("test_output/reconstruction_basic");
        
        std::cout << "Visualization complete!" << std::endl;
    }
    
    std::cout << "=============================================\n" << std::endl;
}

/**
 * @brief 测试 TMinuit 优化方法 (包括可视化优化过程)
 */
TEST_F(TargetReconstructorTest, TMinuitOptimizationWithPath) {
    TVector3 targetPos(0, 0, -800);
    TVector3 trueMom(150, 100, 1200);
    double protonMass = 938.272;
    double energy = TMath::Sqrt(trueMom.Mag2() + protonMass*protonMass);
    TLorentzVector trueP4(trueMom.X(), trueMom.Y(), trueMom.Z(), energy);
    
    std::cout << "\n=== Test: TMinuit Optimization ===" << std::endl;
    std::cout << "True momentum: (" << trueMom.X() << ", " << trueMom.Y() << ", " << trueMom.Z() << ") MeV/c" << std::endl;
    std::cout << "True momentum magnitude: |p|=" << trueMom.Mag() << " MeV/c" << std::endl;
    
    RecoTrack track = CreateSimulatedTrack(targetPos, trueP4, 1.0, protonMass);
    
    // 如果启用可视化，先计算全局网格
    ReconstructionVisualizer::GlobalGrid grid;
    if (enableVisualization && visualizer) {
        grid = visualizer->CalculateGlobalGrid(
            reconstructor, track, targetPos, 100.0, 2500.0, 150
        );
    }
    
    // Run TMinuit optimization (enable step recording for debugging)
    double pInit = 1000.0; // Initial guess
    TargetReconstructionResult result = reconstructor->ReconstructAtTargetMinuit(
        track, targetPos, enableVisualization, pInit, 0.5, 100,
        true  // recordSteps=true to enable optimization step recording
    );
    
    std::cout << "TMinuit result:" << std::endl;
    std::cout << "  Success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "  Final distance: " << result.finalDistance << " mm" << std::endl;
    std::cout << "  Reconstructed momentum: (" << result.bestMomentum.Px() << ", " 
              << result.bestMomentum.Py() << ", " << result.bestMomentum.Pz() << ") MeV/c" << std::endl;
    std::cout << "  Reconstructed |p|: " << result.bestMomentum.P() << " MeV/c" << std::endl;
    std::cout << "  True momentum: (" << trueMom.X() << ", " << trueMom.Y() << ", " << trueMom.Z() << ") MeV/c" << std::endl;
    std::cout << "  True |p|: " << trueMom.Mag() << " MeV/c" << std::endl;
    
    double pError = TMath::Abs(result.bestMomentum.P() - trueMom.Mag());
    std::cout << "  Absolute error (magnitude): " << pError << " MeV/c" << std::endl;
    
    TVector3 pDiff = result.bestMomentum.Vect() - trueMom;
    std::cout << "  Vector error: (" << pDiff.X() << ", " << pDiff.Y() << ", " << pDiff.Z() << ") MeV/c" << std::endl;
    std::cout << "  Vector error magnitude: " << pDiff.Mag() << " MeV/c" << std::endl;
    
    // Print optimization steps
    if (result.optimizationSteps_P.size() > 0) {
        std::cout << "\n=== TMinuit Optimization Path ===" << std::endl;
        std::cout << "Total iterations: " << result.totalIterations << std::endl;
        std::cout << std::setw(6) << "Step" 
                  << std::setw(15) << "Momentum(MeV/c)" 
                  << std::setw(15) << "Loss(mm^2)"
                  << std::setw(12) << "Distance(mm)" << std::endl;
        std::cout << std::string(48, '-') << std::endl;
        
        for (size_t i = 0; i < result.optimizationSteps_P.size(); ++i) {
            double p = result.optimizationSteps_P[i];
            double loss = result.optimizationSteps_Loss[i];
            std::cout << std::setw(6) << i 
                      << std::setw(15) << std::fixed << std::setprecision(2) << p
                      << std::setw(15) << std::scientific << std::setprecision(3) << loss
                      << std::setw(12) << std::fixed << std::setprecision(2) << std::sqrt(loss)
                      << std::endl;
        }
        std::cout << "Initial momentum: " << result.optimizationSteps_P.front() << " MeV/c" << std::endl;
        std::cout << "Final momentum: " << result.optimizationSteps_P.back() << " MeV/c" << std::endl;
        std::cout << "Momentum change: " << (result.optimizationSteps_P.back() - result.optimizationSteps_P.front()) << " MeV/c" << std::endl;
    } else {
        std::cout << "\nWarning: No optimization steps recorded (recordSteps=false?)" << std::endl;
    }
    
    EXPECT_TRUE(result.success);
    EXPECT_LT(result.finalDistance, 2.0);
    EXPECT_LT(pError, 50.0); // Allow 50 MeV/c error
    
    // Visualize optimization process (if visualization is enabled)
    if (enableVisualization && visualizer && !result.bestTrajectory.empty()) {
        std::cout << "\n=== Generating Visualization ===" << std::endl;
        
        // Convert TMinuit optimization steps to visualization format
        std::vector<ReconstructionVisualizer::OptimizationStep> vizSteps;
        for (size_t i = 0; i < result.optimizationSteps_P.size(); ++i) {
            ReconstructionVisualizer::OptimizationStep step;
            step.momentum = result.optimizationSteps_P[i];
            step.distance = std::sqrt(result.optimizationSteps_Loss[i]);  // loss is distance squared
            step.iteration = i;
            step.gradient = 0.0;  // TMinuit doesn't directly provide gradient
            vizSteps.push_back(step);
        }
        
        // Plot optimization path (with arrows and step labels, marking true momentum)
        // Extract reconstructed momentum vector
        TVector3 recoMomVec = result.bestMomentum.Vect();
        
        auto* cPath = visualizer->PlotOptimizationPath(
            grid, vizSteps, "TMinuit Optimization Path - Full Process",
            trueMom.Mag(),    // Pass true momentum magnitude
            &trueMom,         // Pass true momentum vector
            &recoMomVec       // Pass reconstructed momentum vector
        );
        
        // Calculate true trajectory (for comparison)
        std::vector<TrajectoryPoint> trueTrajectoryPoints;
        ParticleTrajectory trueTraj(magField);
        auto trueFullTraj = trueTraj.CalculateTrajectory(targetPos, trueP4, 1.0, protonMass);
        for (const auto& pt : trueFullTraj) {
            TrajectoryPoint tp;
            tp.position = pt.position;
            tp.momentum = pt.momentum;
            tp.time = pt.time;
            trueTrajectoryPoints.push_back(tp);
        }
        
        // Plot trajectory comparison (true vs reconstructed)
        auto* cTraj = visualizer->PlotTrajectory3D(
            result.bestTrajectory, targetPos, 
            "Trajectory Comparison: True (green) vs Reconstructed (red)",
            &trueTrajectoryPoints  // Pass true trajectory for comparison
        );
        
        // Save all plots
        visualizer->SavePlots("test_output/reconstruction_minuit");
        std::cout << "✓ Plots saved to test_output/reconstruction_minuit/" << std::endl;
    }
    
    std::cout << "===================================\n" << std::endl;
}

/**
 * @brief 测试梯度下降方法 (记录每一步)
 */
TEST_F(TargetReconstructorTest, GradientDescentWithStepRecording) {
    TVector3 targetPos(0, 0, -600);
    TVector3 trueMom(100, 50, 900);
    double protonMass = 938.272;
    double energy = TMath::Sqrt(trueMom.Mag2() + protonMass*protonMass);
    TLorentzVector trueP4(trueMom.X(), trueMom.Y(), trueMom.Z(), energy);
    
    std::cout << "\n=== Test: Gradient Descent with Step Recording ===" << std::endl;
    
    RecoTrack track = CreateSimulatedTrack(targetPos, trueP4, 1.0, protonMass);
    
    // 计算全局网格
    ReconstructionVisualizer::GlobalGrid grid;
    if (enableVisualization && visualizer) {
        grid = visualizer->CalculateGlobalGrid(
            reconstructor, track, targetPos, 100.0, 2000.0, 150
        );
    }
    
    // 运行梯度下降 (手动记录步骤用于可视化)
    double pInit = 1500.0;
    double learningRate = 50.0;
    
    std::cout << "Starting gradient descent with p_init=" << pInit 
              << " MeV/c, learning_rate=" << learningRate << std::endl;
    
    // 如果启用可视化，需要修改 TargetReconstructor 以支持步骤回调
    // 这里我们用现有的方法，然后手动采样优化路径
    
    TargetReconstructionResult result = reconstructor->ReconstructAtTargetGradientDescent(
        track, targetPos, enableVisualization, pInit, learningRate, 0.5, 50
    );
    
    std::cout << "Gradient descent completed:" << std::endl;
    std::cout << "  Final distance: " << result.finalDistance << " mm" << std::endl;
    std::cout << "  Reconstructed |p|: " << result.bestMomentum.P() << " MeV/c" << std::endl;
    
    EXPECT_LT(result.finalDistance, 5.0);
    
    // 可视化：手动创建优化路径 (采样多个动量值)
    if (enableVisualization && visualizer) {
        std::vector<ReconstructionVisualizer::OptimizationStep> steps;
        
        // 采样从初始猜测到最终结果的路径
        int nSteps = 20;
        double pStart = pInit;
        double pEnd = result.bestMomentum.P();
        
        for (int i = 0; i <= nSteps; i++) {
            double alpha = static_cast<double>(i) / nSteps;
            double p = pStart + alpha * (pEnd - pStart);
            
            double dist = reconstructor->CalculateMinimumDistance(
                p, track.start, (track.end - track.start), targetPos, -1.0, protonMass
            );
            
            ReconstructionVisualizer::OptimizationStep step;
            step.momentum = p;
            step.distance = dist;
            step.iteration = i;
            step.gradient = 0.0; // 未记录
            
            steps.push_back(step);
            visualizer->RecordStep(step);
        }
        
        // 绘制优化路径
        auto* cPath = visualizer->PlotOptimizationPath(
            grid, steps, "Gradient Descent - Optimization Path"
        );
        
        // 打印统计信息
        visualizer->PrintStatistics();
        
        visualizer->SavePlots("test_output/reconstruction_gradient");
        visualizer->Clear();
    }
    
    std::cout << "==========================================\n" << std::endl;
}

/**
 * @brief 性能基准测试：批量重建 (无可视化)
 */
TEST_F(TargetReconstructorTest, PerformanceBenchmark) {
    // 此测试应该在性能模式下运行
    if (enableVisualization) {
        GTEST_SKIP() << "Skipping performance test in visualization mode";
    }
    
    std::cout << "\n=== Performance Benchmark ===" << std::endl;
    
    TVector3 targetPos(0, 0, -90);
    double protonMass = 938.272;
    
    int nTrials = 10;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < nTrials; i++) {
        // 随机生成真实动量
        double px = 100.0 + (i * 50.0);
        double py = 50.0;
        double pz = 800.0 + (i * 100.0);
        TVector3 trueMom(px, py, pz);
        
        double energy = TMath::Sqrt(trueMom.Mag2() + protonMass*protonMass);
        TLorentzVector trueP4(trueMom.X(), trueMom.Y(), trueMom.Z(), energy);
        
        RecoTrack track = CreateSimulatedTrack(targetPos, trueP4, 1.0, protonMass);
        
        // 重建 (无可视化数据收集)
        TLorentzVector recoP4 = reconstructor->ReconstructAtTarget(
            track, targetPos, 100.0, 2500.0, 1.0, 3
        );
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double avgTime = static_cast<double>(duration.count()) / nTrials;
    
    std::cout << "Reconstructed " << nTrials << " events in " 
              << duration.count() << " ms" << std::endl;
    std::cout << "Average time per event: " << avgTime << " ms" << std::endl;
    
    // 性能基准：每个事件应该在合理时间内完成
    EXPECT_LT(avgTime, 200.0); // 每个事件少于 200 ms
    
    std::cout << "===========================\n" << std::endl;
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // 打印环境信息
    const char* vizEnv = std::getenv("SM_TEST_VISUALIZATION");
    std::cout << "\n==========================================" << std::endl;
    std::cout << "SMSimulator Analysis Test Suite" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Visualization mode: " 
              << (vizEnv && std::string(vizEnv) == "ON" ? "ENABLED" : "DISABLED") 
              << std::endl;
    
    if (!vizEnv || std::string(vizEnv) != "ON") {
        std::cout << "\nTip: Set SM_TEST_VISUALIZATION=ON to enable" << std::endl;
        std::cout << "     detailed plots and optimization visualization" << std::endl;
    }
    std::cout << "==========================================" << std::endl;
    
    return RUN_ALL_TESTS();
}
