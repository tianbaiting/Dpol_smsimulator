/**
 * @file test_rotation_consistency.cc
 * @brief 测试坐标旋转一致性
 * 
 * 专门用于测试：
 * 1. MagneticField 中的坐标变换
 * 2. DetectorAcceptanceCalculator 中的动量变换
 * 3. GeoAcceptanceManager 中的绘图变换
 * 
 * 三者是否使用一致的旋转约定
 */

#include <iostream>
#include <iomanip>
#include <cmath>

#include "TVector3.h"
#include "TMath.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TArrow.h"
#include "TLatex.h"
#include "TH2F.h"
#include "TStyle.h"
#include "TROOT.h"

// ============================================================================
// 旋转变换函数 - 模拟不同模块的实现
// ============================================================================

/**
 * MagneticField 中的变换 (从 MagneticField.cc)
 * 
 * SetRotationAngle: angleRad = angle * PI/180 (正角度)
 * 
 * RotateToMagnetFrame (Lab -> Magnet):
 *   x_m = x_lab * cos(θ) + z_lab * sin(θ)
 *   z_m = -x_lab * sin(θ) + z_lab * cos(θ)
 * 
 * RotateToLabFrame (Magnet -> Lab):
 *   x_lab = x_m * cos(θ) - z_m * sin(θ)
 *   z_lab = x_m * sin(θ) + z_m * cos(θ)
 * 
 * 物理意义：磁铁+Z方向在实验室中偏向-X方向
 */
TVector3 MagneticField_ToMagnetFrame(const TVector3& labPos, double angleDeg) {
    double angleRad = angleDeg * TMath::DegToRad();  // 正角度
    double cosTheta = TMath::Cos(angleRad);
    double sinTheta = TMath::Sin(angleRad);
    
    double x = labPos.X() * cosTheta + labPos.Z() * sinTheta;
    double y = labPos.Y();
    double z = -labPos.X() * sinTheta + labPos.Z() * cosTheta;
    
    return TVector3(x, y, z);
}

TVector3 MagneticField_ToLabFrame(const TVector3& magnetPos, double angleDeg) {
    double angleRad = angleDeg * TMath::DegToRad();  // 正角度
    double cosTheta = TMath::Cos(angleRad);
    double sinTheta = TMath::Sin(angleRad);
    
    double x = magnetPos.X() * cosTheta - magnetPos.Z() * sinTheta;
    double y = magnetPos.Y();
    double z = magnetPos.X() * sinTheta + magnetPos.Z() * cosTheta;
    
    return TVector3(x, y, z);
}

/**
 * DetectorAcceptanceCalculator 中的动量变换 (从 DetectorAcceptanceCalculator.cc)
 * 
 * CalculateOptimalPDCPosition:
 *   theta_rad = targetRotationAngle * DegToRad()  // 正角度！
 * 
 * 局部 -> 实验室:
 *   px_lab = px_local*cos(θ) + pz_local*sin(θ)
 *   pz_lab = -px_local*sin(θ) + pz_local*cos(θ)
 */
TVector3 DetectorCalc_LocalToLab(const TVector3& localMom, double angleDeg) {
    double theta_rad = angleDeg * TMath::DegToRad();  // 正角度！
    double cos_theta = TMath::Cos(theta_rad);
    double sin_theta = TMath::Sin(theta_rad);
    
    double px_lab = localMom.X() * cos_theta + localMom.Z() * sin_theta;
    double py_lab = localMom.Y();
    double pz_lab = -localMom.X() * sin_theta + localMom.Z() * cos_theta;
    
    return TVector3(px_lab, py_lab, pz_lab);
}

/**
 * GeoAcceptanceManager 画图中的变换 (从 GeoAcceptanceManager.cc)
 * 
 * magnetAngle = magnetAngleDeg * PI/180  // 正角度
 * 
 * 磁铁坐标 -> 实验室坐标 (画图用):
 *   labX = cos(θ)*X_m - sin(θ)*Z_m
 *   labZ = sin(θ)*X_m + cos(θ)*Z_m
 */
