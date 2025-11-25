/**
 * @file run_event_display_realdata.C
 * @brief 真实数据事件3D显示脚本（带重建轨迹）
 * 
 * @author SAMURAI Polarization Analysis Group
 * @date 2025-11
 * 
 * ============================================================================
 * 功能说明
 * ============================================================================
 * 1. 读取真实数据ROOT文件
 * 2. PDC轨迹重建
 * 3. 质子在靶点的动量重建（TMinuit优化）
 * 4. EventDisplay 3D可视化
 * 5. 保存PNG图像和结果
 * 
 * ============================================================================
 * 使用方法
 * ============================================================================
 * 
 * 前提条件：
 *   1. 确保已设置环境：source setup.sh
 *   2. 库路径已配置：LD_LIBRARY_PATH 包含 build/lib
 * 
 * 方式1 - 直接运行（推荐）：
 *   root -l run_event_display_realdata.C
 * 
 * 方式2 - 指定事件：
 *   root -l 'run_event_display_realdata.C(5)'
 * 
 * 方式3 - 指定文件和事件：
 *   root -l 'run_event_display_realdata.C(0, "/path/to/data.root")'
 * 
 * 方式4 - 使用Shell脚本（推荐用于批量处理）：
 *   ./run_3d_display.sh 0
 * 
 * ============================================================================
 * 库加载说明
 * ============================================================================
 * 
 * 脚本会自动加载以下库（需要先 source setup.sh）：
 *   - libsmdata.so        : TBeamSimData等数据类
 *   - libsmlogger.so      : 日志系统（可选）
 *   - libanalysis.so      : 分析库（RecoHit, RecoTrack, EventDisplay等）
 * 
 * 如果遇到库加载失败：
 *   1. 确认已运行：source setup.sh
 *   2. 检查库路径：echo $LD_LIBRARY_PATH
 *   3. 确认库存在：ls -la $SMSIMDIR/build/lib/
 *   4. 重新编译：cd build && make -j4
 * 
 * ============================================================================
 * 输出说明
 * ============================================================================
 * 
 * 输出目录：test_output/reconstruction_realdata_event<N>/
 *   - event_display_3d.png     : 3D事件显示图像
 * 
 * 终端输出：
 *   - 重建轨迹数量
 *   - TMinuit优化过程
 *   - 重建动量和精度
 *   - 靶点距离
 * 
 * ============================================================================
 * 故障排查
 * ============================================================================
 * 
 * Q1: "环境变量 SMSIMDIR 未设置"
 * A1: 运行 source /path/to/smsimulator5.5/setup.sh
 * 
 * Q2: "Failed to load libsmdata.so"
 * A2: 检查 LD_LIBRARY_PATH，确保包含 build/lib
 * 
 * Q3: "无法打开数据文件"
 * A3: 检查文件路径是否正确，文件是否存在
 * 
 * Q4: "事件 XX 无PDC hits"
 * A4: 该事件无有效数据，尝试其他事件
 * 
 * Q5: "重建失败"
 * A5: 可能是优化未收敛，检查磁场配置和初始参数
 * 
 * ============================================================================
 * 技术细节
 * ============================================================================
 * 
 * 重建算法：TMinuit优化
 *   - 初始猜测：800 MeV/c
 *   - 容差：5.0 mm
 *   - 最大迭代：5000次
 * 
 * 磁场配置：
 *   - 文件：configs/simulation/geometry/filed_map/180626-1,20T-3000.root
 *   - 旋转角：30°（绕Y轴）
 * 
 * 相机视角：
 *   - 观察中心：(-1500, 0, 1500) mm
 *   - 旋转：(-0.3, 0.5) rad
 *   - 距离：2000 units
 */

#include "TSystem.h"
#include "TGLViewer.h"
#include "TGLCamera.h"
#include "TEveManager.h"
#include <iostream>

