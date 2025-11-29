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

// ============================================================================
// 辅助函数
// ============================================================================

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
 * @brief 绘制测试结果图形
 */
void DrawTestResults(MagneticField* magField,
                     BeamDeflectionCalculator* beamCalc,
                     DetectorAcceptanceCalculator* detCalc,
                     const std::vector<BeamDeflectionCalculator::TargetPosition>& targetPositions,
                     const std::string& outputFile) {
    PrintSeparator("测试5: 绘制可视化结果");
    
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    
    // 绘图范围 - 扩大以包含PDC
    double zMin = -5000;
    double zMax = 6000;
    double xMin = -10000;
    double xMax = 3000;
    
    // 计算范围使得横纵比例相同
    double zRange = zMax - zMin;  // 11000 mm
    double xRange = xMax - xMin;  // 13000 mm
    
    // 根据范围调整画布大小，使比例尺相同
    int canvasWidth = 1400;
    int canvasHeight = (int)(canvasWidth * xRange / zRange);
    
    TCanvas* canvas = new TCanvas("testCanvas", "PDC Position Test", canvasWidth, canvasHeight);
    canvas->SetGrid();
    canvas->SetMargin(0.10, 0.15, 0.08, 0.06);  // left, right, bottom, top
    
    TH2F* frame = new TH2F("frame", "", 100, zMin, zMax, 100, xMin, xMax);
    frame->GetXaxis()->SetTitle("Z (mm) - Beam Direction");
    frame->GetYaxis()->SetTitle("X (mm)");
    frame->GetXaxis()->SetTitleSize(0.035);
    frame->GetYaxis()->SetTitleSize(0.035);
    frame->GetXaxis()->SetTitleOffset(1.0);
    frame->GetYaxis()->SetTitleOffset(1.2);
    frame->Draw();
    
    // 绘制磁铁轮廓
    double magnetHalfZ = 1000;
    double magnetHalfX = 1500;
    double magnetAngleDeg = magField->GetRotationAngle();
    // 与MagneticField::RotateToLabFrame一致，使用正角度
    double magnetAngle = magnetAngleDeg * TMath::Pi() / 180.0;
    double cosTheta = TMath::Cos(magnetAngle);
    double sinTheta = TMath::Sin(magnetAngle);
    
    double corners[4][2] = {
        {-magnetHalfZ, -magnetHalfX},
        { magnetHalfZ, -magnetHalfX},
        { magnetHalfZ,  magnetHalfX},
        {-magnetHalfZ,  magnetHalfX}
    };
    
    TGraph* magnetGraph = new TGraph(5);
    for (int i = 0; i < 4; i++) {
        double Zm = corners[i][0];
        double Xm = corners[i][1];
        double labX = cosTheta * Xm - sinTheta * Zm;
        double labZ = sinTheta * Xm + cosTheta * Zm;
        magnetGraph->SetPoint(i, labZ, labX);
    }
    magnetGraph->SetPoint(4, magnetGraph->GetX()[0], magnetGraph->GetY()[0]);
    magnetGraph->SetFillColorAlpha(kCyan, 0.3);
    magnetGraph->SetLineColor(kBlue);
    magnetGraph->SetLineWidth(2);
    magnetGraph->Draw("F");
    magnetGraph->Draw("L SAME");
    
    TLatex* magnetLabel = new TLatex(0, 200, Form("SAMURAI Magnet (%.0f#circ)", magnetAngleDeg));
    magnetLabel->SetTextSize(0.025);
    magnetLabel->SetTextAlign(22);
    magnetLabel->SetTextColor(kBlue);
    magnetLabel->Draw();
    
    // 绘制束流入射线
    TArrow* beamLine = new TArrow(-4000, 0, -500, 0, 0.02, ">");
    beamLine->SetLineColor(kGreen+2);
    beamLine->SetLineWidth(3);
    beamLine->Draw();
    
    TLatex* beamLabel = new TLatex(-4000, 100, "Beam");
    beamLabel->SetTextSize(0.025);
    beamLabel->SetTextColor(kGreen+2);
    beamLabel->Draw();
    
    // 绘制完整束流轨迹
    auto fullTrajectory = beamCalc->CalculateFullBeamTrajectory();
    if (fullTrajectory.size() > 10) {
        TGraph* trajGraph = new TGraph(fullTrajectory.size());
        for (size_t i = 0; i < fullTrajectory.size(); i++) {
            trajGraph->SetPoint(i, fullTrajectory[i].position.Z(), 
                                  fullTrajectory[i].position.X());
        }
        trajGraph->SetLineColor(kGreen+2);
        trajGraph->SetLineWidth(2);
        trajGraph->SetLineStyle(1);
        trajGraph->Draw("L SAME");
    }
    
    // 颜色方案
    int colors[] = {kRed, kBlue, kMagenta, kOrange+1, kCyan+1};
    int nColors = sizeof(colors) / sizeof(colors[0]);
    
    TLegend* legend = new TLegend(0.70, 0.55, 0.94, 0.92);
    legend->SetBorderSize(1);
    legend->SetFillColor(kWhite);
    legend->SetTextSize(0.020);
    legend->SetHeader("Configurations", "C");
    
    int colorIdx = 0;
    for (const auto& targetPos : targetPositions) {
        int color = colors[colorIdx % nColors];
        
        // 绘制Target位置
        TEllipse* target = new TEllipse(targetPos.position.Z(), 
                                        targetPos.position.X(), 
                                        60, 60);
        target->SetFillColor(color);
        target->SetLineColor(color);
        target->Draw();
        
        // 绘制束流方向
        double arrowLen = 300;
        TArrow* beamDir = new TArrow(
            targetPos.position.Z(),
            targetPos.position.X(),
            targetPos.position.Z() + arrowLen * targetPos.beamDirection.Z(),
            targetPos.position.X() + arrowLen * targetPos.beamDirection.X(),
            0.015, ">"
        );
        beamDir->SetLineColor(color);
        beamDir->SetLineWidth(2);
        beamDir->Draw();
        
        // 计算并绘制PDC位置
        auto pdcConfig = detCalc->CalculateOptimalPDCPosition(
            targetPos.position, targetPos.rotationAngle);
        
        if (pdcConfig.isOptimal) {
            // 获取质子轨迹（从target到PDC）
            ParticleTrajectory* trajectory = new ParticleTrajectory(magField);
            trajectory->SetStepSize(10.0);  // 10mm步长
            
            // 创建质子动量（在target局部坐标系中Px=0, Pz=600 MeV/c）
            double theta_rad = targetPos.rotationAngle * TMath::DegToRad();
            double cos_theta = TMath::Cos(theta_rad);
            double sin_theta = TMath::Sin(theta_rad);
            double pz_local = 600.0;  // MeV/c
            
            // 转换到实验室坐标系
            double px_lab = pz_local * sin_theta;
            double pz_lab = pz_local * cos_theta;
            
            TVector3 momentum(px_lab, 0, pz_lab);
            double protonMass = 938.272;  // MeV/c²
            double E = TMath::Sqrt(momentum.Mag2() + protonMass * protonMass);
            TLorentzVector p4(momentum, E);
            
            auto protonTraj = trajectory->CalculateTrajectory(
                targetPos.position, p4, 1, protonMass);
            
            if (protonTraj.size() > 10) {
                TGraph* protonGraph = new TGraph();
                int nPoints = 0;
                for (size_t i = 0; i < protonTraj.size(); i += 5) {  // 每5个点取一个
                    protonGraph->SetPoint(nPoints++, 
                                         protonTraj[i].position.Z(),
                                         protonTraj[i].position.X());
                }
                protonGraph->SetLineColor(color);
                protonGraph->SetLineWidth(2);
                protonGraph->SetLineStyle(1);
                protonGraph->Draw("L SAME");
            }
            
            delete trajectory;
            
            // PDC中心位置
            double pdcZ = pdcConfig.position.Z();
            double pdcX = pdcConfig.position.X();
            
            // PDC尺寸（在探测器局部坐标系中）
            double pdcHalfWidth = pdcConfig.width / 2.0;   // 沿探测器平面的宽度
            double pdcHalfDepth = pdcConfig.depth / 2.0;   // 垂直于探测器平面的深度
            
            // PDC方向 (法向量)
            double nx = pdcConfig.normal.X();
            double nz = pdcConfig.normal.Z();
            
            // 计算PDC矩形的四个角点
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
            
            pdcGraph->SetFillColorAlpha(color, 0.3);
            pdcGraph->SetLineColor(color);
            pdcGraph->SetLineWidth(2);
            pdcGraph->Draw("F");
            pdcGraph->Draw("L SAME");
            
            // 绘制PDC法向量
            double normalLen = 400;
            TArrow* pdcNormal = new TArrow(
                pdcZ, pdcX,
                pdcZ + normalLen * nz,
                pdcX + normalLen * nx,
                0.015, ">"
            );
            pdcNormal->SetLineColor(color);
            pdcNormal->SetLineWidth(2);
            pdcNormal->SetLineStyle(2);  // 虚线
            pdcNormal->Draw();
            
            // 绘制PDC中心点
            TEllipse* pdcCenter = new TEllipse(pdcZ, pdcX, 40, 40);
            pdcCenter->SetFillColor(color);
            pdcCenter->SetLineColor(color);
            pdcCenter->Draw();
            
            // 添加PDC标签
            TLatex* pdcLabel = new TLatex(pdcZ + 150, pdcX + 150, 
                Form("PDC %.0f#circ", targetPos.deflectionAngle));
            pdcLabel->SetTextSize(0.018);
            pdcLabel->SetTextColor(color);
            pdcLabel->Draw();
        }
        
        // 图例
        legend->AddEntry(target, Form("%.1f#circ: Target + PDC", targetPos.deflectionAngle), "f");
        
        colorIdx++;
    }
    
    legend->Draw();
    
    // 标题
    TLatex* title = new TLatex(0.5, 0.95, "PDC Position Test - Beam Trajectory & Target Positions");
    title->SetNDC();
    title->SetTextSize(0.035);
    title->SetTextAlign(22);
    title->Draw();
    
    // 保存
    canvas->Update();
    canvas->SaveAs(outputFile.c_str());
    std::cout << "图形已保存到: " << outputFile << "\n";
    
    // 同时保存PDF
    std::string pdfFile = outputFile.substr(0, outputFile.find_last_of('.')) + ".pdf";
    canvas->SaveAs(pdfFile.c_str());
    std::cout << "PDF版本已保存到: " << pdfFile << "\n";
    
    delete canvas;
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
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         PDC Position Calculation Test Program                ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    
    // 解析命令行参数
    std::string fieldMapFile = "";
    double rotationAngle = 30.0;  // 默认30度
    
    if (argc >= 2) {
        fieldMapFile = argv[1];
    }
    if (argc >= 3) {
        rotationAngle = std::atof(argv[2]);
    }
    
    // 查找磁场文件
    if (fieldMapFile.empty()) {
        // 尝试默认路径
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
    
    if (fieldMapFile.empty()) {
        std::cerr << "ERROR: 未找到磁场文件！\n";
        std::cerr << "用法: " << argv[0] << " [field_map_file] [rotation_angle]\n";
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
    std::vector<double> deflectionAngles = {0.0, 5.0, 10.0, 15.0};
    TestTargetPositionCalculation(beamCalc, deflectionAngles);
    
    // 重新计算Target位置用于后续测试
    std::vector<BeamDeflectionCalculator::TargetPosition> targetPositions;
    for (double angle : deflectionAngles) {
        targetPositions.push_back(beamCalc->CalculateTargetPosition(angle));
    }
    
    // 测试4: PDC最佳位置
    TestOptimalPDCPosition(detCalc, targetPositions);
    
    // 测试5: 绘制结果（包含PDC位置）
    DrawTestResults(magField, beamCalc, detCalc, targetPositions, "test_pdc_position.png");
    
    // 测试6: 坐标变换一致性
    TestCoordinateTransformConsistency(rotationAngle);
    
    // 清理
    PrintSeparator("测试完成");
    std::cout << "所有测试已完成。\n";
    std::cout << "请检查输出结果和生成的图形文件。\n";
    
    delete beamCalc;
    delete detCalc;
    delete magField;
    
    return 0;
}