void GeoManager_MagnetToLab(double Xm, double Zm, double angleDeg, 
                            double& labX, double& labZ) {
    double magnetAngle = angleDeg * TMath::DegToRad();  // 正角度
    double cosTheta = TMath::Cos(magnetAngle);
    double sinTheta = TMath::Sin(magnetAngle);
    
    labX = cosTheta * Xm - sinTheta * Zm;
    labZ = sinTheta * Xm + cosTheta * Zm;
}

// ============================================================================
// 测试函数
// ============================================================================

void PrintVector(const std::string& name, const TVector3& v) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  " << std::setw(25) << name << ": ("
              << std::setw(10) << v.X() << ", "
              << std::setw(10) << v.Y() << ", "
              << std::setw(10) << v.Z() << ")\n";
}

void TestRotationConsistency(double angleDeg) {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  旋转一致性测试 (角度 = " << angleDeg << "°)\n";
    std::cout << "================================================================\n";
    
    // 预期结果说明
    std::cout << "\n=== 物理情境 ===\n";
    std::cout << "磁铁相对于实验室坐标系绕Y轴旋转 " << angleDeg << "° (假设顺时针从上看)\n";
    std::cout << "- 实验室坐标系：Z轴沿束流方向，X轴向左\n";
    std::cout << "- 磁铁坐标系：磁场数据定义的坐标系\n\n";
    
    // 测试1：实验室 -> 磁铁坐标系
    std::cout << "=== 测试1: 实验室 -> 磁铁坐标系 (MagneticField) ===\n";
    TVector3 labZ(0, 0, 1000);  // 实验室Z轴方向的点
    TVector3 labX(1000, 0, 0);  // 实验室X轴方向的点
    
    TVector3 magnetZ = MagneticField_ToMagnetFrame(labZ, angleDeg);
    TVector3 magnetX = MagneticField_ToMagnetFrame(labX, angleDeg);
    
    PrintVector("Lab (0,0,1000)", labZ);
    PrintVector("-> Magnet", magnetZ);
    PrintVector("Lab (1000,0,0)", labX);
    PrintVector("-> Magnet", magnetX);
    
    // 测试2：磁铁 -> 实验室坐标系
    std::cout << "\n=== 测试2: 磁铁 -> 实验室坐标系 (MagneticField) ===\n";
    TVector3 magZ(0, 0, 1000);  // 磁铁Z轴方向的点
    TVector3 magX(1000, 0, 0);  // 磁铁X轴方向的点
    
    TVector3 labFromMagZ = MagneticField_ToLabFrame(magZ, angleDeg);
    TVector3 labFromMagX = MagneticField_ToLabFrame(magX, angleDeg);
    
    PrintVector("Magnet (0,0,1000)", magZ);
    PrintVector("-> Lab", labFromMagZ);
    PrintVector("Magnet (1000,0,0)", magX);
    PrintVector("-> Lab", labFromMagX);
    
    // 测试3：动量变换 (DetectorAcceptanceCalculator)
    std::cout << "\n=== 测试3: 动量变换 (DetectorAcceptanceCalculator) ===\n";
    std::cout << "目标：将target局部坐标系的动量转换到实验室坐标系\n";
    std::cout << "target局部坐标系：Z轴沿束流方向（在该target处）\n\n";
    
    TVector3 localPz(0, 0, 600);    // 局部Z方向的动量
    TVector3 localPx(100, 0, 0);    // 局部X方向的动量
    TVector3 localPxPz(100, 0, 600); // 典型动量
    
    TVector3 labPz = DetectorCalc_LocalToLab(localPz, angleDeg);
    TVector3 labPx = DetectorCalc_LocalToLab(localPx, angleDeg);
    TVector3 labPxPz = DetectorCalc_LocalToLab(localPxPz, angleDeg);
    
    PrintVector("Local (0,0,600)", localPz);
    PrintVector("-> Lab", labPz);
    PrintVector("Local (100,0,0)", localPx);
    PrintVector("-> Lab", labPx);
    PrintVector("Local (100,0,600)", localPxPz);
    PrintVector("-> Lab", labPxPz);
    
    // 测试4：画图变换 (GeoAcceptanceManager)
    std::cout << "\n=== 测试4: 画图变换 (GeoAcceptanceManager) ===\n";
    double labX_plot, labZ_plot;
    
    GeoManager_MagnetToLab(0, 1000, angleDeg, labX_plot, labZ_plot);
    std::cout << "  Magnet (0, 1000) -> Lab: (" << labX_plot << ", " << labZ_plot << ")\n";
    
    GeoManager_MagnetToLab(1000, 0, angleDeg, labX_plot, labZ_plot);
    std::cout << "  Magnet (1000, 0) -> Lab: (" << labX_plot << ", " << labZ_plot << ")\n";
    
    // ============================================================
    // 一致性检查
    // ============================================================
    std::cout << "\n=== 一致性检查 ===\n";
    
    // 检查1: MagneticField的往返变换
    TVector3 testPoint(123, 456, 789);
    TVector3 inMagnet = MagneticField_ToMagnetFrame(testPoint, angleDeg);
    TVector3 backToLab = MagneticField_ToLabFrame(inMagnet, angleDeg);
    TVector3 diff1 = backToLab - testPoint;
    
    std::cout << "MagneticField往返变换:\n";
    PrintVector("原始点", testPoint);
    PrintVector("Lab->Magnet->Lab", backToLab);
    std::cout << "  误差: " << diff1.Mag() << " mm ";
    std::cout << (diff1.Mag() < 0.001 ? "[OK]" : "[ERROR!]") << "\n";
    
    // 检查2: 画图变换与MagneticField的一致性
    std::cout << "\n画图变换与MagneticField的Magnet->Lab一致性:\n";
    
    TVector3 testMag(500, 0, 800);
    TVector3 labFromMag = MagneticField_ToLabFrame(testMag, angleDeg);
    double plotX, plotZ;
    GeoManager_MagnetToLab(testMag.X(), testMag.Z(), angleDeg, plotX, plotZ);
    
    std::cout << "  MagneticField结果: (" << labFromMag.X() << ", " << labFromMag.Z() << ")\n";
    std::cout << "  GeoManager结果: (" << plotX << ", " << plotZ << ")\n";
    
    double diffPlot = std::sqrt((labFromMag.X()-plotX)*(labFromMag.X()-plotX) + 
                                (labFromMag.Z()-plotZ)*(labFromMag.Z()-plotZ));
    std::cout << "  误差: " << diffPlot << " mm ";
    std::cout << (diffPlot < 0.001 ? "[OK]" : "[ERROR!]") << "\n";
    
    // 检查3: DetectorCalc的动量变换与预期
    std::cout << "\nDetectorCalc动量变换检查:\n";
    std::cout << "如果target旋转角为θ，则局部Z方向动量应该变换为：\n";
    std::cout << "  px_lab = pz_local * sin(θ)\n";
    std::cout << "  pz_lab = pz_local * cos(θ)\n";
    
    double expectedPx = 600 * TMath::Sin(angleDeg * TMath::DegToRad());
    double expectedPz = 600 * TMath::Cos(angleDeg * TMath::DegToRad());
    
    std::cout << "  预期: (" << expectedPx << ", 0, " << expectedPz << ")\n";
    std::cout << "  实际: (" << labPz.X() << ", " << labPz.Y() << ", " << labPz.Z() << ")\n";
    
    double diffMom = std::sqrt((labPz.X()-expectedPx)*(labPz.X()-expectedPx) + 
                               (labPz.Z()-expectedPz)*(labPz.Z()-expectedPz));
    std::cout << "  误差: " << diffMom << " MeV/c ";
    std::cout << (diffMom < 0.001 ? "[OK]" : "[ERROR!]") << "\n";
    
    // ============================================================
    // 关键问题分析
    // ============================================================
    std::cout << "\n=== 关键问题分析 ===\n";
    
    // 比较 MagneticField 和 DetectorCalc 的变换方向
    std::cout << "比较两种变换对相同输入的结果:\n";
    std::cout << "(假设局部坐标系 = 磁铁坐标系的变体)\n\n";
    
    TVector3 inputVec(100, 0, 600);  // 作为位置或动量
    
    // MagneticField: Magnet -> Lab
    TVector3 mf_result = MagneticField_ToLabFrame(inputVec, angleDeg);
    // DetectorCalc: Local -> Lab  
    TVector3 dc_result = DetectorCalc_LocalToLab(inputVec, angleDeg);
    
    std::cout << "输入向量: (" << inputVec.X() << ", " << inputVec.Y() << ", " << inputVec.Z() << ")\n";
    PrintVector("MagneticField Magnet->Lab", mf_result);
    PrintVector("DetectorCalc Local->Lab", dc_result);
    
    TVector3 diffVec = mf_result - dc_result;
    std::cout << "\n差异: (" << diffVec.X() << ", " << diffVec.Y() << ", " << diffVec.Z() << ")\n";
    
    if (diffVec.Mag() > 0.001) {
        std::cout << "\n*** 警告: MagneticField 和 DetectorCalc 的变换不一致! ***\n";
        std::cout << "\n原因分析:\n";
        std::cout << "  两者应该使用相同的正角度约定\n";
        std::cout << "\n这导致两者的旋转方向相反!\n";
        
        // 验证：如果使用相反符号的角度
        TVector3 dc_corrected = DetectorCalc_LocalToLab(inputVec, -angleDeg);
        PrintVector("\nDetectorCalc使用-angle", dc_corrected);
        
        TVector3 correctedDiff = mf_result - dc_corrected;
        std::cout << "与MagneticField的差异: " << correctedDiff.Mag() << " ";
        std::cout << (correctedDiff.Mag() < 0.001 ? "[现在一致!]" : "[仍不一致]") << "\n";
    } else {
        std::cout << "\n两种变换一致 [OK]\n";
        std::cout << "\n=== 物理验证 ===\n";
        std::cout << "磁铁旋转" << angleDeg << "°时:\n";
        std::cout << "  磁铁+Z方向 (0,0,1000) 在实验室中应该偏向-X\n";
        TVector3 magnetZ(0, 0, 1000);
        TVector3 labZ = MagneticField_ToLabFrame(magnetZ, angleDeg);
        std::cout << "  实际结果: (" << labZ.X() << ", " << labZ.Y() << ", " << labZ.Z() << ")\n";
        if (labZ.X() < 0) {
            std::cout << "  验证通过: X分量为负 [OK]\n";
        } else {
            std::cout << "  验证失败: X分量应为负 [ERROR]\n";
        }
    }
}