// 加载必要的库
void LoadLibraries() {
    // 获取环境变量
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir) {
        Error("LoadLibraries", "环境变量 SMSIMDIR 未设置!");
        Error("LoadLibraries", "请先运行: source setup.sh");
        return;
    }
    
    // 如果设置了 LD_LIBRARY_PATH（通过 setup.sh），可以直接用库名
    std::cout << "Loading libraries..." << std::endl;
    
    // 1. smdata库（包含 TBeamSimData 等数据类）- 必须先加载
    if (gSystem->Load("libsmdata") < 0) {
        Error("LoadLibraries", "Failed to load libsmdata.so");
        Error("LoadLibraries", "请确保已运行: source setup.sh");
        return;
    }
    std::cout << "  ✓ Loaded libsmdata.so" << std::endl;
    
    // 2. smlogger库（可选）
    if (gSystem->Load("libsmlogger") < 0) {
        Warning("LoadLibraries", "Failed to load libsmlogger.so (optional)");
    } else {
        std::cout << "  ✓ Loaded libsmlogger.so" << std::endl;
    }
    
    // 3. analysis库（包含RecoHit, RecoTrack等）
    // 注意：实际库名是 libpdcanalysis.so，但通过符号链接可以用 libanalysis
    if (gSystem->Load("libanalysis") < 0) {
        Error("LoadLibraries", "Failed to load libanalysis.so");
        return;
    }
    std::cout << "  ✓ Loaded libanalysis.so" << std::endl;
    
    std::cout << "All libraries loaded successfully!\n" << std::endl;
}

// 包含分析库的头文件（在加载库之后）
#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "EventDisplay.hh"
#include "RecoEvent.hh"
#include "MagneticField.hh"
#include "TargetReconstructor.hh"
#include "TBeamSimData.hh"

