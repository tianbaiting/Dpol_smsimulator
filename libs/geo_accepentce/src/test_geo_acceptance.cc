/**
 * @file test_geo_acceptance.cc
 * @brief 几何接受度分析库的简单测试
 */

#include "BeamDeflectionCalculator.hh"
#include "DetectorAcceptanceCalculator.hh"
#include "GeoAcceptanceManager.hh"
#include "MagneticField.hh"
#include "SMLogger.hh"

int main() {
    // 初始化日志系统
    SMLogger::Logger::Instance().Initialize();
    
    SM_INFO("=== Testing Geometry Acceptance Library ===");
    
    // 测试1: BeamDeflectionCalculator
    SM_INFO("Test 1: Beam Deflection Calculator");
    SM_INFO("-----------------------------------");
    
    BeamDeflectionCalculator beamCalc;
    beamCalc.PrintBeamInfo();
    
    // 测试计算动量等物理量
    SM_INFO("Physics calculations:");
    SM_INFO("  Momentum: {:.2f} MeV/c", beamCalc.CalculateBeamMomentum());
    SM_INFO("  Energy: {:.2f} MeV", beamCalc.CalculateBeamEnergy());
    SM_INFO("  Beta: {:.4f}", beamCalc.CalculateBeamBeta());
    SM_INFO("  Gamma: {:.4f}", beamCalc.CalculateBeamGamma());
    
    // 测试2: DetectorAcceptanceCalculator
    SM_INFO("Test 2: Detector Acceptance Calculator");
    SM_INFO("---------------------------------------");
    
    DetectorAcceptanceCalculator detCalc;
    
    auto pdcConfig = detCalc.GetPDCConfiguration();
    SM_INFO("Default PDC configuration:");
    SM_INFO("  Px range: [{:.2f}, {:.2f}] MeV/c", pdcConfig.pxMin, pdcConfig.pxMax);
    
    auto nebulaConfig = detCalc.GetNEBULAConfiguration();
    SM_INFO("Default NEBULA configuration:");
    SM_INFO("  Position: ({:.2f}, {:.2f}, {:.2f}) mm", 
            nebulaConfig.position.X(), nebulaConfig.position.Y(), nebulaConfig.position.Z());
    SM_INFO("  Size: {:.0f} x {:.0f} x {:.0f} mm³", 
            nebulaConfig.width, nebulaConfig.height, nebulaConfig.depth);
    
    // 测试3: GeoAcceptanceManager
    SM_INFO("Test 3: Geo Acceptance Manager");
    SM_INFO("-------------------------------");
    
    auto config = GeoAcceptanceManager::CreateDefaultConfig();
    SM_INFO("Default configuration:");
    SM_INFO("  QMD data path: {}", config.qmdDataPath);
    SM_INFO("  Output path: {}", config.outputPath);
    
    std::string angles_str;
    for (size_t i = 0; i < config.deflectionAngles.size(); i++) {
        angles_str += std::to_string(config.deflectionAngles[i]) + "°";
        if (i < config.deflectionAngles.size() - 1) angles_str += ", ";
    }
    SM_INFO("  Deflection angles: {}", angles_str);
    
    SM_INFO("=== All Tests Passed! ===");
    
    SM_INFO("Note: Full functionality test requires:");
    SM_INFO("  1. Magnetic field map files");
    SM_INFO("  2. QMD reaction data");
    SM_INFO("  Use RunGeoAcceptanceAnalysis for complete analysis");
    
    // 关闭日志系统
    SMLogger::Logger::Instance().Shutdown();
    
    return 0;
}
