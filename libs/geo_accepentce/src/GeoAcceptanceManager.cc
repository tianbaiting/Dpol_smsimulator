#include "GeoAcceptanceManager.hh"
#include "SMLogger.hh"
#include "ParticleTrajectory.hh"
#include "TFile.h"
#include "TTree.h"
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
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstdlib>
#include <regex>

// 从磁场文件名解析磁场强度
// 文件名格式示例: "141114-0,8T-6000.table" -> 0.8 T
//                 "180626-1,00T-6000.table" -> 1.0 T
//                 "180703-1,40T-6000.table" -> 1.4 T
static double ParseFieldStrengthFromFilename(const std::string& filename) {
    // 从路径中提取文件名
    std::string basename = filename;
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        basename = filename.substr(lastSlash + 1);
    }
    
    // 使用正则表达式匹配格式 "数字,数字T" 或 "数字.数字T"
    // 例如: "0,8T", "1,00T", "1,40T"
    std::regex pattern(R"((\d+)[,.](\d+)T)");
    std::smatch match;
    
    if (std::regex_search(basename, match, pattern)) {
        std::string intPart = match[1].str();
        std::string decPart = match[2].str();
        // 将逗号格式转换为小数: "0,8" -> "0.8", "1,00" -> "1.00"
        double value = std::stod(intPart + "." + decPart);
        return value;
    }
    
    // 如果无法解析，返回默认值 1.0
    SM_WARN("Could not parse field strength from filename: {}, using default 1.0T", filename);
    return 1.0;
}

// AnalysisConfig 构造函数 - 使用环境变量设置默认路径
GeoAcceptanceManager::AnalysisConfig::AnalysisConfig()
    : useFixedPDC(false),
      fixedPDCPosition(-3625.0, 0.0, 1690.0),  // [EN] Default PDC position / [CN] 默认PDC位置
      fixedPDCRotationAngle(-25.0),          // [EN] Default PDC rotation / [CN] 默认PDC旋转角度
      pxRange(100.0)                        // [EN] Default Px range / [CN] 默认Px范围
{
    // 获取 SMSIMDIR 环境变量
    const char* smsimdir = std::getenv("SMSIMDIR");
    std::string baseDir = smsimdir ? std::string(smsimdir) : "/home/tian/workspace/dpol/smsimulator5.5";
    
    qmdDataPath = baseDir + "/data/qmdrawdata";
    outputPath = baseDir + "/results";
    
    // 默认配置: 0°, 5°, 10°
    deflectionAngles = {0.0, 5.0, 10.0};
}

GeoAcceptanceManager::GeoAcceptanceManager()
    : fCurrentMagField(nullptr),
      fBeamCalc(nullptr),
      fDetectorCalc(nullptr)
{
    fBeamCalc = new BeamDeflectionCalculator();
    fDetectorCalc = new DetectorAcceptanceCalculator();
}

GeoAcceptanceManager::~GeoAcceptanceManager()
{
    if (fBeamCalc) {
        delete fBeamCalc;
        fBeamCalc = nullptr;
    }
    
    if (fDetectorCalc) {
        delete fDetectorCalc;
        fDetectorCalc = nullptr;
    }
    
    if (fCurrentMagField) {
        delete fCurrentMagField;
        fCurrentMagField = nullptr;
    }
    
    CloseLogFile();
}

void GeoAcceptanceManager::OpenLogFile(const std::string& logPath)
{
    CloseLogFile();  // 关闭已打开的日志文件
    
    fLogFilePath = logPath;
    fLogFile.open(logPath);
    
    if (fLogFile.is_open()) {
        // 重定向 std::cout 到文件
        std::cout.rdbuf(fLogFile.rdbuf());
    }
}

void GeoAcceptanceManager::CloseLogFile()
{
    if (fLogFile.is_open()) {
        // 恢复 std::cout 到标准输出
        static std::streambuf* coutBuf = std::cout.rdbuf();
        std::cout.rdbuf(coutBuf);
        
        fLogFile.close();
    }
}

void GeoAcceptanceManager::AddFieldMap(const std::string& filename, double fieldStrength)
{
    fConfig.fieldMapFiles.push_back(filename);
    fConfig.fieldStrengths.push_back(fieldStrength);
}

void GeoAcceptanceManager::AddDeflectionAngle(double angle)
{
    fConfig.deflectionAngles.push_back(angle);
}

bool GeoAcceptanceManager::RunFullAnalysis()
{
    SM_INFO("");
    SM_INFO("╔═══════════════════════════════════════════════════════════════╗");
    SM_INFO("║     Geometry Acceptance Full Analysis - Starting...          ║");
    SM_INFO("╚═══════════════════════════════════════════════════════════════╝");
    SM_INFO("");
    
    PrintConfiguration();
    
    // 创建输出目录
    struct stat info;
    if (stat(fConfig.outputPath.c_str(), &info) != 0) {
        SM_INFO("Creating output directory: {}", fConfig.outputPath);
        system(("mkdir -p " + fConfig.outputPath).c_str());
    }
    
    // 加载QMD数据（只需加载一次）
    SM_INFO("Loading QMD data...");
    if (!fDetectorCalc->LoadQMDDataFromDirectory(fConfig.qmdDataPath)) {
        SM_ERROR("Failed to load QMD data!");
        return false;
    }
    
    // 遍历所有磁场配置
    for (size_t i = 0; i < fConfig.fieldMapFiles.size(); i++) {
        std::string fieldFile = fConfig.fieldMapFiles[i];
        double fieldStrength = fConfig.fieldStrengths[i];
        
        SM_INFO("");
        SM_INFO("============================================================");
        SM_INFO("Analyzing field configuration {}/{}", i+1, fConfig.fieldMapFiles.size());
        SM_INFO("  Field map: {}", fieldFile);
        SM_INFO("  Field strength: {} T", fieldStrength);
        SM_INFO("============================================================");
        
        if (!AnalyzeFieldConfiguration(fieldFile, fieldStrength)) {
            SM_WARN("Failed to analyze field configuration: {}", fieldFile);
            continue;
        }
    }
    
    SM_INFO("");
    SM_INFO("╔═══════════════════════════════════════════════════════════════╗");
    SM_INFO("║          Full Analysis Completed Successfully!               ║");
    SM_INFO("╚═══════════════════════════════════════════════════════════════╝");
    
    // 生成报告
    GenerateSummaryTable();
    
    std::string textReport = fConfig.outputPath + "/acceptance_report.txt";
    std::string rootFile = fConfig.outputPath + "/acceptance_results.root";
    std::string geoPlot = fConfig.outputPath + "/geometry_layout.png";
    
    GenerateTextReport(textReport);
    GenerateROOTFile(rootFile);
    GenerateGeometryPlot(geoPlot);
    
    return true;
}

bool GeoAcceptanceManager::AnalyzeFieldConfiguration(const std::string& fieldMapFile,
                                                      double fieldStrength)
{
    // 清空之前的结果，避免重复
    fResults.clear();
    
    // 如果 fieldStrength 为默认值 1.0，尝试从文件名解析实际磁场强度
    double actualFieldStrength = fieldStrength;
    if (std::abs(fieldStrength - 1.0) < 0.001) {
        actualFieldStrength = ParseFieldStrengthFromFilename(fieldMapFile);
        SM_INFO("Parsed field strength from filename: {} T", actualFieldStrength);
    }
    
    // 加载磁场
    SM_INFO("Loading magnetic field map...");
    
    if (fCurrentMagField) {
        delete fCurrentMagField;
    }
    fCurrentMagField = new MagneticField(fieldMapFile);
    
    if (!fCurrentMagField) {
        SM_ERROR("Failed to load magnetic field!");
        return false;
    }
    
    // 设置磁场到计算器
    fBeamCalc->SetMagneticField(fCurrentMagField);
    fDetectorCalc->SetMagneticField(fCurrentMagField);
    
    // 打印束流信息
    fBeamCalc->PrintBeamInfo();
    
    // 遍历所有偏转角度
    for (double angle : fConfig.deflectionAngles) {
        SM_INFO("");
        SM_INFO("------------------------------------------------------------");
        SM_INFO("Analyzing deflection angle: {}°", angle);
        SM_INFO("------------------------------------------------------------");
        
        ConfigurationResult result;
        result.fieldStrength = actualFieldStrength;  // 使用解析后的磁场强度
        result.fieldMapFile = fieldMapFile;
        result.deflectionAngle = angle;
        
        // 1. 计算target位置（从入射点追踪氘核轨迹）
        result.targetPos = fBeamCalc->CalculateTargetPosition(angle);
        
        // 2. [EN] Get PDC configuration based on mode / [CN] 根据模式获取PDC配置
        if (fConfig.useFixedPDC) {
            // [EN] Use fixed PDC position / [CN] 使用固定PDC位置
            SM_INFO("  Using FIXED PDC position mode");
            SM_INFO("    PDC position: ({:.1f}, {:.1f}, {:.1f}) mm", 
                    fConfig.fixedPDCPosition.X(), fConfig.fixedPDCPosition.Y(), fConfig.fixedPDCPosition.Z());
            SM_INFO("    PDC rotation: {:.2f} deg", fConfig.fixedPDCRotationAngle);
            result.pdcConfig = fDetectorCalc->CreateFixedPDCConfiguration(
                fConfig.fixedPDCPosition,
                fConfig.fixedPDCRotationAngle,
                -fConfig.pxRange,
                fConfig.pxRange
            );
        } else {
            // [EN] Calculate optimal PDC position / [CN] 计算最佳PDC位置
            SM_INFO("  Using OPTIMIZED PDC position mode");
            result.pdcConfig = fDetectorCalc->CalculateOptimalPDCPosition(
                result.targetPos.position, result.targetPos.rotationAngle, fConfig.pxRange);
        }
        
        // [EN] Set PDC configuration before calculating acceptance / [CN] 在计算接受度之前设置PDC配置
        fDetectorCalc->SetPDCConfiguration(result.pdcConfig);
        
        // 3. 计算接受度
        result.acceptance = fDetectorCalc->CalculateAcceptanceForTarget(
            result.targetPos.position, result.targetPos.rotationAngle);
        
        // 4. 打印报告
        fDetectorCalc->PrintAcceptanceReport(result.acceptance, angle, fieldStrength);
        
        // 保存结果
        fResults.push_back(result);
    }
    
    return true;
}

