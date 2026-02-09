// Usage: root -l 'scripts/visualization/dpol_visual/display_geometry_improved.C()'
#include "TEveManager.h"
#include "TGeoManager.h"
#include "TEveGeoNode.h"
#include "TGLViewer.h"
#include "TGeoVolume.h"
#include "TGeoNode.h"
#include "TString.h"
#include <iostream>

void display_geometry_improved() {
    std::cout << "=== IMPROVED GEOMETRY DISPLAY ===" << std::endl;
    
    // 初始化 EVE 管理器
    TEveManager::Create();
    
    // 导入探测器几何
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir) {
        std::cerr << "Environment variable SMSIMDIR is not set" << std::endl;
        return;
    }
    std::string smsBase(smsDir);
    TString gdmlFile = (smsBase + "/d_work/detector_geometry.gdml").c_str();
    if (!TGeoManager::Import(gdmlFile)) {
        std::cout << "Error: Could not import " << gdmlFile << std::endl;
        return;
    }
    
    std::cout << "✓ Successfully imported geometry from " << gdmlFile << std::endl;
    
    // 分析并报告几何结构
    analyzeGeometry();
    
    // 设置体积可视化属性
    setupVisualization();
    
    // 创建 EVE 场景
    setupEVEScene();
    
    // 配置3D视图
    setup3DViewer();
    
    // 显示位置信息
    showPositionInfo();
    
    std::cout << "\n=== CONTROL FUNCTIONS ===" << std::endl;
    std::cout << "Available functions:" << std::endl;
    std::cout << "  show_only_pdc()     - Show only PDC detectors" << std::endl;
    std::cout << "  show_all()          - Show all components" << std::endl;
    std::cout << "  hide_yoke()         - Hide magnetic yoke" << std::endl;
    std::cout << "  highlight_target()  - Highlight target" << std::endl;
    std::cout << "  top_view()          - Switch to top view" << std::endl;
    std::cout << "  side_view()         - Switch to side view" << std::endl;
    std::cout << "  analyze_positions() - Print detailed position analysis" << std::endl;
}

void analyzeGeometry() {
    std::cout << "\n=== GEOMETRY ANALYSIS ===" << std::endl;
    
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    std::cout << "Total volumes: " << volumes->GetEntries() << std::endl;
    
    // 统计不同类型的体积
    int pdcCount = 0, layerCount = 0, yokeCount = 0, targetCount = 0;
    
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        TString volName = vol->GetName();
        
        if (volName.Contains("PDC")) pdcCount++;
        else if (volName == "U" || volName == "X" || volName == "V") layerCount++;
        else if (volName.Contains("yoke")) yokeCount++;
        else if (volName.Contains("target")) targetCount++;
    }
    
    std::cout << "Component summary:" << std::endl;
    std::cout << "  - PDC volumes: " << pdcCount << std::endl;
    std::cout << "  - Detection layers (U/X/V): " << layerCount << std::endl;
    std::cout << "  - Yoke components: " << yokeCount << std::endl;
    std::cout << "  - Target components: " << targetCount << std::endl;
}

