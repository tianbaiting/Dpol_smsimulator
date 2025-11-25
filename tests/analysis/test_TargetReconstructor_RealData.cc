#include <gtest/gtest.h>
#include "TargetReconstructor.hh"
#include "MagneticField.hh"
#include "ReconstructionVisualizer.hh"
#include "EventDataReader.hh"
#include "EventDisplay.hh"
#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "RecoEvent.hh"
#include "TBeamSimData.hh"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TMath.h"
#include "TEveManager.h"
#include "TGLViewer.h"
#include "TGLCamera.h"
#include "TImage.h"
#include "TSystem.h"
#include "TROOT.h"
#include <cstdlib>
#include <iomanip>

/**
 * @brief TargetReconstructor 真实数据测试
 * 
 * 使用模拟输出的真实ROOT文件测试重建算法
 * 可视化TMinuit优化过程在loss function - P 图上的轨迹
 */
class TargetReconstructorRealDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 获取环境变量
        const char* smsDirEnv = getenv("SMSIMDIR");
        if (!smsDirEnv) {
            GTEST_SKIP() << "SMSIMDIR environment variable not set";
        }
        smsDir = std::string(smsDirEnv);
        
        // 设置文件路径
        // dataFilePath = "/home/tian/workspace/dpol/smsimulator5.5/data/simulation/output_tree/ypol_5000events/Pb208_g050/ypol_np_Pb208_g0500000.root";
        dataFilePath = "/home/tian/workspace/dpol/smsimulator5.5/data/simulation/output_tree/testry0000.root";
        geometryFile = "/home/tian/workspace/dpol/smsimulator5.5/detector_geometry.gdml";
        magFieldFile = smsDir + "/configs/simulation/geometry/filed_map/180626-1,20T-3000.root";
        
        std::cout << "\n=== Real Data Test Setup ===" << std::endl;
        std::cout << "Data file: " << dataFilePath << std::endl;
        std::cout << "Geometry: " << geometryFile << std::endl;
        std::cout << "Mag field: " << magFieldFile << std::endl;
        
        // 加载磁场
        magField = new MagneticField();
        bool loaded = magField->LoadFromROOTFile(magFieldFile, "MagField");
        
        if (!loaded) {
            std::cerr << "ERROR: Could not load magnetic field from " << magFieldFile << std::endl;
            GTEST_SKIP() << "Magnetic field file not found";
        }
        
        // 设置磁场旋转角度（根据run_display_safe.C）
        magField->SetRotationAngle(30.0);
        std::cout << "Loaded magnetic field with 30° rotation" << std::endl;
        
        // 加载几何
        geo = new GeometryManager();
        std::string geoMacFile = smsDir + "/d_work/geometry/5deg_1.2T.mac";
        geo->LoadGeometry(geoMacFile.c_str());
        
        targetPos = geo->GetTargetPosition();
        std::cout << "Target position: (" << targetPos.X() << ", " << targetPos.Y() 
                  << ", " << targetPos.Z() << ") mm" << std::endl;
        std::cout << "Target angle: " << geo->GetTargetAngleDeg() << " deg" << std::endl;
        
        // 创建PDC分析器
        pdcAna = new PDCSimAna(*geo);
        pdcAna->SetSmearing(0.5, 0.5); // 位置涂抹 0.5mm
        
        // 创建重建器
        reconstructor = new TargetReconstructor(magField);
        
        // 检查可视化模式
        const char* vizEnv = std::getenv("SM_TEST_VISUALIZATION");
        enableVisualization = (vizEnv != nullptr && std::string(vizEnv) == "ON");
        
        if (enableVisualization) {
            std::cout << "\n=== Visualization Mode ENABLED ===" << std::endl;
            visualizer = new ReconstructionVisualizer();
        } else {
            std::cout << "\n=== Performance Mode (Visualization OFF) ===" << std::endl;
            visualizer = nullptr;
        }
        
        std::cout << "============================\n" << std::endl;
    }

    void TearDown() override {
        if (visualizer) delete visualizer;
        delete reconstructor;
        delete pdcAna;
        delete geo;
        delete magField;
    }

    std::string smsDir;
    std::string dataFilePath;
    std::string geometryFile;
    std::string magFieldFile;
    
    MagneticField* magField;
    GeometryManager* geo;
    PDCSimAna* pdcAna;
    TargetReconstructor* reconstructor;
    ReconstructionVisualizer* visualizer;
    TVector3 targetPos;
    bool enableVisualization;
};