void GeoAcceptanceManager::GenerateSummaryTable() const
{
    SM_INFO("");
    SM_INFO("╔══════════════════════════════════════════════════════════════════════════════════════════════════╗");
    SM_INFO("║                                    ACCEPTANCE SUMMARY TABLE                                      ║");
    SM_INFO("╠═══════╦═══════╦═══════════════════════════════════╦═══════╦════════════╦════════════╦════════════╣");
    SM_INFO("║ Field ║ Angle ║      Target Position (mm)        ║ TRot  ║    PDC     ║   NEBULA   ║ Coincidence║");
    SM_INFO("║  (T)  ║ (deg) ║        (X, Y, Z)                 ║ (deg) ║    (%)     ║    (%)     ║    (%)     ║");
    SM_INFO("╠═══════╬═══════╬═══════════════════════════════════╬═══════╬════════════╬════════════╬════════════╣");
    
    for (const auto& result : fResults) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "║ " << std::setw(5) << result.fieldStrength << " ║ ";
        oss << std::setw(5) << result.deflectionAngle << " ║ ";
        oss << "(" << std::setw(7) << result.targetPos.position.X() << ", "
            << std::setw(6) << result.targetPos.position.Y() << ", "
            << std::setw(7) << result.targetPos.position.Z() << ") ║ ";
        oss << std::setw(5) << result.targetPos.rotationAngle << " ║ ";
        oss << std::setw(10) << result.acceptance.pdcAcceptance << " ║ ";
        oss << std::setw(10) << result.acceptance.nebulaAcceptance << " ║ ";
        oss << std::setw(11) << result.acceptance.coincidenceAcceptance << " ║";
        SM_INFO("{}", oss.str());
    }
    
    SM_INFO("╚═══════╩═══════╩═══════════════════════════════════╩═══════╩════════════╩════════════╩════════════╝");
    SM_INFO("");
}

void GeoAcceptanceManager::GenerateTextReport(const std::string& filename) const
{
    std::ofstream outFile(filename);
    
    if (!outFile.is_open()) {
        SM_ERROR("Cannot create report file {}", filename);
        return;
    }
    
    outFile << "===============================================" << std::endl;
    outFile << "  Geometry Acceptance Analysis Report" << std::endl;
    outFile << "===============================================" << std::endl;
    outFile << std::endl;
    
    outFile << "Configuration:" << std::endl;
    outFile << "  QMD Data Path: " << fConfig.qmdDataPath << std::endl;
    outFile << "  Output Path: " << fConfig.outputPath << std::endl;
    outFile << "  Number of field configurations: " << fConfig.fieldMapFiles.size() << std::endl;
    outFile << "  Number of deflection angles: " << fConfig.deflectionAngles.size() << std::endl;
    outFile << std::endl;
    
    for (const auto& result : fResults) {
        outFile << "-----------------------------------------------" << std::endl;
        outFile << "Field Strength: " << result.fieldStrength << " T" << std::endl;
        outFile << "Deflection Angle: " << result.deflectionAngle << " deg" << std::endl;
        outFile << std::endl;
        
        outFile << "Target Position:" << std::endl;
        outFile << "  X = " << result.targetPos.position.X() << " mm" << std::endl;
        outFile << "  Y = " << result.targetPos.position.Y() << " mm" << std::endl;
        outFile << "  Z = " << result.targetPos.position.Z() << " mm" << std::endl;
        outFile << "  Rotation Angle = " << result.targetPos.rotationAngle << " deg" << std::endl;
        outFile << std::endl;
        
        outFile << "PDC Configuration:" << std::endl;
        outFile << "  Position: (" << result.pdcConfig.position.X() << ", "
                << result.pdcConfig.position.Y() << ", "
                << result.pdcConfig.position.Z() << ") mm" << std::endl;
        outFile << "  Rotation: " << result.pdcConfig.rotationAngle << " deg" << std::endl;
        outFile << "  Px range: [" << result.pdcConfig.pxMin << ", "
                << result.pdcConfig.pxMax << "] MeV/c" << std::endl;
        outFile << std::endl;
        
        outFile << "Acceptance Results:" << std::endl;
        outFile << "  Total Events: " << result.acceptance.totalEvents << std::endl;
        outFile << "  PDC Acceptance: " << result.acceptance.pdcAcceptance << " % ("
                << result.acceptance.pdcHits << " hits)" << std::endl;
        outFile << "  NEBULA Acceptance: " << result.acceptance.nebulaAcceptance << " % ("
                << result.acceptance.nebulaHits << " hits)" << std::endl;
        outFile << "  Coincidence: " << result.acceptance.coincidenceAcceptance << " % ("
                << result.acceptance.bothHits << " hits)" << std::endl;
        outFile << std::endl;
    }
    
    outFile.close();
    SM_INFO("Text report saved to: {}", filename);
}

void GeoAcceptanceManager::GenerateROOTFile(const std::string& filename) const
{
    TFile* outFile = TFile::Open(filename.c_str(), "RECREATE");
    if (!outFile || outFile->IsZombie()) {
        SM_ERROR("Cannot create ROOT file {}", filename);
        return;
    }
    
    // 创建TTree保存结果
    TTree* tree = new TTree("acceptance", "Detector Acceptance Results");
    
    double fieldStrength, deflectionAngle;
    double targetX, targetY, targetZ, targetRotation;
    double pdcX, pdcY, pdcZ, pdcRotation;
    int totalEvents, pdcHits, nebulaHits, bothHits;
    double pdcAcc, nebulaAcc, coincAcc;
    
    tree->Branch("fieldStrength", &fieldStrength, "fieldStrength/D");
    tree->Branch("deflectionAngle", &deflectionAngle, "deflectionAngle/D");
    tree->Branch("targetX", &targetX, "targetX/D");
    tree->Branch("targetY", &targetY, "targetY/D");
    tree->Branch("targetZ", &targetZ, "targetZ/D");
    tree->Branch("targetRotation", &targetRotation, "targetRotation/D");
    tree->Branch("pdcX", &pdcX, "pdcX/D");
    tree->Branch("pdcY", &pdcY, "pdcY/D");
    tree->Branch("pdcZ", &pdcZ, "pdcZ/D");
    tree->Branch("pdcRotation", &pdcRotation, "pdcRotation/D");
    tree->Branch("totalEvents", &totalEvents, "totalEvents/I");
    tree->Branch("pdcHits", &pdcHits, "pdcHits/I");
    tree->Branch("nebulaHits", &nebulaHits, "nebulaHits/I");
    tree->Branch("bothHits", &bothHits, "bothHits/I");
    tree->Branch("pdcAcceptance", &pdcAcc, "pdcAcceptance/D");
    tree->Branch("nebulaAcceptance", &nebulaAcc, "nebulaAcceptance/D");
    tree->Branch("coincidenceAcceptance", &coincAcc, "coincidenceAcceptance/D");
    
    for (const auto& result : fResults) {
        fieldStrength = result.fieldStrength;
        deflectionAngle = result.deflectionAngle;
        targetX = result.targetPos.position.X();
        targetY = result.targetPos.position.Y();
        targetZ = result.targetPos.position.Z();
        targetRotation = result.targetPos.rotationAngle;
        pdcX = result.pdcConfig.position.X();
        pdcY = result.pdcConfig.position.Y();
        pdcZ = result.pdcConfig.position.Z();
        pdcRotation = result.pdcConfig.rotationAngle;
        totalEvents = result.acceptance.totalEvents;
        pdcHits = result.acceptance.pdcHits;
        nebulaHits = result.acceptance.nebulaHits;
        bothHits = result.acceptance.bothHits;
        pdcAcc = result.acceptance.pdcAcceptance;
        nebulaAcc = result.acceptance.nebulaAcceptance;
        coincAcc = result.acceptance.coincidenceAcceptance;
        
        tree->Fill();
    }
    
    tree->Write();
    outFile->Close();
    delete outFile;
    
    SM_INFO("ROOT file saved to: {}", filename);
}

