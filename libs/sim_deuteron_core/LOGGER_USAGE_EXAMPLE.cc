// 示例：如何在 sim_deuteron_core 中使用 SMLogger

#include "DeutDetectorConstruction.hh"
#include "SMLogger.hh"  // 添加日志头文件

DeutDetectorConstruction::DeutDetectorConstruction() 
{
    // 之前使用 G4cout
    // G4cout << "Constructor of DeutDetectorConstruction"  << G4endl;
    
    // 现在使用 SMLogger
    SM_INFO("DeutDetectorConstruction initialized");
    SM_DEBUG("Configuration: FillAir={}, SetTarget={}, SetDump={}", 
             fFillAir, fSetTarget, fSetDump);
}

void DeutDetectorConstruction::SetTargetPosition(G4ThreeVector pos)
{
    fTargetPos = pos;
    
    // 之前
    // std::cout << "DeutDetectorConstruction : Set target position at  "
    //           << fTargetPos/cm << " cm " << std::endl;
    
    // 现在
    SM_INFO("Set target position: ({:.2f}, {:.2f}, {:.2f}) cm",
            fTargetPos.x()/cm, fTargetPos.y()/cm, fTargetPos.z()/cm);
}

void DeutDetectorConstruction::Construct()
{
    SM_INFO("Constructing detector geometry...");
    
    // ... 构建代码 ...
    
    // 使用条件日志
    SM_WARN_IF(!fSetTarget, "Target is disabled in configuration");
    
    // 避免日志轰炸的例子
    for (int i = 0; i < numDetectors; i++) {
        // 每100个探测器输出一次进度
        SM_INFO_EVERY_N(100, "Placed {} detectors", i);
    }
    
    SM_INFO("Geometry construction completed");
}