void run_event_display_realdata(Long64_t event_id = 0, 
                                const char* filename = "/home/tian/workspace/dpol/smsimulator5.5/data/simulation/output_tree/testry0000.root") {
    
    // 0. 加载必要的库
    LoadLibraries();
    
    // 1. 检查环境变量
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir) {
        Error("run_event_display_realdata", "环境变量 SMSIMDIR 未设置!");
        return;
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "真实数据3D事件显示（带重建轨迹）" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "数据文件: " << filename << std::endl;
    std::cout << "事件 ID: " << event_id << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // 2. 加载几何
    GeometryManager geo;
    geo.LoadGeometry(Form("%s/d_work/geometry/5deg_1.2T.mac", smsDir));
    TVector3 targetPos = geo.GetTargetPosition();
    
    Info("run_event_display_realdata", "Target位置: (%.2f, %.2f, %.2f) mm", 
         targetPos.X(), targetPos.Y(), targetPos.Z());
    
    // 3. 创建PDC分析器
    PDCSimAna ana(geo);
    ana.SetSmearing(0.5, 0.5);
    
    // 4. 打开数据文件
    EventDataReader reader(filename);
    if (!reader.IsOpen()) {
        Error("run_event_display_realdata", "无法打开数据文件: %s", filename);
        return;
    }
    
    if (!reader.GoToEvent(event_id)) {
        Error("run_event_display_realdata", "无法访问事件 %lld", event_id);
        return;
    }
    
    // 5. 读取入射粒子信息
    const std::vector<TBeamSimData>* beamData = reader.GetBeamData();
    if (beamData && !beamData->empty()) {
        const TBeamSimData& beam = (*beamData)[0];
        TVector3 trueMomVec = beam.fMomentum.Vect();
        double trueMom = trueMomVec.Mag();
        
        Info("run_event_display_realdata", "入射粒子: %s, |p|=%.1f MeV/c", 
             beam.fParticleName.Data(), trueMom);
    }
    
    // 6. PDC重建
    TClonesArray* hits = reader.GetHits();
    if (!hits || hits->GetEntries() == 0) {
        Error("run_event_display_realdata", "事件 %lld 无PDC hits", event_id);
        return;
    }
    
    RecoEvent event = ana.ProcessEvent(hits);
    event.eventID = event_id;
    
    Info("run_event_display_realdata", "PDC重建: %zu 个轨迹", event.tracks.size());
    
    if (event.tracks.empty()) {
        Error("run_event_display_realdata", "事件 %lld 无重建轨迹", event_id);
        return;
    }
    
    // 7. 加载磁场
    TString magFieldFile = Form("%s/configs/simulation/geometry/filed_map/180626-1,20T-3000.root", smsDir);
    MagneticField* magField = new MagneticField();
    if (!magField->LoadFromROOTFile(magFieldFile.Data(), "MagField")) {
        Error("run_event_display_realdata", "无法加载磁场文件");
        delete magField;
        return;
    }
    magField->SetRotationAngle(30.0);
    Info("run_event_display_realdata", "磁场加载成功（旋转30°）");
    
    // 8. 靶点重建
    TargetReconstructor targetRecon(magField);
    const RecoTrack& track = event.tracks[0];
    
    Info("run_event_display_realdata", "运行靶点重建...");
    TargetReconstructionResult result = targetRecon.ReconstructAtTargetMinuit(
        track, targetPos,
        true,    // saveTrajectories
        800.0,   // 初始动量猜测
        5.0,     // 容差
        5000,    // 最大迭代次数
        false    // recordSteps
    );
    
    if (result.success) {
        double recoP = result.bestMomentum.P();
        Info("run_event_display_realdata", "重建成功: |p|=%.1f MeV/c, 距离=%.2f mm", 
             recoP, result.finalDistance);
    } else {
        Warning("run_event_display_realdata", "重建失败");
    }
    
    // 9. 创建EventDisplay并显示
    Info("run_event_display_realdata", "初始化3D显示...");
    TString geomFile = Form("%s/detector_geometry.gdml", smsDir);
    EventDisplay* display = new EventDisplay(geomFile.Data(), geo);
    
    // 显示事件和入射粒子轨迹
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
    
    // 绘制重建轨迹
    if (!result.bestTrajectory.empty()) {
        Info("run_event_display_realdata", "绘制重建轨迹（%zu 个点）", result.bestTrajectory.size());
        display->DrawReconstructionResults(result, false);
    }
    
    // 10. 设置相机视角
    if (gEve && gEve->GetDefaultGLViewer()) {
        TGLViewer* viewer = gEve->GetDefaultGLViewer();
        TGLCamera& camera = viewer->CurrentCamera();
        
        // 设置俯视角度
        camera.SetExternalCenter(true);
        camera.SetCenterVec(-1500.0, 0.0, 1500.0);  // 观察中心
        camera.RotateRad(-0.3, 0.5);                // 旋转视角
        camera.Dolly(2000, false, false);           // 拉远距离
        
        viewer->UpdateScene();
        viewer->RequestDraw();
    }
    
    display->Redraw();
    
    // 11. 保存图像
    TString outputDir = Form("test_output/reconstruction_realdata_event%lld", event_id);
    gSystem->mkdir(outputDir.Data(), kTRUE);
    
    TString pngPath = Form("%s/event_display_3d.png", outputDir.Data());
    
    if (gEve && gEve->GetDefaultGLViewer()) {
        TGLViewer* viewer = gEve->GetDefaultGLViewer();
        viewer->UpdateScene();
        gSystem->ProcessEvents();
        viewer->SavePicture(pngPath.Data());
        
        Info("run_event_display_realdata", "3D显示已保存: %s", pngPath.Data());
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "✓ 3D事件显示完成" << std::endl;
    std::cout << "  输出目录: " << outputDir.Data() << std::endl;
    std::cout << "  图像文件: event_display_3d.png" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n提示: 窗口保持打开，可以交互旋转视角" << std::endl;
    std::cout << "      输入 .q 退出ROOT" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // 注意：不删除 display 和 magField，保持显示窗口打开
}

// 自动调用版本
void run_event_display_realdata_auto() {
    run_event_display_realdata(0);
}