void GeoAcceptanceManager::PrintConfiguration() const
{
    SM_INFO("Analysis Configuration:");
    SM_INFO("  QMD Data Path: {}", fConfig.qmdDataPath);
    SM_INFO("  Output Path: {}", fConfig.outputPath);
    SM_INFO("  Field Maps: {} configurations", fConfig.fieldMapFiles.size());
    for (size_t i = 0; i < fConfig.fieldMapFiles.size(); i++) {
        SM_INFO("    {}. {} ({} T)", i+1, fConfig.fieldMapFiles[i], fConfig.fieldStrengths[i]);
    }
    std::ostringstream oss;
    oss << "  Deflection Angles: ";
    for (size_t i = 0; i < fConfig.deflectionAngles.size(); i++) {
        oss << fConfig.deflectionAngles[i] << "°";
        if (i < fConfig.deflectionAngles.size() - 1) oss << ", ";
    }
    SM_INFO("{}", oss.str());
}

void GeoAcceptanceManager::PrintResults() const
{
    GenerateSummaryTable();
}

GeoAcceptanceManager::AnalysisConfig GeoAcceptanceManager::CreateDefaultConfig()
{
    // 使用 AnalysisConfig 构造函数中的默认值（已使用环境变量）
    AnalysisConfig config;
    return config;
}

GeoAcceptanceManager::AnalysisConfig GeoAcceptanceManager::CreateConfig(
    const std::vector<std::string>& fieldMaps,
    const std::vector<double>& angles)
{
    // 使用 AnalysisConfig 构造函数中的默认值（已使用环境变量）
    AnalysisConfig config;
    config.fieldMapFiles = fieldMaps;
    config.deflectionAngles = angles;
    
    // 从文件名推断磁场强度（这是一个简化，实际应该明确指定）
    for (size_t i = 0; i < fieldMaps.size(); i++) {
        config.fieldStrengths.push_back(1.0); // 默认1T，需要根据实际情况修改
    }
    
    return config;
}