void DrawRotationDiagram(double angleDeg, const std::string& outputFile) {
    std::cout << "\n生成旋转变换示意图: " << outputFile << "\n";
    
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    
    TCanvas* canvas = new TCanvas("rotCanvas", "Rotation Transform Diagram", 1000, 1000);
    canvas->SetMargin(0.12, 0.05, 0.12, 0.08);
    
    double range = 1500;
    TH2F* frame = new TH2F("frame", "", 100, -range, range, 100, -range, range);
    frame->GetXaxis()->SetTitle("Z (or Pz) direction");
    frame->GetYaxis()->SetTitle("X (or Px) direction");
    frame->GetXaxis()->SetTitleSize(0.04);
    frame->GetYaxis()->SetTitleSize(0.04);
    frame->Draw();
    
    // 绘制实验室坐标系轴
    TArrow* labZ = new TArrow(0, 0, 1200, 0, 0.02, "|>");
    labZ->SetLineColor(kBlack);
    labZ->SetLineWidth(2);
    labZ->Draw();
    
    TArrow* labX = new TArrow(0, 0, 0, 1200, 0.02, "|>");
    labX->SetLineColor(kBlack);
    labX->SetLineWidth(2);
    labX->Draw();
    
    TLatex* labZLabel = new TLatex(1250, 50, "Z_{lab}");
    labZLabel->SetTextSize(0.03);
    labZLabel->Draw();
    
    TLatex* labXLabel = new TLatex(50, 1250, "X_{lab}");
    labXLabel->SetTextSize(0.03);
    labXLabel->Draw();
    
    // 计算旋转后的坐标轴（使用MagneticField的变换）
    TVector3 magZ(0, 0, 1000);
    TVector3 magX(1000, 0, 0);
    TVector3 magZ_lab = MagneticField_ToLabFrame(magZ, angleDeg);
    TVector3 magX_lab = MagneticField_ToLabFrame(magX, angleDeg);
    
    // 绘制磁铁坐标系轴
    TArrow* magZArrow = new TArrow(0, 0, magZ_lab.Z(), magZ_lab.X(), 0.02, "|>");
    magZArrow->SetLineColor(kBlue);
    magZArrow->SetLineWidth(2);
    magZArrow->Draw();
    
    TArrow* magXArrow = new TArrow(0, 0, magX_lab.Z(), magX_lab.X(), 0.02, "|>");
    magXArrow->SetLineColor(kBlue);
    magXArrow->SetLineWidth(2);
    magXArrow->Draw();
    
    TLatex* magZLabel = new TLatex(magZ_lab.Z() * 1.15, magZ_lab.X() * 1.15 + 50, "Z_{mag}");
    magZLabel->SetTextSize(0.03);
    magZLabel->SetTextColor(kBlue);
    magZLabel->Draw();
    
    TLatex* magXLabel = new TLatex(magX_lab.Z() * 1.15 + 50, magX_lab.X() * 1.15, "X_{mag}");
    magXLabel->SetTextSize(0.03);
    magXLabel->SetTextColor(kBlue);
    magXLabel->Draw();
    
    // 绘制DetectorCalc的变换结果（用不同颜色）
    TVector3 localZ(0, 0, 1000);
    TVector3 localX(1000, 0, 0);
    TVector3 localZ_lab = DetectorCalc_LocalToLab(localZ, angleDeg);
    TVector3 localX_lab = DetectorCalc_LocalToLab(localX, angleDeg);
    
    TArrow* dcZArrow = new TArrow(0, 0, localZ_lab.Z(), localZ_lab.X(), 0.02, "|>");
    dcZArrow->SetLineColor(kRed);
    dcZArrow->SetLineWidth(2);
    dcZArrow->SetLineStyle(2);
    dcZArrow->Draw();
    
    TArrow* dcXArrow = new TArrow(0, 0, localX_lab.Z(), localX_lab.X(), 0.02, "|>");
    dcXArrow->SetLineColor(kRed);
    dcXArrow->SetLineWidth(2);
    dcXArrow->SetLineStyle(2);
    dcXArrow->Draw();
    
    TLatex* dcZLabel = new TLatex(localZ_lab.Z() * 1.15, localZ_lab.X() * 1.15 - 50, "Z_{DC}");
    dcZLabel->SetTextSize(0.03);
    dcZLabel->SetTextColor(kRed);
    dcZLabel->Draw();
    
    // 标题
    TLatex* title = new TLatex(0.5, 0.95, Form("Rotation Transform Comparison (angle = %.0f#circ)", angleDeg));
    title->SetNDC();
    title->SetTextSize(0.035);
    title->SetTextAlign(22);
    title->Draw();
    
    // 图例
    TLatex* leg1 = new TLatex(0.15, 0.88, "Black: Laboratory coordinate system");
    leg1->SetNDC();
    leg1->SetTextSize(0.025);
    leg1->Draw();
    
    TLatex* leg2 = new TLatex(0.15, 0.84, "Blue: MagneticField (Magnet->Lab, angle=-30#circ)");
    leg2->SetNDC();
    leg2->SetTextSize(0.025);
    leg2->SetTextColor(kBlue);
    leg2->Draw();
    
    TLatex* leg3 = new TLatex(0.15, 0.80, "Red (dashed): DetectorCalc (Local->Lab, angle=+30#circ)");
    leg3->SetNDC();
    leg3->SetTextSize(0.025);
    leg3->SetTextColor(kRed);
    leg3->Draw();
    
    canvas->Update();
    canvas->SaveAs(outputFile.c_str());
    
    std::string pdfFile = outputFile.substr(0, outputFile.find_last_of('.')) + ".pdf";
    canvas->SaveAs(pdfFile.c_str());
    
    delete canvas;
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║       Rotation Consistency Test Program                       ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    
    gROOT->SetBatch(true);
    
    // 测试不同角度
    std::vector<double> testAngles = {30.0, 0.0, 15.0, -15.0};
    
    for (double angle : testAngles) {
        TestRotationConsistency(angle);
    }
    
    // 生成示意图
    DrawRotationDiagram(30.0, "rotation_diagram.png");
    
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  测试完成\n";
    std::cout << "================================================================\n";
    
    return 0;
}