/**
 * @brief 测试单个事件的重建并可视化TMinuit优化路径
 */
TEST_F(TargetReconstructorRealDataTest, SingleEventReconstruction) {
    // 打开数据文件
    EventDataReader reader(dataFilePath.c_str());
    if (!reader.IsOpen()) {
        GTEST_SKIP() << "Could not open data file: " << dataFilePath;
    }
    
    std::cout << "\n=== Test: Single Event Reconstruction ===" << std::endl;
    std::cout << "Total events in file: " << reader.GetTotalEvents() << std::endl;
    
    // 选择要分析的事件
    Long64_t eventID = 0; // 可以改为其他事件
    
    if (!reader.GoToEvent(eventID)) {
        GTEST_SKIP() << "Could not access event " << eventID;
    }
    
    std::cout << "\nAnalyzing Event " << eventID << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 读取入射粒子（beam）信息
    double trueMomentum = -1.0;  // 真实动量大小 (MeV/c)
    TVector3 trueMomentumVec;    // 真实动量矢量 (MeV/c)
    
    const std::vector<TBeamSimData>* beamData = reader.GetBeamData();
    if (beamData && !beamData->empty()) {
        const TBeamSimData& beam = (*beamData)[0]; // 第一个入射粒子（通常是质子）
        trueMomentumVec = beam.fMomentum.Vect();
        trueMomentum = trueMomentumVec.Mag();
        
        std::cout << "\n=== 入射粒子信息（MC Truth）===" << std::endl;
        std::cout << "  粒子名称: " << beam.fParticleName << std::endl;
        std::cout << "  PDG代码: " << beam.fPDGCode << std::endl;
        std::cout << "  Z=" << beam.fZ << ", A=" << beam.fA << std::endl;
        std::cout << "  电荷: " << beam.fCharge << std::endl;
        std::cout << "  质量: " << beam.fMass << " MeV/c²" << std::endl;
        std::cout << "  入射位置: (" << beam.fPosition.X() << ", " << beam.fPosition.Y() 
                  << ", " << beam.fPosition.Z() << ") mm" << std::endl;
        std::cout << "  入射动量: (" << trueMomentumVec.Px() << ", " << trueMomentumVec.Py() 
                  << ", " << trueMomentumVec.Pz() << ") MeV/c" << std::endl;
        std::cout << "  入射动量大小 |p|: " << trueMomentum << " MeV/c" << std::endl;
        std::cout << "  总能量: " << beam.fMomentum.E() << " MeV" << std::endl;
        std::cout << "  动能: " << (beam.fMomentum.E() - beam.fMass) << " MeV" << std::endl;
        std::cout << "================================" << std::endl;
    } else {
        std::cout << "\n⚠️  警告：未找到入射粒子信息（beam数据）" << std::endl;
    }
    
    // 获取hits并重建
    TClonesArray* hits = reader.GetHits();
    if (!hits || hits->GetEntries() == 0) {
        GTEST_SKIP() << "No PDC hits in event " << eventID;
    }
    
    std::cout << "\nRaw hits: " << hits->GetEntries() << std::endl;
    
    // PDC重建
    RecoEvent event = pdcAna->ProcessEvent(hits);
    event.eventID = eventID;
    
    std::cout << "Smeared hits: " << event.smearedHits.size() << std::endl;
    std::cout << "Reconstructed tracks: " << event.tracks.size() << std::endl;
    
    if (event.tracks.empty()) {
        GTEST_SKIP() << "No reconstructed tracks in event " << eventID;
    }
    
    // 分析第一条轨迹
    const RecoTrack& track = event.tracks[0];
    
    std::cout << "\nTrack 0:" << std::endl;
    std::cout << "  PDC1 (start): (" << track.start.X() << ", " << track.start.Y() 
              << ", " << track.start.Z() << ") mm" << std::endl;
    std::cout << "  PDC2 (end):   (" << track.end.X() << ", " << track.end.Y() 
              << ", " << track.end.Z() << ") mm" << std::endl;
    
    TVector3 trackDir = (track.end - track.start).Unit();
    std::cout << "  Direction: (" << trackDir.X() << ", " << trackDir.Y() 
              << ", " << trackDir.Z() << ")" << std::endl;
    
    // 如果启用可视化，先计算全局网格
    ReconstructionVisualizer::GlobalGrid grid;
    if (enableVisualization && visualizer) {
        std::cout << "\n=== Calculating Global Loss Function Grid ===" << std::endl;
        std::cout << "This may take a while (scanning 200 momentum points)..." << std::endl;
        
        grid = visualizer->CalculateGlobalGrid(
            reconstructor, track, targetPos, 
            100.0,   // pMin (MeV/c)
            3000.0,  // pMax (MeV/c)
            200      // nPoints - 精细扫描以获得平滑曲线
        );
        
        std::cout << "✓ Global grid calculated with " << grid.momenta.size() << " points" << std::endl;
        
        // 找到全局最小值位置
        auto minIt = std::min_element(grid.distances.begin(), grid.distances.end());
        size_t minIdx = std::distance(grid.distances.begin(), minIt);
        double globalMinP = grid.momenta[minIdx];
        double globalMinDist = grid.distances[minIdx];
        std::cout << "✓ Global minimum found at: p=" << globalMinP << " MeV/c, distance=" 
                  << globalMinDist << " mm" << std::endl;
    }
    
    // 使用TMinuit重建（总是记录优化步骤）
    std::cout << "\n=== Running TMinuit Reconstruction ===" << std::endl;
    
    double pInit = 800.0; // 初始猜测
    std::cout << "Initial momentum guess: " << pInit << " MeV/c" << std::endl;
    std::cout << "Tolerance: 5 mm, Max iterations: 5000" << std::endl;
    
    TargetReconstructionResult result = reconstructor->ReconstructAtTargetMinuit(
        track, targetPos, 
        enableVisualization,  // saveTrajectories (仅在可视化模式保存完整轨迹)
        pInit,                // initial momentum guess
        5.0,                  // tolerance (mm)
        5000,                 // max iterations
        true                  // recordSteps=true (总是记录优化步骤)
    );
    
    std::cout << "\n=== TMinuit Results ===" << std::endl;
    std::cout << "  Success: " << (result.success ? "✓ YES" : "✗ NO") << std::endl;
    std::cout << "  Final distance: " << result.finalDistance << " mm" << std::endl;
    std::cout << "  Reconstructed momentum: (" << result.bestMomentum.Px() << ", " 
              << result.bestMomentum.Py() << ", " << result.bestMomentum.Pz() << ") MeV/c" << std::endl;
    std::cout << "  Reconstructed |p|: " << result.bestMomentum.P() << " MeV/c" << std::endl;
    std::cout << "  Total iterations: " << result.totalIterations << std::endl;
    
    // 如果有真实动量，打印对比
    if (trueMomentum > 0) {
        double recoP = result.bestMomentum.P();
        double deltaP = recoP - trueMomentum;
        double deltaPPercent = (deltaP / trueMomentum) * 100.0;
        
        std::cout << "\n=== 与MC Truth的对比 ===" << std::endl;
        std::cout << "  真实动量 |p_true|: " << trueMomentum << " MeV/c" << std::endl;
        std::cout << "  重建动量 |p_reco|: " << recoP << " MeV/c" << std::endl;
        std::cout << "  绝对误差 Δp: " << deltaP << " MeV/c" << std::endl;
        std::cout << "  相对误差: " << std::fixed << std::setprecision(2) << deltaPPercent << " %" << std::endl;
        
        // 动量矢量对比
        TVector3 recoVec = result.bestMomentum.Vect();
        double dotProduct = recoVec.Dot(trueMomentumVec);
        double cosAngle = dotProduct / (recoVec.Mag() * trueMomentumVec.Mag());
        double angle = TMath::ACos(cosAngle) * TMath::RadToDeg();
        
        std::cout << "  动量方向夹角: " << std::setprecision(3) << angle << " deg" << std::endl;
        std::cout << "========================" << std::endl;
    }
    
    // 打印优化路径
    if (!result.optimizationSteps_P.empty()) {
        std::cout << "\n=== TMinuit Optimization Path (详细记录) ===" << std::endl;
        std::cout << std::setw(8) << "Step" 
                  << std::setw(16) << "Momentum(MeV/c)" 
                  << std::setw(16) << "Loss(mm^2)"
                  << std::setw(14) << "Distance(mm)" << std::endl;
        std::cout << std::string(54, '-') << std::endl;
        
        size_t nSteps = result.optimizationSteps_P.size();
        size_t maxPrint = 20; // 只打印前后各10步
        
        for (size_t i = 0; i < std::min(nSteps, maxPrint/2); ++i) {
            double p = result.optimizationSteps_P[i];
            double loss = result.optimizationSteps_Loss[i];
            std::cout << std::setw(8) << i 
                      << std::setw(16) << std::fixed << std::setprecision(2) << p
                      << std::setw(16) << std::scientific << std::setprecision(4) << loss
                      << std::setw(14) << std::fixed << std::setprecision(2) << std::sqrt(loss)
                      << std::endl;
        }
        
        if (nSteps > maxPrint) {
            std::cout << "  ... (" << (nSteps - maxPrint) << " steps omitted) ..." << std::endl;
            
            for (size_t i = nSteps - maxPrint/2; i < nSteps; ++i) {
                double p = result.optimizationSteps_P[i];
                double loss = result.optimizationSteps_Loss[i];
                std::cout << std::setw(8) << i 
                          << std::setw(16) << std::fixed << std::setprecision(2) << p
                          << std::setw(16) << std::scientific << std::setprecision(4) << loss
                          << std::setw(14) << std::fixed << std::setprecision(2) << std::sqrt(loss)
                          << std::endl;
            }
        }
        
        std::cout << "\n优化路径摘要:" << std::endl;
        std::cout << "  初始动量: " << result.optimizationSteps_P.front() << " MeV/c" << std::endl;
        std::cout << "  最终动量: " << result.optimizationSteps_P.back() << " MeV/c" << std::endl;
        std::cout << "  动量变化: " << (result.optimizationSteps_P.back() - result.optimizationSteps_P.front()) << " MeV/c" << std::endl;
        std::cout << "  初始距离: " << std::sqrt(result.optimizationSteps_Loss.front()) << " mm" << std::endl;
        std::cout << "  最终距离: " << std::sqrt(result.optimizationSteps_Loss.back()) << " mm" << std::endl;
        std::cout << "  距离改善: " << (std::sqrt(result.optimizationSteps_Loss.front()) - std::sqrt(result.optimizationSteps_Loss.back())) << " mm" << std::endl;
        std::cout << "  优化步数: " << nSteps << std::endl;
    } else {
        std::cout << "\n⚠️  Warning: No optimization steps recorded!" << std::endl;
    }
    
    EXPECT_TRUE(result.success);
    EXPECT_LT(result.finalDistance, 10.0); // 允许10mm误差
    
    // 可视化（绘制 Loss Function vs P 图并标注 TMinuit 每一步）
    if (enableVisualization && visualizer && !result.optimizationSteps_P.empty()) {
        std::cout << "\n=== Generating Visualization ===" << std::endl;
        
        // 转换优化步骤为可视化格式
        std::vector<ReconstructionVisualizer::OptimizationStep> vizSteps;
        vizSteps.reserve(result.optimizationSteps_P.size());
        
        for (size_t i = 0; i < result.optimizationSteps_P.size(); ++i) {
            ReconstructionVisualizer::OptimizationStep step;
            step.momentum = result.optimizationSteps_P[i];
            step.distance = std::sqrt(result.optimizationSteps_Loss[i]);  // 转换为距离(mm)
            step.iteration = static_cast<int>(i);
            step.gradient = 0.0;  // TMinuit内部计算，不对外暴露
            vizSteps.push_back(step);
        }
        
        std::cout << "✓ Converted " << vizSteps.size() << " optimization steps for plotting" << std::endl;
        
        // 1. 绘制全局 Loss Function vs P 图（带 TMinuit 优化路径标注）
        TVector3 recoMomVec = result.bestMomentum.Vect();
        
        std::cout << "✓ Plotting Loss Function vs Momentum with TMinuit path..." << std::endl;
        TCanvas* cPath = visualizer->PlotOptimizationPath(
            grid,          // 全局扫描网格
            vizSteps,      // TMinuit 优化步骤（每一步都会被标注）
            Form("Event %lld - Loss Function & TMinuit Optimization Path", eventID),
            trueMomentum,  // 真实动量（如果有，否则为-1）
            (trueMomentum > 0 ? &trueMomentumVec : nullptr),  // 真实动量矢量
            &recoMomVec    // reconstructed momentum (最终重建结果)
        );
        
        if (cPath) {
            std::cout << "  ✓ Loss function plot created successfully" << std::endl;
            std::cout << "    - Blue curve: Global loss function D(p)" << std::endl;
            std::cout << "    - Red line with markers: TMinuit optimization path" << std::endl;
            if (trueMomentum > 0) {
                std::cout << "    - Magenta star: True incident momentum (MC Truth)" << std::endl;
            }
            std::cout << "    - Green star: Final reconstructed momentum" << std::endl;
        } else {
            std::cout << "  ✗ Failed to create loss function plot" << std::endl;
        }
        
        // 2. 绘制3D轨迹（如果有保存）
        if (!result.bestTrajectory.empty()) {
            std::cout << "✓ Plotting 3D reconstructed trajectory..." << std::endl;
            TCanvas* cTraj = visualizer->PlotTrajectory3D(
                result.bestTrajectory, targetPos,
                Form("Event %lld - Reconstructed Trajectory (3D)", eventID)
            );
            
            if (cTraj) {
                std::cout << "  ✓ 3D trajectory plot created with " 
                          << result.bestTrajectory.size() << " points" << std::endl;
            }
        } else {
            std::cout << "⚠️  No trajectory data saved (visualization mode may be disabled)" << std::endl;
        }
        
        // 3. 保存所有图像到文件
        std::string outputDir = Form("test_output/reconstruction_realdata_event%lld", eventID);
        std::cout << "\n✓ Saving all plots to: " << outputDir << "/" << std::endl;
        visualizer->SavePlots(outputDir);
        std::cout << "  ✓ Plots saved successfully" << std::endl;
        std::cout << "  Files:" << std::endl;
        std::cout << "    - optimization_path_*.png  (Loss function with TMinuit steps)" << std::endl;
        std::cout << "    - trajectory_3d_*.png       (3D particle trajectory)" << std::endl;
        
        // 4. 打印统计信息
        std::cout << "\n=== Visualization Statistics ===" << std::endl;
        visualizer->PrintStatistics();
        
        // 5. 使用 EventDisplay 绘制 3D 场景（探测器 + 轨迹）
        // 注意：EventDisplay 需要图形环境，在批处理模式下会跳过
        if (!gROOT->IsBatch()) {
            std::cout << "\n=== Creating 3D Event Display ===" << std::endl;
            std::cout << "Initializing EventDisplay with detector geometry..." << std::endl;
            
            try {
                // 创建 EventDisplay 对象
                EventDisplay* display = new EventDisplay(geometryFile.c_str(), *geo);
                
                // 显示事件和入射粒子轨迹
                std::cout << "Drawing incident particle trajectories..." << std::endl;
                display->DisplayEventWithTrajectories(reader, event, magField);
                
                // 添加靶点标记
                TEvePointSet* targetMarker = new TEvePointSet("Target");
                targetMarker->SetMarkerColor(kYellow);
                targetMarker->SetMarkerStyle(29); // Star
                targetMarker->SetMarkerSize(3.0);
                targetMarker->SetNextPoint(targetPos.X(), targetPos.Y(), targetPos.Z());
                TEveElementList* eventElements = display->GetCurrentEventElements();
                if (eventElements) {
                    eventElements->AddElement(targetMarker);
                }
                
                // 绘制重建结果（包含重建轨迹）
                if (!result.bestTrajectory.empty()) {
                    std::cout << "Drawing reconstructed trajectory to target..." << std::endl;
                    display->DrawReconstructionResults(result, false);
                }
                
                // 设置相机视角
                std::cout << "Setting camera view..." << std::endl;
                if (gEve && gEve->GetDefaultGLViewer()) {
                    TGLViewer* viewer = gEve->GetDefaultGLViewer();
                    TGLCamera& camera = viewer->CurrentCamera();
                    
                    // 设置相机位置和方向（俯视角度，便于观察轨迹）
                    camera.SetExternalCenter(true);
                    camera.SetCenterVec(-1500.0, 0.0, 1500.0);  // 观察中心
                    camera.RotateRad(-0.3, 0.5);  // 旋转视角
                    camera.Dolly(2000, false, false);  // 拉远距离
                    
                    viewer->UpdateScene();
                    viewer->RequestDraw();
                }
                
                // 重绘场景
                display->Redraw();
                
                // 保存为图像文件
                std::cout << "Saving 3D event display as PNG..." << std::endl;
                std::string displayPngPath = outputDir + "/event_display_3d.png";
                
                if (gEve && gEve->GetDefaultGLViewer()) {
                    TGLViewer* viewer = gEve->GetDefaultGLViewer();
                    
                    // 确保场景已更新
                    viewer->UpdateScene();
                    gSystem->ProcessEvents();
                    
                    // 保存图像 - 使用正确的方法
                    viewer->SavePicture(displayPngPath.c_str());
                    
                    std::cout << "  ✓ 3D event display saved to: " << displayPngPath << std::endl;
                } else {
                    std::cout << "  ✗ Failed to access GL viewer" << std::endl;
                }
                
                // 清理
                delete display;
                
            } catch (const std::exception& e) {
                std::cout << "  ⚠️  EventDisplay skipped (requires graphical environment): " << e.what() << std::endl;
            }
        } else {
            std::cout << "\n💡 EventDisplay skipped (ROOT running in batch mode)" << std::endl;
            std::cout << "   To enable 3D display, run ROOT in interactive/graphical mode" << std::endl;
        }
        
    } else if (!enableVisualization) {
        std::cout << "\n💡 Visualization disabled. Set SM_TEST_VISUALIZATION=ON to enable plots." << std::endl;
    } else if (!visualizer) {
        std::cout << "\n⚠️  Visualizer not available." << std::endl;
    } else if (result.optimizationSteps_P.empty()) {
        std::cout << "\n⚠️  No optimization steps to visualize." << std::endl;
    }
    
    std::cout << "\n========================================" << std::endl;
}