void GeoAcceptanceManager::GenerateGeometryPlot(const std::string& filename) const
{
    if (fResults.empty()) {
        SM_WARN("No results to plot!");
        return;
    }
    
    // 设置样式
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    
    // 创建画布 - X-Z平面俯视图
    TCanvas* canvas = new TCanvas("geoCanvas", "Detector Geometry Layout", 1800, 1200);
    canvas->SetGrid();
    canvas->SetMargin(0.10, 0.02, 0.10, 0.08);
    
    // 确定绘图范围 (扩大范围以显示完整轨迹)
    double zMin = -5000;
    double zMax = 8000;
    double xMin = -6000;    
    double xMax = 2000;
    
    // 创建坐标框架
    TH2F* frame = new TH2F("frame", "", 100, zMin, zMax, 100, xMin, xMax);
    frame->GetXaxis()->SetTitle("Z (mm) - Beam Direction");
    frame->GetYaxis()->SetTitle("X (mm)");
    frame->GetXaxis()->SetTitleSize(0.04);
    frame->GetYaxis()->SetTitleSize(0.04);
    frame->GetXaxis()->SetLabelSize(0.035);
    frame->GetYaxis()->SetLabelSize(0.035);
    frame->GetXaxis()->CenterTitle();
    frame->GetYaxis()->CenterTitle();
    frame->Draw();
    
    // 绘制坐标轴
    TArrow* zAxis = new TArrow(zMin + 200, 0, zMax - 200, 0, 0.02, "|>");
    zAxis->SetLineWidth(2);
    zAxis->SetLineColor(kBlack);
    zAxis->Draw();
    
    TArrow* xAxis = new TArrow(0, xMin + 200, 0, xMax - 200, 0.02, "|>");
    xAxis->SetLineWidth(2);
    xAxis->SetLineColor(kBlack);
    xAxis->Draw();
    
    // 绘制SAMURAI磁铁区域 (示意性)
    // 磁铁中心在原点，Z方向（沿磁铁轴向）较短，X方向（横向）较长
    double magnetHalfZ = 1000;  // 磁铁Z方向半长度（沿束流方向较短）
    double magnetHalfX = 1500;  // 磁铁X方向半长度（横向较长）

    // 使用实际设置的磁场旋转角度（如果可用），否则默认为30°
    // 磁铁旋转θ度意味着磁铁+Z方向在实验室中偏向-X方向
    double magnetAngleDeg = 30.0;
    if (fCurrentMagField) magnetAngleDeg = fCurrentMagField->GetRotationAngle();
    double magnetAngle = magnetAngleDeg * TMath::Pi() / 180.0;  // 使用正角度
    double cosTheta = TMath::Cos(magnetAngle);
    double sinTheta = TMath::Sin(magnetAngle);

    // corners 表示磁铁在 磁铁坐标系 (Z_m, X_m)
    double magnetCorners[5][2];  // 5个点闭合多边形 (will store lab Z, lab X)
    double corners[4][2] = {
        {-magnetHalfZ, -magnetHalfX},
        { magnetHalfZ, -magnetHalfX},
        { magnetHalfZ,  magnetHalfX},
        {-magnetHalfZ,  magnetHalfX}
    };

    for (int i = 0; i < 4; i++) {
        double Zm = corners[i][0];  // 磁铁坐标系中的 Z
        double Xm = corners[i][1];  // 磁铁坐标系中的 X
        // 从磁铁坐标系到实验室坐标系（与 RotateToLabFrame 相同的变换）
        // labX = cos(theta)*Xm - sin(theta)*Zm
        // labZ = sin(theta)*Xm + cos(theta)*Zm
        double labX = cosTheta * Xm - sinTheta * Zm;
        double labZ = sinTheta * Xm + cosTheta * Zm;
        magnetCorners[i][0] = labZ; // graph X axis is Z
        magnetCorners[i][1] = labX; // graph Y axis is X
    }
    magnetCorners[4][0] = magnetCorners[0][0];
    magnetCorners[4][1] = magnetCorners[0][1];

    TGraph* magnetGraph = new TGraph(5);
    for (int i = 0; i < 5; i++) {
        magnetGraph->SetPoint(i, magnetCorners[i][0], magnetCorners[i][1]);
    }
    magnetGraph->SetFillColorAlpha(kCyan, 0.3);
    magnetGraph->SetLineColor(kBlue);
    magnetGraph->SetLineWidth(2);
    magnetGraph->Draw("F");
    magnetGraph->Draw("L SAME");
    
    // 添加磁铁标签
    TLatex* magnetLabel = new TLatex(0, 200, "SAMURAI Magnet");
    magnetLabel->SetTextSize(0.03);
    magnetLabel->SetTextAlign(22);
    magnetLabel->SetTextColor(kBlue);
    magnetLabel->Draw();
    
    // 绘制束流入射线 (Z = -4000)
    TLine* beamLine = new TLine(-4000, 0, 0, 0);
    beamLine->SetLineColor(kGreen+2);
    beamLine->SetLineWidth(3);
    beamLine->SetLineStyle(2);
    beamLine->Draw();
    
    TLatex* beamLabel = new TLatex(-4000, 100, "Beam (190 MeV/u ^{2}H)");
    beamLabel->SetTextSize(0.025);
    beamLabel->SetTextColor(kGreen+2);
    beamLabel->Draw();
    
    // 颜色方案：不同偏转角度
    int colors[] = {kRed, kBlue, kGreen+2, kMagenta, kOrange+1, kCyan+1};
    int nColors = sizeof(colors) / sizeof(colors[0]);
    
    // 为每个配置绘制探测器位置
    TLegend* legend = new TLegend(0.65, 0.70, 0.98, 0.92);
    legend->SetBorderSize(1);
    legend->SetFillColor(kWhite);
    legend->SetTextSize(0.022);
    legend->SetHeader("Configurations", "C");
    
    // 创建粒子轨迹计算器用于绘制质子轨迹
    ParticleTrajectory* trajCalc = nullptr;
    if (fCurrentMagField) {
        trajCalc = new ParticleTrajectory(fCurrentMagField);
        trajCalc->SetStepSize(5.0);
        trajCalc->SetMaxDistance(10000.0);
        trajCalc->SetMaxTime(500.0);
    }
    
    // 质子质量常数
    const double kProtonMass = 938.272;  // MeV/c²
    
    int colorIdx = 0;
    for (const auto& result : fResults) {
        int color = colors[colorIdx % nColors];
        
        // 1. 绘制Target位置 (圆形标记)
        TEllipse* target = new TEllipse(result.targetPos.position.Z(), 
                                        result.targetPos.position.X(), 
                                        80, 80);
        target->SetFillColor(color);
        target->SetLineColor(color);
        target->SetLineWidth(2);
        target->Draw();
        
        // 1.5 绘制中心质子轨迹 (Px=0, Py=0, Pz=627 MeV/c 在target局部坐标系)
        if (trajCalc) {
            // 计算target局部坐标系到实验室坐标系的变换
            double targetRotRad = result.targetPos.rotationAngle * TMath::DegToRad();
            double cos_rot = TMath::Cos(targetRotRad);
            double sin_rot = TMath::Sin(targetRotRad);
            
            // 局部坐标系: Px_local=0, Py_local=0, Pz_local=627 MeV/c
            double pz_local = 627.0;  // MeV/c
            
            // 转换到实验室坐标系
            // px_lab = px_local*cos(θ) + pz_local*sin(θ)
            // pz_lab = -px_local*sin(θ) + pz_local*cos(θ)
            double px_lab = pz_local * sin_rot;  // px_local=0
            double py_lab = 0.0;
            double pz_lab = pz_local * cos_rot;
            
            TVector3 protonMom(px_lab, py_lab, pz_lab);
            double E_proton = TMath::Sqrt(protonMom.Mag2() + kProtonMass * kProtonMass);
            TLorentzVector protonP4(protonMom, E_proton);
            
            // 计算质子轨迹
            auto protonTraj = trajCalc->CalculateTrajectory(
                result.targetPos.position, protonP4, 1.0, kProtonMass);
            
            // 绘制质子轨迹
            if (protonTraj.size() > 10) {
                std::vector<double> trajZ, trajX;
                
                // 每隔几个点取一个，避免太密集
                size_t step = std::max(size_t(1), protonTraj.size() / 200);
                for (size_t i = 0; i < protonTraj.size(); i += step) {
                    trajZ.push_back(protonTraj[i].position.Z());
                    trajX.push_back(protonTraj[i].position.X());
                }
                
                TGraph* protonGraph = new TGraph(trajZ.size(), trajZ.data(), trajX.data());
                protonGraph->SetLineColor(color);
                protonGraph->SetLineWidth(2);
                protonGraph->SetLineStyle(1);  // 实线
                protonGraph->Draw("L SAME");
                
                // 在轨迹末端添加箭头指示方向
                if (protonTraj.size() > 2) {
                    size_t endIdx = protonTraj.size() - 1;
                    size_t midIdx = protonTraj.size() / 2;
                    
                    // 在轨迹中间位置画一个小箭头指示方向
                    double arrowZ1 = protonTraj[midIdx - 10].position.Z();
                    double arrowX1 = protonTraj[midIdx - 10].position.X();
                    double arrowZ2 = protonTraj[midIdx + 10].position.Z();
                    double arrowX2 = protonTraj[midIdx + 10].position.X();
                    
                    TArrow* trajArrow = new TArrow(arrowZ1, arrowX1, arrowZ2, arrowX2, 0.015, ">");
                    trajArrow->SetLineColor(color);
                    trajArrow->SetLineWidth(2);
                    trajArrow->Draw();
                }
            }
        }
        
        // 2. 绘制PDC (矩形)
        // PDC尺寸: 约 1680 x 780 mm (宽 x 高)
        double pdcHalfWidth = result.pdcConfig.width / 2.0;
        double pdcHalfDepth = result.pdcConfig.depth / 2.0;
        
        // PDC中心位置
        double pdcZ = result.pdcConfig.position.Z();
        double pdcX = result.pdcConfig.position.X();
        
        // PDC方向 (使用法向量，与test_pdc_position.cc一致)
        double nx = result.pdcConfig.normal.X();
        double nz = result.pdcConfig.normal.Z();
        
        // 切向量 (垂直于法向量，在XZ平面内)
        double tx = nz;   // 切向量
        double tz = -nx;
        
        // 四个角点（在XZ平面的投影）
        double corner1_z = pdcZ - pdcHalfWidth * tx + pdcHalfDepth * nz;
        double corner1_x = pdcX - pdcHalfWidth * tz + pdcHalfDepth * nx;
        double corner2_z = pdcZ + pdcHalfWidth * tx + pdcHalfDepth * nz;
        double corner2_x = pdcX + pdcHalfWidth * tz + pdcHalfDepth * nx;
        double corner3_z = pdcZ + pdcHalfWidth * tx - pdcHalfDepth * nz;
        double corner3_x = pdcX + pdcHalfWidth * tz - pdcHalfDepth * nx;
        double corner4_z = pdcZ - pdcHalfWidth * tx - pdcHalfDepth * nz;
        double corner4_x = pdcX - pdcHalfWidth * tz - pdcHalfDepth * nx;
        
        TGraph* pdcGraph = new TGraph(5);
        pdcGraph->SetPoint(0, corner1_z, corner1_x);
        pdcGraph->SetPoint(1, corner2_z, corner2_x);
        pdcGraph->SetPoint(2, corner3_z, corner3_x);
        pdcGraph->SetPoint(3, corner4_z, corner4_x);
        pdcGraph->SetPoint(4, corner1_z, corner1_x);  // 闭合
        pdcGraph->SetFillColorAlpha(color, 0.2);
        pdcGraph->SetLineColor(color);
        pdcGraph->SetLineWidth(2);
        pdcGraph->Draw("F");
        pdcGraph->Draw("L SAME");
        
        // 3. 绘制NEBULA位置 (固定在Z=5000附近)
        // NEBULA尺寸: 约 3600 x 1800 mm (宽 x 高)
        double nebulaZ = 5000;
        double nebulaHalfWidth = 1800;
        double nebulaHalfDepth = 100;  // 深度较薄用于显示
        
        TBox* nebula = new TBox(nebulaZ - nebulaHalfDepth, -nebulaHalfWidth,
                                nebulaZ + nebulaHalfDepth, nebulaHalfWidth);
        nebula->SetFillColorAlpha(color, 0.15);
        nebula->SetLineColor(color);
        nebula->SetLineWidth(2);
        nebula->SetLineStyle(2);
        nebula->Draw("L");
        
        // 添加标签
        std::ostringstream labelText;
        labelText << std::fixed << std::setprecision(1);
        labelText << result.fieldStrength << "T, " << result.deflectionAngle << "#circ";
        
        // Target标签
        TLatex* targetLabel = new TLatex(result.targetPos.position.Z() - 150, 
                                         result.targetPos.position.X() - 150,
                                         "T");
        targetLabel->SetTextSize(0.02);
        targetLabel->SetTextColor(color);
        targetLabel->Draw();
        
        // PDC标签
        TLatex* pdcLabel = new TLatex(result.pdcConfig.position.Z() + 100,
                                      result.pdcConfig.position.X() + 200,
                                      "PDC");
        pdcLabel->SetTextSize(0.02);
        pdcLabel->SetTextColor(color);
        pdcLabel->Draw();
        
        // 添加到图例
        std::ostringstream legendEntry;
        legendEntry << std::fixed << std::setprecision(1);
        legendEntry << result.fieldStrength << " T, " << result.deflectionAngle 
                   << "#circ (PDC: " << std::setprecision(1) << result.acceptance.pdcAcceptance << "%)";
        
        TGraph* legendMarker = new TGraph(1);
        legendMarker->SetMarkerStyle(20);
        legendMarker->SetMarkerColor(color);
        legendMarker->SetLineColor(color);
        legend->AddEntry(legendMarker, legendEntry.str().c_str(), "lp");
        
        colorIdx++;
    }
    
    // 清理轨迹计算器
    if (trajCalc) {
        delete trajCalc;
        trajCalc = nullptr;
    }
    
    // NEBULA标签 (只绘制一次)
    TLatex* nebulaLabel = new TLatex(5200, 0, "NEBULA");
    nebulaLabel->SetTextSize(0.025);
    nebulaLabel->SetTextColor(kBlack);
    nebulaLabel->SetTextAngle(90);
    nebulaLabel->Draw();
    
    legend->Draw();
    
    // 添加标题
    TLatex* title = new TLatex(0.5, 0.95, "Detector Geometry Layout (X-Z Plane, Top View)");
    title->SetNDC();
    title->SetTextSize(0.04);
    title->SetTextAlign(22);
    title->Draw();
    
    // 添加比例尺
    TLine* scaleLine = new TLine(-4000, -3500, -3000, -3500);
    scaleLine->SetLineWidth(3);
    scaleLine->SetLineColor(kBlack);
    scaleLine->Draw();
    
    TLatex* scaleLabel = new TLatex(-3500, -3700, "1000 mm");
    scaleLabel->SetTextSize(0.025);
    scaleLabel->SetTextAlign(22);
    scaleLabel->Draw();
    
    // 添加图例说明
    TLatex* note1 = new TLatex(0.10, 0.03, "T = Target, PDC = Proton Drift Chamber, NEBULA = Neutron Detector, Curves = Proton (P_{z}=627 MeV/c) trajectory");
    note1->SetNDC();
    note1->SetTextSize(0.022);
    note1->Draw();
    
    // 保存图像
    canvas->Update();
    canvas->SaveAs(filename.c_str());
    
    // 同时保存PDF版本
    std::string pdfFile = filename.substr(0, filename.find_last_of('.')) + ".pdf";
    canvas->SaveAs(pdfFile.c_str());
    
    SM_INFO("Geometry plot saved to: {}", filename);
    SM_INFO("Geometry plot (PDF) saved to: {}", pdfFile);
    
    delete canvas;
}

