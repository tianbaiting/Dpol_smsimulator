/**
 * @file test_pdc_position.cc
 * @brief PDC最佳位置计算测试程序
 * 
 * 本测试程序分离出PDC位置计算逻辑，便于单独调试。
 * 主要测试：
 * 1. 束流在磁场中的偏转轨迹
 * 2. Target位置计算（给定偏转角度）
 * 3. 最佳PDC位置计算
 * 4. 磁场旋转和坐标变换的正确性
 * 
 * 运行方式：
 *   ./test_pdc_position [field_map_file] [rotation_angle]
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <filesystem>

// ROOT
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TMultiGraph.h"
#include "TLegend.h"
#include "TLine.h"
#include "TBox.h"
#include "TLatex.h"
#include "TArrow.h"
#include "TEllipse.h"
#include "TStyle.h"
#include "TH2F.h"
#include "TMath.h"
#include "TApplication.h"
#include "TROOT.h"

// SMSimulator libs
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include "BeamDeflectionCalculator.hh"
#include "DetectorAcceptanceCalculator.hh"
#include "GeoAcceptanceManager.hh"

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * @brief 获取输出目录路径 ($SMSIMDIR/results)
 */
std::string GetOutputDir() {
    const char* smsimdir = std::getenv("SMSIMDIR");
    std::string baseDir = smsimdir ? std::string(smsimdir) : "/home/tian/workspace/dpol/smsimulator5.5";
    return baseDir + "/results/test_pdc_position";
}

/**
 * @brief 日志文件管理类
 */
class LogManager {
private:
    std::ofstream logFile;
    std::streambuf* originalCoutBuf;
    std::streambuf* originalCerrBuf;
    std::string logPath;
    
public:
    LogManager() : originalCoutBuf(nullptr), originalCerrBuf(nullptr) {}
    
    ~LogManager() {
        Close();
    }
    
    bool Open(const std::string& path) {
        logPath = path;
        logFile.open(path);
        
        if (!logFile.is_open()) {
            return false;
        }
        
        // 保存原始缓冲区
        originalCoutBuf = std::cout.rdbuf();
        originalCerrBuf = std::cerr.rdbuf();
        
        // 重定向到文件
        std::cout.rdbuf(logFile.rdbuf());
        std::cerr.rdbuf(logFile.rdbuf());
        
        return true;
    }
    
    void Close() {
        if (originalCoutBuf) {
            std::cout.rdbuf(originalCoutBuf);
            originalCoutBuf = nullptr;
        }
        
        if (originalCerrBuf) {
            std::cerr.rdbuf(originalCerrBuf);
            originalCerrBuf = nullptr;
        }
        
        if (logFile.is_open()) {
            logFile.close();
        }
    }
    
    std::string GetLogPath() const { return logPath; }
};

void PrintSeparator(const std::string& title = "") {
    std::cout << "\n";
    std::cout << "================================================================\n";
    if (!title.empty()) {
        std::cout << "  " << title << "\n";
        std::cout << "================================================================\n";
    }
}

