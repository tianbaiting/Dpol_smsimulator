#include "GeoAcceptanceManager.hh"
#include "SMLogger.hh"
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
        result.fieldStrength = fieldStrength;
        result.fieldMapFile = fieldMapFile;
        result.deflectionAngle = angle;
        
        // 1. 计算target位置（从入射点追踪氘核轨迹）
        result.targetPos = fBeamCalc->CalculateTargetPosition(angle);
        
        // 2. 计算最佳PDC位置（使用target旋转角度）
        result.pdcConfig = fDetectorCalc->CalculateOptimalPDCPosition(
            result.targetPos.position, result.targetPos.rotationAngle);
        
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
    AnalysisConfig config;
    config.deflectionAngles = {0.0, 5.0, 10.0};
    config.qmdDataPath = "/home/tian/workspace/dpol/smsimulator5.5/data/qmdrawdata";
    config.outputPath = "./results";
    return config;
}

GeoAcceptanceManager::AnalysisConfig GeoAcceptanceManager::CreateConfig(
    const std::vector<std::string>& fieldMaps,
    const std::vector<double>& angles)
{
    AnalysisConfig config;
    config.fieldMapFiles = fieldMaps;
    config.deflectionAngles = angles;
    config.qmdDataPath = "/home/tian/workspace/dpol/smsimulator5.5/data/qmdrawdata";
    config.outputPath = "./results";
    
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
    TCanvas* canvas = new TCanvas("geoCanvas", "Detector Geometry Layout", 1600, 1000);
    canvas->SetGrid();
    canvas->SetMargin(0.08, 0.02, 0.10, 0.08);
    
    // 确定绘图范围
    double zMin = -4500;
    double zMax = 6000;
    double xMin = -4000;
    double xMax = 1500;
    
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
    TLegend* legend = new TLegend(0.65, 0.75, 0.98, 0.92);
    legend->SetBorderSize(1);
    legend->SetFillColor(kWhite);
    legend->SetTextSize(0.025);
    legend->SetHeader("Configurations", "C");
    
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
        
        // 2. 绘制PDC (矩形)
        // PDC尺寸: 约 1680 x 780 mm (宽 x 高)
        double pdcHalfWidth = result.pdcConfig.width / 2.0;
        double pdcHalfDepth = result.pdcConfig.depth / 2.0;
        // PDC 的旋转角度使用正角度，与 MagneticField 的约定一致
        double pdcAngleRad = result.pdcConfig.rotationAngle * TMath::Pi() / 180.0;
        double cosPDC = TMath::Cos(pdcAngleRad);
        double sinPDC = TMath::Sin(pdcAngleRad);
        
        // PDC四个角点（在局部坐标系中）
        double pdcLocalCorners[5][2] = {
            {-pdcHalfDepth, -pdcHalfWidth},
            { pdcHalfDepth, -pdcHalfWidth},
            { pdcHalfDepth,  pdcHalfWidth},
            {-pdcHalfDepth,  pdcHalfWidth},
            {-pdcHalfDepth, -pdcHalfWidth}  // 闭合
        };
        
        TGraph* pdcGraph = new TGraph(5);
        for (int i = 0; i < 5; i++) {
            // 旋转并平移到全局坐标 (使用与 MagneticField::RotateToLabFrame 相同的变换)
            double localZ = pdcLocalCorners[i][0];  // 磁铁坐标系 Z
            double localX = pdcLocalCorners[i][1];  // 磁铁坐标系 X
            // 从局部坐标到实验室坐标:
            // labX = cos(theta)*X_m - sin(theta)*Z_m
            // labZ = sin(theta)*X_m + cos(theta)*Z_m
            double labX = cosPDC * localX - sinPDC * localZ;
            double labZ = sinPDC * localX + cosPDC * localZ;
            double globalZ = result.pdcConfig.position.Z() + labZ;
            double globalX = result.pdcConfig.position.X() + labX;
            pdcGraph->SetPoint(i, globalZ, globalX);
        }
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
    TLatex* note1 = new TLatex(0.10, 0.03, "T = Target, PDC = Proton Drift Chamber, NEBULA = Neutron Detector");
    note1->SetNDC();
    note1->SetTextSize(0.025);
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