void GeoAcceptanceManager::GenerateSingleConfigPlot(const ConfigurationResult& result,
                                                     const std::string& filename) const
{
    // 设置样式
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    
    // 创建画布 - X-Z平面俯视图
    TCanvas* canvas = new TCanvas("singleConfigCanvas", "Single Configuration Layout", 1800, 1200);
    canvas->SetGrid();
    canvas->SetMargin(0.10, 0.02, 0.10, 0.08);
    
    // 确定绘图范围
    double zMin = -5000;
    double zMax = 8000;
    double xMin = -7000;
    double xMax = 2000;
    
    // 创建坐标框架
    TH2F* frame = new TH2F("frameSingle", "", 100, zMin, zMax, 100, xMin, xMax);
    frame->GetXaxis()->SetTitle("Z (mm) - Beam Direction");
    frame->GetYaxis()->SetTitle("X (mm)");
    frame->GetXaxis()->SetTitleSize(0.04);
    frame->GetYaxis()->SetTitleSize(0.04);
    frame->GetXaxis()->SetLabelSize(0.035);
    frame->GetYaxis()->SetLabelSize(0.035);
    frame->GetXaxis()->CenterTitle();
    frame->GetYaxis()->CenterTitle();
    frame->Draw();
    
    // 绘制坐标轴
    TArrow* zAxis = new TArrow(zMin + 200, 0, zMax - 200, 0, 0.02, "|>");
    zAxis->SetLineWidth(2);
    zAxis->SetLineColor(kBlack);
    zAxis->Draw();
    
    TArrow* xAxis = new TArrow(0, xMin + 200, 0, xMax - 200, 0.02, "|>");
    xAxis->SetLineWidth(2);
    xAxis->SetLineColor(kBlack);
    xAxis->Draw();
    
    // 绘制SAMURAI磁铁区域
    double magnetHalfZ = 1000;
    double magnetHalfX = 1500;
    double magnetAngleDeg = 30.0;
    if (fCurrentMagField) magnetAngleDeg = fCurrentMagField->GetRotationAngle();
    double magnetAngle = magnetAngleDeg * TMath::Pi() / 180.0;
    double cosTheta = TMath::Cos(magnetAngle);
    double sinTheta = TMath::Sin(magnetAngle);

    double magnetCorners[5][2];
    double corners[4][2] = {
        {-magnetHalfZ, -magnetHalfX},
        { magnetHalfZ, -magnetHalfX},
        { magnetHalfZ,  magnetHalfX},
        {-magnetHalfZ,  magnetHalfX}
    };

    for (int i = 0; i < 4; i++) {
        double Zm = corners[i][0];
        double Xm = corners[i][1];
        double labX = cosTheta * Xm - sinTheta * Zm;
        double labZ = sinTheta * Xm + cosTheta * Zm;
        magnetCorners[i][0] = labZ;
        magnetCorners[i][1] = labX;
    }
    magnetCorners[4][0] = magnetCorners[0][0];
    magnetCorners[4][1] = magnetCorners[0][1];

    TGraph* magnetGraph = new TGraph(5);
    for (int i = 0; i < 5; i++) {
        magnetGraph->SetPoint(i, magnetCorners[i][0], magnetCorners[i][1]);
    }
    magnetGraph->SetFillColorAlpha(kCyan, 0.3);
    magnetGraph->SetLineColor(kBlue);
    magnetGraph->SetLineWidth(2);
    magnetGraph->Draw("F");
    magnetGraph->Draw("L SAME");
    
    TLatex* magnetLabel = new TLatex(0, 200, "SAMURAI Magnet");
    magnetLabel->SetTextSize(0.03);
    magnetLabel->SetTextAlign(22);
    magnetLabel->SetTextColor(kBlue);
    magnetLabel->Draw();
    
    // 绘制束流入射线
    TLine* beamLine = new TLine(-4500, 0, 0, 0);
    beamLine->SetLineColor(kGreen+2);
    beamLine->SetLineWidth(3);
    beamLine->SetLineStyle(2);
    beamLine->Draw();
    
    TLatex* beamLabel = new TLatex(-4500, 150, "Beam (190 MeV/u ^{2}H)");
    beamLabel->SetTextSize(0.025);
    beamLabel->SetTextColor(kGreen+2);
    beamLabel->Draw();
    
    // 创建粒子轨迹计算器
    ParticleTrajectory* trajCalc = nullptr;
    if (fCurrentMagField) {
        trajCalc = new ParticleTrajectory(fCurrentMagField);
        trajCalc->SetStepSize(5.0);
        trajCalc->SetMaxDistance(10000.0);
        trajCalc->SetMaxTime(500.0);
    }
    
    const double kProtonMass = 938.272;
    
    // 图例
    TLegend* legend = new TLegend(0.65, 0.75, 0.98, 0.92);
    legend->SetBorderSize(1);
    legend->SetFillColor(kWhite);
    legend->SetTextSize(0.025);
    
    // 绘制Target位置
    TEllipse* target = new TEllipse(result.targetPos.position.Z(), 
                                    result.targetPos.position.X(), 
                                    100, 100);
    target->SetFillColor(kRed);
    target->SetLineColor(kRed);
    target->SetLineWidth(2);
    target->Draw();
    
    // 计算并绘制三条质子轨迹 (Px = -100, 0, +100 MeV/c)
    if (trajCalc) {
        double targetRotRad = result.targetPos.rotationAngle * TMath::DegToRad();
        double cos_rot = TMath::Cos(targetRotRad);
        double sin_rot = TMath::Sin(targetRotRad);
        double pz_local = 627.0;
        
        // 三种 Px 值: -100, 0, +100 MeV/c
        std::vector<double> pxValues = {-100.0, 0.0, 100.0};
        int trajColors[] = {kBlue, kRed, kGreen+2};
        int trajStyles[] = {2, 1, 2};  // 虚线, 实线, 虚线
        const char* trajLabels[] = {"P_{x}=-100 MeV/c", "P_{x}=0 (center)", "P_{x}=+100 MeV/c"};
        
        for (size_t iPx = 0; iPx < pxValues.size(); iPx++) {
            double px_local = pxValues[iPx];
            
            // 转换到实验室坐标系
            double px_lab = px_local * cos_rot + pz_local * sin_rot;
            double py_lab = 0.0;
            double pz_lab = -px_local * sin_rot + pz_local * cos_rot;
            
            TVector3 protonMom(px_lab, py_lab, pz_lab);
            double E_proton = TMath::Sqrt(protonMom.Mag2() + kProtonMass * kProtonMass);
            TLorentzVector protonP4(protonMom, E_proton);
            
            auto protonTraj = trajCalc->CalculateTrajectory(
                result.targetPos.position, protonP4, 1.0, kProtonMass);
            
            if (protonTraj.size() > 10) {
                std::vector<double> trajZ, trajX;
                size_t step = std::max(size_t(1), protonTraj.size() / 300);
                for (size_t i = 0; i < protonTraj.size(); i += step) {
                    trajZ.push_back(protonTraj[i].position.Z());
                    trajX.push_back(protonTraj[i].position.X());
                }
                
                TGraph* protonGraph = new TGraph(trajZ.size(), trajZ.data(), trajX.data());
                protonGraph->SetLineColor(trajColors[iPx]);
                protonGraph->SetLineWidth(2);
                protonGraph->SetLineStyle(trajStyles[iPx]);
                protonGraph->Draw("L SAME");
                
                legend->AddEntry(protonGraph, trajLabels[iPx], "l");
                
                // 在轨迹中间添加方向箭头
                if (protonTraj.size() > 40) {
                    size_t midIdx = protonTraj.size() / 2;
                    double arrowZ1 = protonTraj[midIdx - 15].position.Z();
                    double arrowX1 = protonTraj[midIdx - 15].position.X();
                    double arrowZ2 = protonTraj[midIdx + 15].position.Z();
                    double arrowX2 = protonTraj[midIdx + 15].position.X();
                    
                    TArrow* trajArrow = new TArrow(arrowZ1, arrowX1, arrowZ2, arrowX2, 0.012, ">");
                    trajArrow->SetLineColor(trajColors[iPx]);
                    trajArrow->SetLineWidth(2);
                    trajArrow->Draw();
                }
            }
        }
    }
    
    // 绘制PDC
    double pdcHalfWidth = result.pdcConfig.width / 2.0;
    double pdcHalfDepth = result.pdcConfig.depth / 2.0;
    double pdcZ = result.pdcConfig.position.Z();
    double pdcX = result.pdcConfig.position.X();
    double nx = result.pdcConfig.normal.X();
    double nz = result.pdcConfig.normal.Z();
    double tx = nz;
    double tz = -nx;
    
    double corner1_z = pdcZ - pdcHalfWidth * tx + pdcHalfDepth * nz;
    double corner1_x = pdcX - pdcHalfWidth * tz + pdcHalfDepth * nx;
    double corner2_z = pdcZ + pdcHalfWidth * tx + pdcHalfDepth * nz;
    double corner2_x = pdcX + pdcHalfWidth * tz + pdcHalfDepth * nx;
    double corner3_z = pdcZ + pdcHalfWidth * tx - pdcHalfDepth * nz;
    double corner3_x = pdcX + pdcHalfWidth * tz - pdcHalfDepth * nx;
    double corner4_z = pdcZ - pdcHalfWidth * tx - pdcHalfDepth * nz;
    double corner4_x = pdcX - pdcHalfWidth * tz - pdcHalfDepth * nx;
    
    TGraph* pdcGraph = new TGraph(5);
    pdcGraph->SetPoint(0, corner1_z, corner1_x);
    pdcGraph->SetPoint(1, corner2_z, corner2_x);
    pdcGraph->SetPoint(2, corner3_z, corner3_x);
    pdcGraph->SetPoint(3, corner4_z, corner4_x);
    pdcGraph->SetPoint(4, corner1_z, corner1_x);
    pdcGraph->SetFillColorAlpha(kOrange, 0.3);
    pdcGraph->SetLineColor(kOrange+1);
    pdcGraph->SetLineWidth(2);
    pdcGraph->Draw("F");
    pdcGraph->Draw("L SAME");
    
    // 绘制NEBULA
    double nebulaZ = 5000;
    double nebulaHalfWidth = 1800;
    double nebulaHalfDepth = 100;
    
    TBox* nebula = new TBox(nebulaZ - nebulaHalfDepth, -nebulaHalfWidth,
                            nebulaZ + nebulaHalfDepth, nebulaHalfWidth);
    nebula->SetFillColorAlpha(kGray, 0.3);
    nebula->SetLineColor(kGray+2);
    nebula->SetLineWidth(2);
    nebula->Draw("L");
    
    // 标签
    TLatex* targetLabel = new TLatex(result.targetPos.position.Z() - 200, 
                                     result.targetPos.position.X() - 200,
                                     "Target");
    targetLabel->SetTextSize(0.025);
    targetLabel->SetTextColor(kRed);
    targetLabel->Draw();
    
    TLatex* pdcLabel = new TLatex(result.pdcConfig.position.Z() + 150,
                                  result.pdcConfig.position.X() + 300,
                                  "PDC");
    pdcLabel->SetTextSize(0.025);
    pdcLabel->SetTextColor(kOrange+1);
    pdcLabel->Draw();
    
    TLatex* nebulaLabel = new TLatex(5200, 0, "NEBULA");
    nebulaLabel->SetTextSize(0.025);
    nebulaLabel->SetTextColor(kGray+2);
    nebulaLabel->SetTextAngle(90);
    nebulaLabel->Draw();
    
    legend->Draw();
    
    // 添加标题
    std::ostringstream titleText;
    titleText << std::fixed << std::setprecision(1);
    titleText << "Detector Layout: " << result.fieldStrength << " T, Deflection " 
              << result.deflectionAngle << "#circ";
    TLatex* title = new TLatex(0.5, 0.95, titleText.str().c_str());
    title->SetNDC();
    title->SetTextSize(0.04);
    title->SetTextAlign(22);
    title->Draw();
    
    // 添加配置信息
    std::ostringstream infoText;
    infoText << std::fixed << std::setprecision(1);
    infoText << "Target: (" << result.targetPos.position.X() << ", " 
             << result.targetPos.position.Y() << ", " 
             << result.targetPos.position.Z() << ") mm, Rot: " 
             << result.targetPos.rotationAngle << "#circ";
    TLatex* info1 = new TLatex(0.12, 0.88, infoText.str().c_str());
    info1->SetNDC();
    info1->SetTextSize(0.022);
    info1->Draw();
    
    std::ostringstream infoText2;
    infoText2 << std::fixed << std::setprecision(1);
    infoText2 << "PDC: (" << result.pdcConfig.position.X() << ", " 
              << result.pdcConfig.position.Y() << ", " 
              << result.pdcConfig.position.Z() << ") mm, Rot: " 
              << result.pdcConfig.rotationAngle << "#circ";
    TLatex* info2 = new TLatex(0.12, 0.85, infoText2.str().c_str());
    info2->SetNDC();
    info2->SetTextSize(0.022);
    info2->Draw();
    
    // 添加比例尺
    TLine* scaleLine = new TLine(-4500, -4500, -3500, -4500);
    scaleLine->SetLineWidth(3);
    scaleLine->SetLineColor(kBlack);
    scaleLine->Draw();
    
    TLatex* scaleLabel = new TLatex(-4000, -4700, "1000 mm");
    scaleLabel->SetTextSize(0.025);
    scaleLabel->SetTextAlign(22);
    scaleLabel->Draw();
    
    // 添加图例说明
    TLatex* note1 = new TLatex(0.10, 0.03, "Solid = center proton (P_{x}=0), Dashed = edge protons (P_{x}=#pm100 MeV/c), P_{z}=600 MeV/c");
    note1->SetNDC();
    note1->SetTextSize(0.020);
    note1->Draw();
    
    // 清理轨迹计算器
    if (trajCalc) {
        delete trajCalc;
    }
    
    // 保存图像
    canvas->Update();
    canvas->SaveAs(filename.c_str());
    
    std::string pdfFile = filename.substr(0, filename.find_last_of('.')) + ".pdf";
    canvas->SaveAs(pdfFile.c_str());
    
    SM_INFO("Single config plot saved to: {}", filename);
    
    delete canvas;
}