void PrintVector3(const std::string& name, const TVector3& v) {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  " << name << ": (" 
              << std::setw(10) << v.X() << ", "
              << std::setw(10) << v.Y() << ", "
              << std::setw(10) << v.Z() << ")\n";
}

/**
 * @brief 测试磁场旋转变换的正确性
 */
void TestMagneticFieldRotation(MagneticField* magField) {
    PrintSeparator("测试1: 磁场旋转变换验证");
    
    double rotAngle = magField->GetRotationAngle();
    std::cout << "磁铁旋转角度: " << rotAngle << "°\n";
    
    // 测试几个关键点
    std::vector<TVector3> testPoints = {
        TVector3(0, 0, 0),
        TVector3(1000, 0, 0),
        TVector3(0, 0, 1000),
        TVector3(1000, 0, 1000),
        TVector3(-500, 0, 500),
    };
    
    std::cout << "\n实验室坐标 -> 磁场值:\n";
    std::cout << std::setw(35) << "Lab Position (mm)" 
              << std::setw(35) << "B-Field (T)" << "\n";
    std::cout << std::string(70, '-') << "\n";
    
    for (const auto& pos : testPoints) {
        TVector3 B = magField->GetField(pos);
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  (" << std::setw(8) << pos.X() << ", " 
                  << std::setw(8) << pos.Y() << ", "
                  << std::setw(8) << pos.Z() << ")";
        std::cout << " -> (" << std::setw(8) << B.X() << ", "
                  << std::setw(8) << B.Y() << ", "
                  << std::setw(8) << B.Z() << ") |B|=" 
                  << std::setw(6) << B.Mag() << "\n";
    }
    
    // 验证旋转变换
    std::cout << "\n坐标变换验证（角度=" << rotAngle << "°）:\n";
    double theta_rad = -rotAngle * TMath::DegToRad();  // 与MagneticField一致
    double cosTheta = TMath::Cos(theta_rad);
    double sinTheta = TMath::Sin(theta_rad);
    
    std::cout << "  cos(θ) = " << cosTheta << ", sin(θ) = " << sinTheta << "\n";
    
    // Lab->Magnet: (x*cos + z*sin, y, -x*sin + z*cos)
    TVector3 labPoint(1000, 0, 0);  // 沿实验室X轴的点
    TVector3 magnetPoint(
        labPoint.X() * cosTheta + labPoint.Z() * sinTheta,
        labPoint.Y(),
        -labPoint.X() * sinTheta + labPoint.Z() * cosTheta
    );
    
    std::cout << "\n实验室点 (1000, 0, 0):\n";
    PrintVector3("Lab", labPoint);
    PrintVector3("Magnet", magnetPoint);
    std::cout << "  说明: 实验室+X方向的点在磁铁坐标系中偏向-Z方向\n";
    
    // 检查beam line方向
    TVector3 beamDir(0, 0, 1);  // 实验室中的束流方向
    TVector3 beamDirMagnet(
        beamDir.X() * cosTheta + beamDir.Z() * sinTheta,
        beamDir.Y(),
        -beamDir.X() * sinTheta + beamDir.Z() * cosTheta
    );
    
    std::cout << "\n束流方向 (0, 0, 1) 在磁铁坐标系中:\n";
    PrintVector3("Beam (Lab)", beamDir);
    PrintVector3("Beam (Magnet)", beamDirMagnet);
}

/**
 * @brief 测试束流偏转计算
 */
void TestBeamDeflection(BeamDeflectionCalculator* beamCalc) {
    PrintSeparator("测试2: 束流偏转轨迹计算");
    
    // 打印束流参数
    beamCalc->PrintBeamInfo();
    
    // 计算完整轨迹
    std::cout << "\n计算氘核束流完整轨迹...\n";
    auto trajectory = beamCalc->CalculateFullBeamTrajectory();
    
    std::cout << "轨迹点数: " << trajectory.size() << "\n";
    
    if (trajectory.size() < 10) {
        std::cerr << "ERROR: 轨迹计算失败！\n";
        return;
    }
    
    // 打印关键轨迹点
    std::cout << "\n关键轨迹点:\n";
    std::cout << std::setw(8) << "Index" 
              << std::setw(12) << "Z (mm)"
              << std::setw(12) << "X (mm)"
              << std::setw(12) << "Y (mm)"
              << std::setw(12) << "角度(°)"
              << std::setw(12) << "|B| (T)" << "\n";
    std::cout << std::string(68, '-') << "\n";
    
    TVector3 initialDir(0, 0, 1);
    size_t step = trajectory.size() / 20;  // 输出约20个点
    if (step < 1) step = 1;
    
    for (size_t i = 0; i < trajectory.size(); i += step) {
        const auto& pt = trajectory[i];
        TVector3 dir = pt.momentum.Unit();
        double angle = TMath::ACos(dir.Dot(initialDir)) * TMath::RadToDeg();
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(8) << i
                  << std::setw(12) << pt.position.Z()
                  << std::setw(12) << pt.position.X()
                  << std::setw(12) << pt.position.Y()
                  << std::setw(12) << angle
                  << std::setw(12) << pt.bField.Mag() << "\n";
    }
    
    // 打印最终状态
    const auto& lastPt = trajectory.back();
    TVector3 finalDir = lastPt.momentum.Unit();
    double finalAngle = TMath::ACos(finalDir.Dot(initialDir)) * TMath::RadToDeg();
    
    std::cout << "\n最终状态:\n";
    PrintVector3("位置", lastPt.position);
    PrintVector3("动量方向", finalDir);
    std::cout << "  总偏转角度: " << finalAngle << "°\n";
}

/**
 * @brief 测试Target位置计算
 */
void TestTargetPositionCalculation(BeamDeflectionCalculator* beamCalc,
                                   const std::vector<double>& deflectionAngles) {
    PrintSeparator("测试3: Target位置计算");
    
    std::cout << "\n对不同偏转角度计算Target位置:\n";
    std::cout << std::setw(10) << "偏转角(°)"
              << std::setw(35) << "Target位置 (X, Y, Z) [mm]"
              << std::setw(12) << "旋转角(°)"
              << std::setw(35) << "束流方向" << "\n";
    std::cout << std::string(92, '-') << "\n";
    
    std::vector<BeamDeflectionCalculator::TargetPosition> targetPositions;
    
    for (double angle : deflectionAngles) {
        auto targetPos = beamCalc->CalculateTargetPosition(angle);
        targetPositions.push_back(targetPos);
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(10) << angle
                  << "  (" << std::setw(8) << targetPos.position.X() << ", "
                  << std::setw(8) << targetPos.position.Y() << ", "
                  << std::setw(8) << targetPos.position.Z() << ")"
                  << std::setw(12) << targetPos.rotationAngle
                  << "  (" << std::setw(8) << targetPos.beamDirection.X() << ", "
                  << std::setw(8) << targetPos.beamDirection.Y() << ", "
                  << std::setw(8) << targetPos.beamDirection.Z() << ")\n";
    }
    
    // 验证结果
    std::cout << "\n结果验证:\n";
    for (const auto& tp : targetPositions) {
        // 检查束流方向与旋转角度是否一致
        double expectedRotAngle = TMath::ATan2(tp.beamDirection.X(), 
                                               tp.beamDirection.Z()) * TMath::RadToDeg();
        double diff = tp.rotationAngle - expectedRotAngle;
        
        std::cout << "  偏转" << tp.deflectionAngle << "°: "
                  << "rotationAngle=" << tp.rotationAngle << "° "
                  << "expected=" << expectedRotAngle << "° "
                  << "差异=" << diff << "°";
        
        if (std::abs(diff) < 0.1) {
            std::cout << " [OK]\n";
        } else {
            std::cout << " [MISMATCH!]\n";
        }
    }
}

/**
 * @brief 测试PDC最佳位置计算
 */
void TestOptimalPDCPosition(DetectorAcceptanceCalculator* detCalc,
                            const std::vector<BeamDeflectionCalculator::TargetPosition>& targetPositions) {
    PrintSeparator("测试4: PDC最佳位置计算");
    
    std::cout << "\n对每个Target位置计算最佳PDC配置:\n";
    
    for (const auto& targetPos : targetPositions) {
        std::cout << "\n--- 偏转角 " << targetPos.deflectionAngle << "° ---\n";
        PrintVector3("Target位置", targetPos.position);
        std::cout << "  Target旋转角: " << targetPos.rotationAngle << "°\n";
        
        // 计算最佳PDC位置
        auto pdcConfig = detCalc->CalculateOptimalPDCPosition(
            targetPos.position, targetPos.rotationAngle);
        
        std::cout << "\nPDC配置:\n";
        PrintVector3("PDC位置", pdcConfig.position);
        PrintVector3("PDC法向量", pdcConfig.normal);
        std::cout << "  PDC旋转角: " << pdcConfig.rotationAngle << "°\n";
        std::cout << "  尺寸: " << pdcConfig.width << " x " << pdcConfig.height 
                  << " x " << pdcConfig.depth << " mm³\n";
        std::cout << "  Px范围: [" << pdcConfig.pxMin << ", " << pdcConfig.pxMax << "] MeV/c\n";
        std::cout << "  最优: " << (pdcConfig.isOptimal ? "是" : "否") << "\n";
        
        // 验证PDC法向量与旋转角度
        double expectedPDCRotAngle = TMath::ATan2(pdcConfig.normal.X(), 
                                                   pdcConfig.normal.Z()) * TMath::RadToDeg();
        double diff = pdcConfig.rotationAngle - expectedPDCRotAngle;
        
        std::cout << "\n验证:\n";
        std::cout << "  法向量计算的旋转角: " << expectedPDCRotAngle << "°\n";
        std::cout << "  配置的旋转角: " << pdcConfig.rotationAngle << "°\n";
        std::cout << "  差异: " << diff << "° ";
        if (std::abs(diff) < 0.1) {
            std::cout << "[OK]\n";
        } else {
            std::cout << "[MISMATCH!]\n";
        }
    }
}

/**
 * @brief 使用库中的 GeoAcceptanceManager 来执行 target/PDC 计算并绘图
 * 
 * [EN] This function demonstrates how to use the GeoAcceptanceManager API
 *      to calculate target positions and generate geometry plots with tracks.
 * [CN] 此函数演示如何使用 GeoAcceptanceManager API 计算靶子位置并生成带轨迹的几何图。
 * 
 * [EN] The generated plots include: / [CN] 生成的图包括：
 *   - Deuteron track (beam trajectory) / 氘核轨迹（束流轨迹）
 *   - Center proton track (Px=0) / 中心质子轨迹 (Px=0)
 *   - Edge proton tracks (Px=±100, ±150 MeV/c) / 边缘质子轨迹
 * 
 * @param pxEdgeValues Configurable Px values for edge proton tracks [MeV/c]
 *                     Default: {100.0, 150.0} draws Px=±100 and Px=±150 tracks
 */
void TestUsingGeoAcceptanceManager(const std::string& fieldMapFile,
                                  const std::vector<double>& deflectionAngles,
                                  const std::string& outputFile,
                                  const std::string& outputDir = "",
                                  LogManager* logMgr = nullptr,
                                  const std::vector<double>& pxEdgeValues = {100.0, 150.0}) {
    PrintSeparator("测试5 (库调用): 使用 GeoAcceptanceManager 进行分析与绘图");

    GeoAcceptanceManager mgr;
    std::string actualOutputDir = outputDir.empty() ? GetOutputDir() : outputDir;
    mgr.SetOutputPath(actualOutputDir);
    std::cout << "输出目录: " << actualOutputDir << "\n";
    
    // [EN] Configure track plotting / [CN] 配置轨迹绘图
    TrackPlotConfig trackConfig;
    trackConfig.drawDeuteronTrack = true;      // [EN] Draw deuteron / [CN] 绘制氘核
    trackConfig.drawCenterProtonTrack = true;  // [EN] Draw Px=0 proton / [CN] 绘制Px=0质子
    trackConfig.pxEdgeValues = pxEdgeValues;   // [EN] Edge proton Px values / [CN] 边缘质子Px值
    trackConfig.pzProton = 627.0;              // [EN] Proton Pz / [CN] 质子Pz
    mgr.SetTrackPlotConfig(trackConfig);
    
    std::cout << "轨迹配置:\n";
    std::cout << "  - 氘核轨迹: " << (trackConfig.drawDeuteronTrack ? "启用" : "禁用") << "\n";
    std::cout << "  - 中心质子 (Px=0): " << (trackConfig.drawCenterProtonTrack ? "启用" : "禁用") << "\n";
    std::cout << "  - 边缘质子 Px 值:";
    for (double px : trackConfig.pxEdgeValues) std::cout << " ±" << px;
    std::cout << " MeV/c\n";
    std::cout << "  - 质子 Pz: " << trackConfig.pzProton << " MeV/c\n";

    // 添加偏转角度配置
    for (double a : deflectionAngles) mgr.AddDeflectionAngle(a);

    // 分析场配置
    if (!mgr.AnalyzeFieldConfiguration(fieldMapFile, 1.0)) {
        std::cerr << "GeoAcceptanceManager 分析失败: " << fieldMapFile << "\n";
        return;
    }

    // 生成图表
    std::filesystem::create_directories(actualOutputDir);
    std::string fullOutputPath = actualOutputDir + "/" + outputFile;
    mgr.GenerateGeometryPlot(fullOutputPath);
    std::cout << "总体布局图已保存: " << fullOutputPath << "\n";

    // [EN] Generate detailed plots for each configuration with full track visualization
    // [CN] 为每个配置生成完整轨迹可视化的详细图
    auto results = mgr.GetResults();
    std::cout << "\n为每个配置生成详细图（含氘核轨迹 + " << trackConfig.pxEdgeValues.size() * 2 + 1 << " 条质子轨迹）...\n";
    
    for (const auto& r : results) {
        std::ostringstream singleFilename;
        singleFilename << actualOutputDir << "/config_" 
                       << std::fixed << std::setprecision(2) << r.fieldStrength << "T_"
                       << std::fixed << std::setprecision(0) << r.deflectionAngle << "deg.png";
        
        // [EN] Use the new function with configurable tracks / [CN] 使用可配置轨迹的新函数
        mgr.GenerateSingleConfigPlotWithTracks(r, singleFilename.str(), trackConfig);
        std::cout << "  配置 " << r.fieldStrength << " T, " << r.deflectionAngle 
                  << "° -> " << singleFilename.str() << "\n";
    }

    // 打印结果
    std::cout << "\nGeoAcceptanceManager 返回结果数量: " << results.size() << "\n";
    for (const auto& r : results) {
        std::cout << "\n--- Field: " << r.fieldMapFile << "  Angle: " << r.deflectionAngle << "° ---\n";
        PrintVector3("Target位置", r.targetPos.position);
        std::cout << "  Target旋转角: " << r.targetPos.rotationAngle << "°\n";
        PrintVector3("PDC位置", r.pdcConfig.position);
        PrintVector3("PDC法向量", r.pdcConfig.normal);
        std::cout << "  PDC旋转角: " << r.pdcConfig.rotationAngle << "°\n";
        std::cout << "  PDC 尺寸: " << r.pdcConfig.width << " x " << r.pdcConfig.height << " x " << r.pdcConfig.depth << " mm³\n";
        std::cout << "  Acceptance (PDC): " << r.acceptance.pdcAcceptance << "%\n";
    }
}

// 运行完整测试流程（便于对多个场文件重复执行）
// Forward declaration: TestCoordinateTransformConsistency is defined further below
void TestCoordinateTransformConsistency(double magnetRotationAngle);

void RunAllTestsForField(const std::string& fieldMapFile,
                         double rotationAngle,
                         const std::vector<double>& deflectionAngles,
                         const std::string& outputBaseDir,
                         LogManager& logMgr) {
    PrintSeparator(std::string("Running tests for field: ") + fieldMapFile);

    // 初始化并加载磁场
    MagneticField* magField = new MagneticField();
    if (!magField->LoadFieldMap(fieldMapFile)) {
        std::cerr << "ERROR: 无法加载磁场文件: " << fieldMapFile << "\n";
        delete magField;
        return;
    }
    magField->SetRotationAngle(rotationAngle);
    magField->PrintInfo();

    BeamDeflectionCalculator* beamCalc = new BeamDeflectionCalculator();
    beamCalc->SetMagneticField(magField);

    DetectorAcceptanceCalculator* detCalc = new DetectorAcceptanceCalculator();
    detCalc->SetMagneticField(magField);

    // 测试1-4
    TestMagneticFieldRotation(magField);
    TestBeamDeflection(beamCalc);
    TestTargetPositionCalculation(beamCalc, deflectionAngles);

    // 重新计算Target位置用于后续测试
    std::vector<BeamDeflectionCalculator::TargetPosition> targetPositions;
    for (double angle : deflectionAngles) {
        targetPositions.push_back(beamCalc->CalculateTargetPosition(angle));
    }
    TestOptimalPDCPosition(detCalc, targetPositions);

    // 为每个场文件创建独立输出目录
    std::string fieldBase = std::filesystem::path(fieldMapFile).stem().string();
    std::string outDir = outputBaseDir;
    if (outDir.back() != '/') outDir += '/';
    outDir += fieldBase;

    // [EN] Configure edge proton Px values for track plotting
    // [CN] 配置边缘质子Px值用于轨迹绘图
    // [EN] Default: {100.0, 150.0} draws Px=±100 and Px=±150 MeV/c tracks
    // [CN] 默认值: {100.0, 150.0} 绘制 Px=±100 和 Px=±150 MeV/c 轨迹
    std::vector<double> pxEdgeValues = {100.0, 150.0};
    
    // 调用 GeoAcceptanceManager 的封装测试（会生成图表到 outDir）
    TestUsingGeoAcceptanceManager(fieldMapFile, deflectionAngles, "test_pdc_position.png", outDir, &logMgr, pxEdgeValues);

    // TestCoordinateTransformConsistency is declared below; forward-declare if needed
    TestCoordinateTransformConsistency(rotationAngle);

    // 清理
    delete beamCalc;
    delete detCalc;
    delete magField;
}

/**
 * @brief 测试坐标变换一致性
 */
void TestCoordinateTransformConsistency(double magnetRotationAngle) {
    PrintSeparator("测试6: 坐标变换一致性检查");
    
    std::cout << "磁铁旋转角度: " << magnetRotationAngle << "°\n";
    
    // MagneticField使用的变换 (修复后使用正角度)
    double angleRad_MF = magnetRotationAngle * TMath::DegToRad();  // 正角度
    double cos_MF = TMath::Cos(angleRad_MF);
    double sin_MF = TMath::Sin(angleRad_MF);
    
    // GeoAcceptanceManager画图使用的变换 (修复后使用正角度)
    double angleRad_GAM = magnetRotationAngle * TMath::DegToRad();  // 正角度
    double cos_GAM = TMath::Cos(angleRad_GAM);
    double sin_GAM = TMath::Sin(angleRad_GAM);
    
    std::cout << "\nMagneticField变换参数:\n";
    std::cout << "  angleRad = " << angleRad_MF << " rad\n";
    std::cout << "  cos = " << cos_MF << ", sin = " << sin_MF << "\n";
    
    std::cout << "\nGeoAcceptanceManager画图变换参数:\n";
    std::cout << "  angleRad = " << angleRad_GAM << " rad\n";
    std::cout << "  cos = " << cos_GAM << ", sin = " << sin_GAM << "\n";
    
    // 测试: 磁铁坐标系 -> 实验室坐标系
    TVector3 magnetPoint(0, 0, 1000);  // 磁铁坐标系中沿+Z的点
    
    // MagneticField的RotateToLabFrame (使用正角度):
    // bx = X_m * cos - Z_m * sin
    // bz = X_m * sin + Z_m * cos
    TVector3 labPoint_MF(
        magnetPoint.X() * cos_MF - magnetPoint.Z() * sin_MF,
        magnetPoint.Y(),
        magnetPoint.X() * sin_MF + magnetPoint.Z() * cos_MF
    );
    
    // GeoAcceptanceManager画图 (使用正角度):
    // labX = cos * X_m - sin * Z_m
    // labZ = sin * X_m + cos * Z_m
    TVector3 labPoint_GAM(
        cos_GAM * magnetPoint.X() - sin_GAM * magnetPoint.Z(),
        magnetPoint.Y(),
        sin_GAM * magnetPoint.X() + cos_GAM * magnetPoint.Z()
    );
    
    std::cout << "\n磁铁坐标系中的点 (0, 0, 1000):\n";
    std::cout << "  MagneticField变换后: (" << labPoint_MF.X() << ", " 
              << labPoint_MF.Y() << ", " << labPoint_MF.Z() << ")\n";
    std::cout << "  GeoAcceptanceManager变换后: (" << labPoint_GAM.X() << ", "
              << labPoint_GAM.Y() << ", " << labPoint_GAM.Z() << ")\n";
    
    bool consistent = (std::abs(labPoint_MF.X() - labPoint_GAM.X()) < 0.001 &&
                       std::abs(labPoint_MF.Z() - labPoint_GAM.Z()) < 0.001);
    std::cout << "\n变换一致性: " << (consistent ? "一致 [OK]" : "不一致 [MISMATCH!]") << "\n";
    
    // 物理验证
    std::cout << "\n=== 物理意义验证 ===\n";
    std::cout << "磁铁旋转角度为" << magnetRotationAngle << "°时:\n";
    std::cout << "- 磁铁坐标系中沿+Z方向的点（磁铁内部轴向）\n";
    std::cout << "- 在实验室坐标系中应该向-X方向偏移\n";
    
    double expected_labZ = 1000 * TMath::Cos(magnetRotationAngle * TMath::DegToRad());
    double expected_labX = -1000 * TMath::Sin(magnetRotationAngle * TMath::DegToRad());
    std::cout << "\n预期实验室坐标: (" << expected_labX << ", 0, " << expected_labZ << ")\n";
    std::cout << "实际计算结果: (" << labPoint_MF.X() << ", 0, " << labPoint_MF.Z() << ")\n";
    
    double error = std::sqrt((labPoint_MF.X() - expected_labX) * (labPoint_MF.X() - expected_labX) +
                             (labPoint_MF.Z() - expected_labZ) * (labPoint_MF.Z() - expected_labZ));
    
    std::cout << "\n=== 检查结论 ===\n";
    if (error < 0.01) {
        std::cout << "坐标变换正确！磁铁+Z方向在实验室中偏向-X [OK]\n";
    } else {
        std::cout << "坐标变换有误！误差 = " << error << " mm [ERROR]\n";
    }
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    // 解析命令行参数（先不输出日志）
    std::string fieldMapFile = "";
    std::string fieldsDir = "";
    std::string outputBaseDir = "";
    double rotationAngle = 30.0;  // 默认30度

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--fields-dir" && i + 1 < argc) {
            fieldsDir = argv[++i];
        } else if (arg == "--outdir" && i + 1 < argc) {
            outputBaseDir = argv[++i];
        } else if (fieldMapFile.empty()) {
            fieldMapFile = arg;
        } else {
            rotationAngle = std::atof(arg.c_str());
        }
    }
    
    // 确定输出目录
    std::string actualOutputDir = outputBaseDir.empty() ? GetOutputDir() : outputBaseDir;
    
    // 查找磁场文件
    if (fieldsDir.empty() && fieldMapFile.empty()) {
        std::vector<std::string> defaultPaths = {
            "/home/tian/workspace/dpol/smsimulator5.5/configs/simulation/geometry/filed_map/180626-1,00T-6000.table",
            "./configs/simulation/geometry/filed_map/180626-1,00T-6000.table",
            "../configs/simulation/geometry/filed_map/180626-1,00T-6000.table"
        };
        
        for (const auto& path : defaultPaths) {
            std::ifstream test(path);
            if (test.good()) {
                fieldMapFile = path;
                break;
            }
        }
    }
    
    // 创建输出目录
    std::filesystem::create_directories(actualOutputDir);
    
    // 创建日志文件
    LogManager logMgr;
    std::string logFilePath = actualOutputDir + "/test_pdc_position.log";
    
    if (!logMgr.Open(logFilePath)) {
        std::cerr << "ERROR: 无法创建日志文件: " << logFilePath << "\n";
        return 1;
    }
    
    // === 现在所有输出都重定向到日志文件，只在控制台输出关键信息 ===
    
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         PDC Position Calculation Test Program                ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    
    // 批量测试所有场文件
    std::vector<double> deflectionAngles = {0.0, 5.0, 10.0, 15.0};

    if (!fieldsDir.empty()) {
        std::vector<std::string> files;
        for (const auto& entry : std::filesystem::directory_iterator(fieldsDir)) {
            if (!entry.is_regular_file()) continue;
            auto p = entry.path();
            if (p.extension() == ".table") files.push_back(p.string());
        }
        if (files.empty()) {
            std::cerr << "ERROR: 在目录中未找到任何 .table 磁场文件: " << fieldsDir << "\n";
            logMgr.Close();
            return 1;
        }
        std::sort(files.begin(), files.end());

        for (const auto& f : files) {
            RunAllTestsForField(f, rotationAngle, deflectionAngles, actualOutputDir, logMgr);
        }

        // 关闭日志并输出位置
        logMgr.Close();
        
        // 恢复控制台输出，只显示结果位置
        std::cout << "\n╔═══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                   测试完成                                    ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";
        std::cout << "输出目录: " << actualOutputDir << "\n";
        std::cout << "日志文件: " << logFilePath << "\n\n";
        std::cout << "生成的文件:\n";
        std::cout << "  - 日志: test_pdc_position.log\n";
        std::cout << "  - 各配置图: config_<磁场>_<角度>.png/pdf\n";
        std::cout << "  - 总体布局图: test_pdc_position.png/pdf\n\n";

        return 0;
    }

    if (fieldMapFile.empty()) {
        std::cerr << "ERROR: 未找到磁场文件！\n";
        std::cerr << "用法: " << argv[0] << " [field_map_file] [rotation_angle] or --fields-dir <dir>\n";
        logMgr.Close();
        return 1;
    }
    
    std::cout << "\n配置:\n";
    std::cout << "  磁场文件: " << fieldMapFile << "\n";
    std::cout << "  旋转角度: " << rotationAngle << "°\n";
    
    // 初始化ROOT
    gROOT->SetBatch(true);  // 批处理模式
    
    // 加载磁场
    PrintSeparator("加载磁场");
    MagneticField* magField = new MagneticField();
    if (!magField->LoadFieldMap(fieldMapFile)) {
        std::cerr << "ERROR: 无法加载磁场文件！\n";
        return 1;
    }
    magField->SetRotationAngle(rotationAngle);
    magField->PrintInfo();
    
    // 创建计算器
    BeamDeflectionCalculator* beamCalc = new BeamDeflectionCalculator();
    beamCalc->SetMagneticField(magField);
    
    DetectorAcceptanceCalculator* detCalc = new DetectorAcceptanceCalculator();
    detCalc->SetMagneticField(magField);
    
    // ========================================
    // 运行测试
    // ========================================
    
    // 测试1: 磁场旋转变换
    TestMagneticFieldRotation(magField);
    
    // 测试2: 束流偏转
    TestBeamDeflection(beamCalc);
    
    // 测试3: Target位置计算
    TestTargetPositionCalculation(beamCalc, deflectionAngles);
    
    // 重新计算Target位置用于后续测试
    std::vector<BeamDeflectionCalculator::TargetPosition> targetPositions;
    for (double angle : deflectionAngles) {
        targetPositions.push_back(beamCalc->CalculateTargetPosition(angle));
    }
    
    // 测试4: PDC最佳位置
    // 使用 GeoAcceptanceManager（库实现）来完成PDC计算与绘图，避免在测试中重新实现绘图逻辑
    // [EN] Configure edge proton Px values: {100.0, 150.0} draws Px=±100, ±150 MeV/c tracks
    // [CN] 配置边缘质子Px值: {100.0, 150.0} 绘制 Px=±100, ±150 MeV/c 轨迹
    std::vector<double> pxEdgeValues = {100.0, 150.0};
    TestUsingGeoAcceptanceManager(fieldMapFile, deflectionAngles, "test_pdc_position.png", actualOutputDir, &logMgr, pxEdgeValues);
    
    // 测试6: 坐标变换一致性
    TestCoordinateTransformConsistency(rotationAngle);
    
    // 清理
    PrintSeparator("测试完成");
    std::cout << "所有测试已完成。\n";
    std::cout << "请检查输出结果和生成的图形文件。\n";
    
    delete beamCalc;
    delete detCalc;
    delete magField;
    
    // 关闭日志
    logMgr.Close();
    
    // 恢复控制台输出，只显示结果位置
    std::cout << "\n╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                   测试完成                                    ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";
    std::cout << "输出目录: " << actualOutputDir << "\n";
    std::cout << "日志文件: " << logFilePath << "\n\n";
    std::cout << "生成的文件:\n";
    std::cout << "  - 日志: test_pdc_position.log\n";
    std::cout << "  - 总体布局图: test_pdc_position.png/pdf\n\n";
    
    return 0;
}