/**
 * @brief 测试多个事件（批量分析）
 */
TEST_F(TargetReconstructorRealDataTest, MultipleEventsAnalysis) {
    // 在性能模式下运行批量分析
    if (enableVisualization) {
        GTEST_SKIP() << "Skipping batch analysis in visualization mode";
    }
    
    EventDataReader reader(dataFilePath.c_str());
    if (!reader.IsOpen()) {
        GTEST_SKIP() << "Could not open data file";
    }
    
    std::cout << "\n=== Multiple Events Analysis ===" << std::endl;
    
    Long64_t nEvents = std::min(static_cast<Long64_t>(10), reader.GetTotalEvents());
    std::cout << "Analyzing first " << nEvents << " events..." << std::endl;
    
    int successCount = 0;
    std::vector<double> distances;
    std::vector<double> momenta;
    
    for (Long64_t i = 0; i < nEvents; ++i) {
        if (!reader.GoToEvent(i)) continue;
        
        TClonesArray* hits = reader.GetHits();
        if (!hits || hits->GetEntries() == 0) continue;
        
        RecoEvent event = pdcAna->ProcessEvent(hits);
        if (event.tracks.empty()) continue;
        
        const RecoTrack& track = event.tracks[0];
        
        // 快速重建（无可视化）
        TargetReconstructionResult result = reconstructor->ReconstructAtTargetMinuit(
            track, targetPos, false, 800.0, 1.0, 1000, false
        );
        
        if (result.success) {
            successCount++;
            distances.push_back(result.finalDistance);
            momenta.push_back(result.bestMomentum.P());
            
            std::cout << "Event " << i << ": |p|=" << result.bestMomentum.P() 
                      << " MeV/c, dist=" << result.finalDistance << " mm" << std::endl;
        }
    }
    
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Successful reconstructions: " << successCount << "/" << nEvents << std::endl;
    
    if (!distances.empty()) {
        double avgDist = std::accumulate(distances.begin(), distances.end(), 0.0) / distances.size();
        double avgMom = std::accumulate(momenta.begin(), momenta.end(), 0.0) / momenta.size();
        
        std::cout << "Average distance: " << avgDist << " mm" << std::endl;
        std::cout << "Average momentum: " << avgMom << " MeV/c" << std::endl;
    }
    
    EXPECT_GT(successCount, 0);
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "\n==========================================" << std::endl;
    std::cout << "TargetReconstructor Real Data Test Suite" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    const char* vizEnv = std::getenv("SM_TEST_VISUALIZATION");
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