void setupVisualization() {
    std::cout << "\n=== SETTING UP VISUALIZATION ===" << std::endl;
    
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        TString volName = vol->GetName();
        
        // 世界体积 - 隐藏
        if (volName.Contains("expHall")) {
            vol->SetVisibility(kFALSE);
            vol->SetTransparency(100);
        }
        // 磁轭 - 半透明灰色
        else if (volName.Contains("yoke")) {
            vol->SetLineColor(kGray+1);
            vol->SetFillColor(kGray);
            vol->SetTransparency(85);
            vol->SetVisibility(kTRUE);
            vol->SetVisDaughters(kTRUE);
        }
        // PDC探测器外壳 - 红色
        else if (volName.Contains("PDC") && !volName.Contains("SD")) {
            vol->SetLineColor(kRed+2);
            vol->SetFillColor(kRed-10);
            vol->SetTransparency(40);
            vol->SetVisibility(kTRUE);
            vol->SetVisDaughters(kTRUE);
            vol->SetLineWidth(2);
        }
        // U层 - 绿色
        else if (volName == "U") {
            vol->SetLineColor(kGreen+2);
            vol->SetFillColor(kGreen);
            vol->SetTransparency(30);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(3);
        }
        // X层 - 蓝色
        else if (volName == "X") {
            vol->SetLineColor(kBlue+2);
            vol->SetFillColor(kBlue);
            vol->SetTransparency(30);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(3);
        }
        // V层 - 洋红色
        else if (volName == "V") {
            vol->SetLineColor(kMagenta+2);
            vol->SetFillColor(kMagenta);
            vol->SetTransparency(30);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(3);
        }
        // 靶标 - 黄色，增大显示
        else if (volName.Contains("target")) {
            vol->SetLineColor(kYellow+2);
            vol->SetFillColor(kYellow);
            vol->SetTransparency(10);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(4);
        }
        // 束流挡块 - 青色
        else if (volName.Contains("Dump")) {
            vol->SetLineColor(kCyan+2);
            vol->SetFillColor(kCyan);
            vol->SetTransparency(60);
            vol->SetVisibility(kTRUE);
            vol->SetVisDaughters(kTRUE);
        }
        
        vol->SetVisDaughters(kTRUE);
    }
}

void setupEVEScene() {
    std::cout << "Setting up EVE scene..." << std::endl;
    
    // 设置几何显示选项
    gGeoManager->SetVisLevel(15);
    gGeoManager->SetVisOption(1);
    gGeoManager->SetTopVisible(kFALSE);
    
    // 创建 EVE 几何节点
    TEveGeoTopNode* geomNode = new TEveGeoTopNode(gGeoManager, gGeoManager->GetTopNode());
    geomNode->SetVisLevel(15);
    
    // 添加到 EVE 场景
    gEve->AddGlobalElement(geomNode);
}

void setup3DViewer() {
    std::cout << "Configuring 3D viewer..." << std::endl;
    
    TGLViewer* viewer = gEve->GetDefaultGLViewer();
    if (viewer) {
        // 设置背景
        viewer->ColorSet().Background().SetColor(kWhite);
        viewer->ColorSet().Foreground().SetColor(kBlack);
        
        // 设置初始相机位置 - 侧视图
        viewer->CurrentCamera().Reset();
        viewer->CurrentCamera().RotateRad(-0.5, 0.3);  // 轻微旋转看到深度
        viewer->CurrentCamera().Dolly(6000, kFALSE, kFALSE);  // 拉远看全貌
        
        // 设置显示选项
        viewer->SetGuideState(TGLUtil::kAxesOrigin, kTRUE, kFALSE, 0);
        viewer->SetStyle(TGLRnrCtx::kFill);
        viewer->SetLOD(TGLRnrCtx::kLODHigh);
        
        viewer->RequestDraw();
    }
    
    // 刷新显示
    gEve->Redraw3D(kTRUE);
    gEve->GetBrowser()->GetTabRight()->SetTab(1);
}

void showPositionInfo() {
    std::cout << "\n=== COMPONENT POSITIONS ===" << std::endl;
    
    TGeoNode* top = gGeoManager->GetTopNode();
    if (top && top->GetNodes()) {
        TObjArray* nodes = top->GetNodes();
        
        for (int i = 0; i < nodes->GetEntries(); i++) {
            TGeoNode* node = (TGeoNode*)nodes->At(i);
            TString nodeName = node->GetName();
            TGeoMatrix* matrix = node->GetMatrix();
            
            if (matrix) {
                const Double_t* trans = matrix->GetTranslation();
                std::cout << nodeName << ": (" 
                         << trans[0] << ", " << trans[1] << ", " << trans[2] 
                         << ") mm" << std::endl;
            }
        }
    }
}

// =================== CONTROL FUNCTIONS ===================