/**
 * @brief [EN] Generate single configuration plot with configurable particle tracks
 *        [CN] 使用可配置的粒子轨迹生成单个配置图
 * 
 * [EN] API Usage Example: / [CN] API使用示例:
 * @code
 *   GeoAcceptanceManager mgr;
 *   
 *   // Load magnetic field / 加载磁场
 *   mgr.AnalyzeFieldConfiguration("/path/to/180626-1,00T-3000.table", 1.0);
 *   auto results = mgr.GetResults();
 *   
 *   // Configure tracks / 配置轨迹
 *   TrackPlotConfig config;
 *   config.drawDeuteronTrack = true;     // Draw deuteron beam / 绘制氘核束流
 *   config.drawCenterProtonTrack = true; // Draw Px=0 proton / 绘制Px=0质子
 *   config.pxEdgeValues = {100.0, 150.0}; // Draw Px=±100, ±150 MeV/c protons
 *   config.pzProton = 627.0;              // Pz for proton tracks / 质子轨迹的Pz
 *   
 *   // Generate plot / 生成图
 *   mgr.GenerateSingleConfigPlotWithTracks(results[0], "output.png", config);
 * @endcode
 * 
 * [EN] Key Parameters: / [CN] 关键参数:
 * - Magnetic field strength: Set by AnalyzeFieldConfiguration() / 磁场强度: 通过AnalyzeFieldConfiguration()设置
 * - Beam deflection angle: Stored in result.deflectionAngle / 束流偏转角度: 存储在result.deflectionAngle
 * - Target position: Calculated automatically / 靶子位置: 自动计算
 * - PDC position: Can be fixed or optimized / PDC位置: 可固定或优化
 * 
 * @param result The configuration result containing target/PDC positions
 * @param filename Output filename (.png or .pdf)
 * @param trackConfig Configuration for which tracks to draw
 */
void GeoAcceptanceManager::GenerateSingleConfigPlotWithTracks(
    const ConfigurationResult& result,
    const std::string& filename,
    const TrackPlotConfig& trackConfig) const
{
    // [EN] Set up canvas / [CN] 设置画布
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    
    TCanvas* canvas = new TCanvas("configPlotTracks", "Configuration with Tracks", 1800, 1200);
    canvas->SetGrid();
    canvas->SetMargin(0.10, 0.02, 0.10, 0.08);
    
    // [EN] Determine plot range / [CN] 确定绘图范围
    double zMin = -5000, zMax = 8000;
    double xMin = -7000, xMax = 2000;
    
    TH2F* frame = new TH2F("frameWithTracks", "", 100, zMin, zMax, 100, xMin, xMax);
    frame->GetXaxis()->SetTitle("Z (mm) - Beam Direction");
    frame->GetYaxis()->SetTitle("X (mm)");
    frame->GetXaxis()->SetTitleSize(0.04);
    frame->GetYaxis()->SetTitleSize(0.04);
    frame->GetXaxis()->SetLabelSize(0.035);
    frame->GetYaxis()->SetLabelSize(0.035);
    frame->GetXaxis()->CenterTitle();
    frame->GetYaxis()->CenterTitle();
    frame->Draw();
    
    // [EN] Draw coordinate axes / [CN] 绘制坐标轴
    TArrow* zAxis = new TArrow(zMin + 200, 0, zMax - 200, 0, 0.02, "|>");
    zAxis->SetLineWidth(2);
    zAxis->SetLineColor(kBlack);
    zAxis->Draw();
    
    TArrow* xAxis = new TArrow(0, xMin + 200, 0, xMax - 200, 0.02, "|>");
    xAxis->SetLineWidth(2);
    xAxis->SetLineColor(kBlack);
    xAxis->Draw();
    
    // [EN] Draw SAMURAI magnet / [CN] 绘制SAMURAI磁铁
    double magnetHalfZ = 1000, magnetHalfX = 1500;
    double magnetAngleDeg = fCurrentMagField ? fCurrentMagField->GetRotationAngle() : 30.0;
    double magnetAngle = magnetAngleDeg * TMath::Pi() / 180.0;
    double cosTheta = TMath::Cos(magnetAngle);
    double sinTheta = TMath::Sin(magnetAngle);
    
    double magnetCorners[5][2];
    double corners[4][2] = {
        {-magnetHalfZ, -magnetHalfX}, {magnetHalfZ, -magnetHalfX},
        {magnetHalfZ, magnetHalfX}, {-magnetHalfZ, magnetHalfX}
    };
    for (int i = 0; i < 4; i++) {
        double Zm = corners[i][0], Xm = corners[i][1];
        magnetCorners[i][0] = sinTheta * Xm + cosTheta * Zm;
        magnetCorners[i][1] = cosTheta * Xm - sinTheta * Zm;
    }
    magnetCorners[4][0] = magnetCorners[0][0];
    magnetCorners[4][1] = magnetCorners[0][1];
    
    TGraph* magnetGraph = new TGraph(5);
    for (int i = 0; i < 5; i++) magnetGraph->SetPoint(i, magnetCorners[i][0], magnetCorners[i][1]);
    magnetGraph->SetFillColorAlpha(kCyan, 0.3);
    magnetGraph->SetLineColor(kBlue);
    magnetGraph->SetLineWidth(2);
    magnetGraph->Draw("F");
    magnetGraph->Draw("L SAME");
    
    TLatex* magnetLabel = new TLatex(0, 200, "SAMURAI Magnet");
    magnetLabel->SetTextSize(0.03);
    magnetLabel->SetTextAlign(22);
    magnetLabel->SetTextColor(kBlue);
    magnetLabel->Draw();
    
    // [EN] Create trajectory calculator / [CN] 创建轨迹计算器
    ParticleTrajectory* trajCalc = nullptr;
    if (fCurrentMagField) {
        trajCalc = new ParticleTrajectory(fCurrentMagField);
        trajCalc->SetStepSize(5.0);
        trajCalc->SetMaxDistance(10000.0);
        trajCalc->SetMaxTime(500.0);
    }
    
    // [EN] Physics constants / [CN] 物理常数
    const double kProtonMass = 938.272;    // MeV/c²
    const double kDeuteronMass = 1875.613; // MeV/c²
    
    // [EN] Legend / [CN] 图例
    TLegend* legend = new TLegend(0.62, 0.68, 0.98, 0.92);
    legend->SetBorderSize(1);
    legend->SetFillColor(kWhite);
    legend->SetTextSize(0.020);
    legend->SetHeader("Particle Tracks", "C");
    
    // ========== [EN] Draw Deuteron Track / [CN] 绘制氘核轨迹 ==========
    if (trackConfig.drawDeuteronTrack && trajCalc && fBeamCalc) {
        // [EN] Get full beam trajectory from beam calculator / [CN] 从束流计算器获取完整束流轨迹
        auto beamTraj = fBeamCalc->CalculateFullBeamTrajectory();
        
        if (beamTraj.size() > 10) {
            std::vector<double> trajZ, trajX;
            size_t step = std::max(size_t(1), beamTraj.size() / 300);
            for (size_t i = 0; i < beamTraj.size(); i += step) {
                trajZ.push_back(beamTraj[i].position.Z());
                trajX.push_back(beamTraj[i].position.X());
            }
            
            TGraph* deuteronGraph = new TGraph(trajZ.size(), trajZ.data(), trajX.data());
            deuteronGraph->SetLineColor(trackConfig.deuteronTrackColor);
            deuteronGraph->SetLineWidth(3);
            deuteronGraph->SetLineStyle(1);  // [EN] Solid line / [CN] 实线
            deuteronGraph->Draw("L SAME");
            
            legend->AddEntry(deuteronGraph, "Deuteron (^{2}H, 190 MeV/u)", "l");
            
            // [EN] Add direction arrow / [CN] 添加方向箭头
            if (beamTraj.size() > 40) {
                size_t midIdx = beamTraj.size() / 3;
                TArrow* arrow = new TArrow(beamTraj[midIdx-15].position.Z(), beamTraj[midIdx-15].position.X(),
                                          beamTraj[midIdx+15].position.Z(), beamTraj[midIdx+15].position.X(), 0.015, ">");
                arrow->SetLineColor(trackConfig.deuteronTrackColor);
                arrow->SetLineWidth(3);
                arrow->Draw();
            }
        }
    }
    
    // ========== [EN] Draw Proton Tracks / [CN] 绘制质子轨迹 ==========
    if (trajCalc) {
        double targetRotRad = result.targetPos.rotationAngle * TMath::DegToRad();
        double cos_rot = TMath::Cos(targetRotRad);
        double sin_rot = TMath::Sin(targetRotRad);
        double pz_local = trackConfig.pzProton;
        
        // [EN] Helper function to draw proton track / [CN] 绘制质子轨迹的辅助函数
        auto drawProtonTrack = [&](double px_local, int color, int lineStyle, const char* label) {
            double px_lab = px_local * cos_rot + pz_local * sin_rot;
            double pz_lab = -px_local * sin_rot + pz_local * cos_rot;
            
            TVector3 protonMom(px_lab, 0.0, pz_lab);
            double E = TMath::Sqrt(protonMom.Mag2() + kProtonMass * kProtonMass);
            TLorentzVector protonP4(protonMom, E);
            
            auto protonTraj = trajCalc->CalculateTrajectory(
                result.targetPos.position, protonP4, 1.0, kProtonMass);
            
            if (protonTraj.size() > 10) {
                std::vector<double> trajZ, trajX;
                size_t step = std::max(size_t(1), protonTraj.size() / 300);
                for (size_t i = 0; i < protonTraj.size(); i += step) {
                    trajZ.push_back(protonTraj[i].position.Z());
                    trajX.push_back(protonTraj[i].position.X());
                }
                
                TGraph* gr = new TGraph(trajZ.size(), trajZ.data(), trajX.data());
                gr->SetLineColor(color);
                gr->SetLineWidth(2);
                gr->SetLineStyle(lineStyle);
                gr->Draw("L SAME");
                
                if (label) legend->AddEntry(gr, label, "l");
                
                // [EN] Direction arrow / [CN] 方向箭头
                if (protonTraj.size() > 40) {
                    size_t midIdx = protonTraj.size() / 2;
                    TArrow* arrow = new TArrow(protonTraj[midIdx-15].position.Z(), protonTraj[midIdx-15].position.X(),
                                              protonTraj[midIdx+15].position.Z(), protonTraj[midIdx+15].position.X(), 0.012, ">");
                    arrow->SetLineColor(color);
                    arrow->SetLineWidth(2);
                    arrow->Draw();
                }
            }
        };
        
        // [EN] Draw center proton (Px=0) / [CN] 绘制中心质子 (Px=0)
        if (trackConfig.drawCenterProtonTrack) {
            drawProtonTrack(0.0, trackConfig.centerProtonColor, 1, "Proton P_{x}=0 (center)");
        }
        
        // [EN] Draw edge proton tracks / [CN] 绘制边缘质子轨迹
        for (size_t iPx = 0; iPx < trackConfig.pxEdgeValues.size(); iPx++) {
            double pxVal = trackConfig.pxEdgeValues[iPx];
            int color = trackConfig.edgeProtonColors[iPx % trackConfig.edgeProtonColors.size()];
            
            // [EN] Positive Px / [CN] 正Px
            char labelPos[64];
            snprintf(labelPos, sizeof(labelPos), "Proton P_{x}=+%.0f MeV/c", pxVal);
            drawProtonTrack(+pxVal, color, 2, labelPos);  // [EN] Dashed / [CN] 虚线
            
            // [EN] Negative Px / [CN] 负Px
            char labelNeg[64];
            snprintf(labelNeg, sizeof(labelNeg), "Proton P_{x}=-%.0f MeV/c", pxVal);
            drawProtonTrack(-pxVal, color, 2, nullptr);  // [EN] Same entry in legend / [CN] 同一图例项
        }
    }
    
    // ========== [EN] Draw Target / [CN] 绘制靶子 ==========
    TEllipse* target = new TEllipse(result.targetPos.position.Z(), result.targetPos.position.X(), 100, 100);
    target->SetFillColor(kRed);
    target->SetLineColor(kRed);
    target->SetLineWidth(2);
    target->Draw();
    
    TLatex* targetLabel = new TLatex(result.targetPos.position.Z() - 200, 
                                     result.targetPos.position.X() - 200, "Target");
    targetLabel->SetTextSize(0.025);
    targetLabel->SetTextColor(kRed);
    targetLabel->Draw();
    
    // ========== [EN] Draw PDC / [CN] 绘制PDC ==========
    double pdcHalfWidth = result.pdcConfig.width / 2.0;
    double pdcHalfDepth = result.pdcConfig.depth / 2.0;
    double pdcZ = result.pdcConfig.position.Z();
    double pdcX = result.pdcConfig.position.X();
    double nx = result.pdcConfig.normal.X(), nz = result.pdcConfig.normal.Z();
    double tx = nz, tz = -nx;
    
    TGraph* pdcGraph = new TGraph(5);
    pdcGraph->SetPoint(0, pdcZ - pdcHalfWidth*tx + pdcHalfDepth*nz, pdcX - pdcHalfWidth*tz + pdcHalfDepth*nx);
    pdcGraph->SetPoint(1, pdcZ + pdcHalfWidth*tx + pdcHalfDepth*nz, pdcX + pdcHalfWidth*tz + pdcHalfDepth*nx);
    pdcGraph->SetPoint(2, pdcZ + pdcHalfWidth*tx - pdcHalfDepth*nz, pdcX + pdcHalfWidth*tz - pdcHalfDepth*nx);
    pdcGraph->SetPoint(3, pdcZ - pdcHalfWidth*tx - pdcHalfDepth*nz, pdcX - pdcHalfWidth*tz - pdcHalfDepth*nx);
    pdcGraph->SetPoint(4, pdcZ - pdcHalfWidth*tx + pdcHalfDepth*nz, pdcX - pdcHalfWidth*tz + pdcHalfDepth*nx);
    pdcGraph->SetFillColorAlpha(kOrange, 0.3);
    pdcGraph->SetLineColor(kOrange+1);
    pdcGraph->SetLineWidth(2);
    pdcGraph->Draw("F");
    pdcGraph->Draw("L SAME");
    
    TLatex* pdcLabel = new TLatex(pdcZ + 150, pdcX + 300, "PDC");
    pdcLabel->SetTextSize(0.025);
    pdcLabel->SetTextColor(kOrange+1);
    pdcLabel->Draw();
    
    // ========== [EN] Draw NEBULA / [CN] 绘制NEBULA ==========
    TBox* nebula = new TBox(4900, -1800, 5100, 1800);
    nebula->SetFillColorAlpha(kGray, 0.3);
    nebula->SetLineColor(kGray+2);
    nebula->SetLineWidth(2);
    nebula->Draw("L");
    
    TLatex* nebulaLabel = new TLatex(5200, 0, "NEBULA");
    nebulaLabel->SetTextSize(0.025);
    nebulaLabel->SetTextColor(kGray+2);
    nebulaLabel->SetTextAngle(90);
    nebulaLabel->Draw();
    
    legend->Draw();
    
    // ========== [EN] Title and Info / [CN] 标题和信息 ==========
    std::ostringstream titleText;
    titleText << std::fixed << std::setprecision(1);
    titleText << "Detector Layout: " << result.fieldStrength << " T, Deflection "
              << result.deflectionAngle << "#circ";
    TLatex* title = new TLatex(0.5, 0.95, titleText.str().c_str());
    title->SetNDC();
    title->SetTextSize(0.04);
    title->SetTextAlign(22);
    title->Draw();
    
    std::ostringstream infoText;
    infoText << std::fixed << std::setprecision(1);
    infoText << "Target: (" << result.targetPos.position.X() << ", "
             << result.targetPos.position.Y() << ", "
             << result.targetPos.position.Z() << ") mm, Rot: "
             << result.targetPos.rotationAngle << "#circ";
    TLatex* info1 = new TLatex(0.12, 0.88, infoText.str().c_str());
    info1->SetNDC();
    info1->SetTextSize(0.022);
    info1->Draw();
    
    std::ostringstream infoText2;
    infoText2 << std::fixed << std::setprecision(1);
    infoText2 << "PDC: (" << result.pdcConfig.position.X() << ", "
              << result.pdcConfig.position.Y() << ", "
              << result.pdcConfig.position.Z() << ") mm, Rot: "
              << result.pdcConfig.rotationAngle << "#circ";
    TLatex* info2 = new TLatex(0.12, 0.85, infoText2.str().c_str());
    info2->SetNDC();
    info2->SetTextSize(0.022);
    info2->Draw();
    
    // [EN] Scale bar / [CN] 比例尺
    TLine* scaleLine = new TLine(-4500, -4500, -3500, -4500);
    scaleLine->SetLineWidth(3);
    scaleLine->Draw();
    TLatex* scaleLabel = new TLatex(-4000, -4700, "1000 mm");
    scaleLabel->SetTextSize(0.025);
    scaleLabel->SetTextAlign(22);
    scaleLabel->Draw();
    
    // [EN] Note with Px values / [CN] 带Px值的说明
    std::ostringstream noteText;
    noteText << "Tracks: Deuteron (green), Center proton P_{x}=0 (red), Edge protons P_{x}=#pm";
    for (size_t i = 0; i < trackConfig.pxEdgeValues.size(); i++) {
        if (i > 0) noteText << ",#pm";
        noteText << std::fixed << std::setprecision(0) << trackConfig.pxEdgeValues[i];
    }
    noteText << " MeV/c, P_{z}=" << std::fixed << std::setprecision(0) << trackConfig.pzProton << " MeV/c";
    TLatex* note = new TLatex(0.10, 0.03, noteText.str().c_str());
    note->SetNDC();
    note->SetTextSize(0.018);
    note->Draw();
    
    // [EN] Cleanup and save / [CN] 清理并保存
    if (trajCalc) delete trajCalc;
    
    canvas->Update();
    canvas->SaveAs(filename.c_str());
    std::string pdfFile = filename.substr(0, filename.find_last_of('.')) + ".pdf";
    canvas->SaveAs(pdfFile.c_str());
    
    SM_INFO("Single config plot with tracks saved to: {}", filename);
    delete canvas;
}