void show_only_pdc() {
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        TString name = vol->GetName();
        
        if (name.Contains("PDC") || name == "U" || name == "X" || name == "V") {
            vol->SetVisibility(kTRUE);
            vol->SetTransparency(20);
        } else if (!name.Contains("expHall")) {
            vol->SetVisibility(kFALSE);
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "✓ Showing only PDC detectors" << std::endl;
}

void show_all() {
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        TString name = vol->GetName();
        
        if (!name.Contains("expHall")) {
            vol->SetVisibility(kTRUE);
            if (name.Contains("yoke")) vol->SetTransparency(85);
            else vol->SetTransparency(30);
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "✓ Showing all components" << std::endl;
}

void hide_yoke() {
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        if (TString(vol->GetName()).Contains("yoke")) {
            vol->SetVisibility(kFALSE);
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "✓ Yoke hidden" << std::endl;
}

void highlight_target() {
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        if (TString(vol->GetName()).Contains("target")) {
            vol->SetLineColor(kRed);
            vol->SetFillColor(kRed);
            vol->SetTransparency(0);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(5);
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "✓ Target highlighted" << std::endl;
}

void top_view() {
    TGLViewer* viewer = gEve->GetDefaultGLViewer();
    if (viewer) {
        viewer->CurrentCamera().Reset();
        viewer->CurrentCamera().RotateRad(0, TMath::PiOver2());  // 顶视图
        viewer->CurrentCamera().Dolly(5000, kFALSE, kFALSE);
        viewer->RequestDraw();
        std::cout << "✓ Switched to top view" << std::endl;
    }
}

void side_view() {
    TGLViewer* viewer = gEve->GetDefaultGLViewer();
    if (viewer) {
        viewer->CurrentCamera().Reset();
        viewer->CurrentCamera().RotateRad(-TMath::PiOver2(), 0);  // 侧视图
        viewer->CurrentCamera().Dolly(5000, kFALSE, kFALSE);
        viewer->RequestDraw();
        std::cout << "✓ Switched to side view" << std::endl;
    }
}

void analyze_positions() {
    std::cout << "\n=== DETAILED POSITION ANALYSIS ===" << std::endl;
    
    // 分析GDML文件中的实际位置
    TGeoNode* top = gGeoManager->GetTopNode();
    if (top && top->GetNodes()) {
        TObjArray* nodes = top->GetNodes();
        
        std::cout << "Physical placement analysis:" << std::endl;
        for (int i = 0; i < nodes->GetEntries(); i++) {
            TGeoNode* node = (TGeoNode*)nodes->At(i);
            TString nodeName = node->GetName();
            TGeoMatrix* matrix = node->GetMatrix();
            
            if (matrix) {
                const Double_t* trans = matrix->GetTranslation();
                const Double_t* rot = matrix->GetRotationMatrix();
                
                std::cout << "\n" << nodeName << ":" << std::endl;
                std::cout << "  Position: (" << trans[0] << ", " << trans[1] << ", " << trans[2] << ") mm" << std::endl;
                
                // 检查旋转
                bool hasRotation = false;
                for (int j = 0; j < 9; j++) {
                    if ((j % 4 == 0 && abs(rot[j] - 1.0) > 1e-6) || 
                        (j % 4 != 0 && abs(rot[j]) > 1e-6)) {
                        hasRotation = true;
                        break;
                    }
                }
                if (hasRotation) {
                    std::cout << "  Rotation: Yes" << std::endl;
                } else {
                    std::cout << "  Rotation: None" << std::endl;
                }
                
                // 体积信息
                TGeoVolume* vol = node->GetVolume();
                if (vol && vol->GetShape()) {
                    TGeoBBox* box = dynamic_cast<TGeoBBox*>(vol->GetShape());
                    if (box) {
                        std::cout << "  Dimensions: " << 2*box->GetDX() << " × " 
                                 << 2*box->GetDY() << " × " << 2*box->GetDZ() << " mm" << std::endl;
                    }
                }
            }
        }
    }
    
    std::cout << "\n=== EXPECTED vs ACTUAL ===" << std::endl;
    std::cout << "Expected PDC positions:" << std::endl;
    std::cout << "  PDC1: (400, 0, 4100) mm" << std::endl;
    std::cout << "  PDC2: (400, 0, 4500) mm" << std::endl;
    std::cout << "Expected Target: (0, 0, 0) mm" << std::endl;
    std::cout << "Expected Dump: (0, 0, 8000) mm" << std::endl;
}