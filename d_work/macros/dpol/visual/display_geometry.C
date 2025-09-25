#include "TEveManager.h"
#include "TGeoManager.h"
#include "TEveGeoNode.h"
#include "TGLViewer.h"

void display_geometry() {
    // 初始化 EVE 管理器
    TEveManager::Create();
    
    // 导入探测器几何
    if (!TGeoManager::Import("/home/tbt/workspace/dpol/smsimulator5.5/d_work/detector_geometry.gdml")) {
        std::cout << "Error: Could not import detector_geometry.gdml" << std::endl;
        return;
    }
    
    std::cout << "Successfully imported geometry from detector_geometry.gdml" << std::endl;
    
    // 遍历所有体积并设置可视化属性
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    std::cout << "Total volumes: " << volumes->GetEntries() << std::endl;
    
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        TString volName = vol->GetName();
        
        std::cout << "Volume " << i << ": " << volName << std::endl;
        
        // 根据体积名称设置不同的属性
        if (volName.Contains("expHall")) {
            // 世界体积 - 完全隐藏
            vol->SetVisibility(kFALSE);
            vol->SetTransparency(100);
        }
        else if (volName.Contains("yoke")) {
            // 磁轭 - 高度透明或隐藏
            vol->SetLineColor(kGray+2);
            vol->SetTransparency(95);
            vol->SetVisibility(kFALSE);  // 隐藏磁轭看内部
            vol->SetVisDaughters(kTRUE);
            std::cout << "Found yoke (magnet): " << volName << std::endl;
        }
        else if (volName.Contains("PDC")) {
            // PDC 探测器 - 红色，可见
            vol->SetLineColor(kRed);
            vol->SetFillColor(kRed-10);
            vol->SetTransparency(30);
            vol->SetVisibility(kTRUE);
            vol->SetVisDaughters(kTRUE);
            std::cout << "Found PDC detector: " << volName << std::endl;
        }
        else if (volName == "U") {
            // U 层 - 绿色
            vol->SetLineColor(kGreen+2);
            vol->SetFillColor(kGreen);
            vol->SetTransparency(20);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(2);
            std::cout << "Found U layer: " << volName << std::endl;
        }
        else if (volName == "X") {
            // X 层 - 蓝色
            vol->SetLineColor(kBlue+2);
            vol->SetFillColor(kBlue);
            vol->SetTransparency(20);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(2);
            std::cout << "Found X layer: " << volName << std::endl;
        }
        else if (volName == "V") {
            // V 层 - 橙色
            vol->SetLineColor(kOrange+2);
            vol->SetFillColor(kOrange);
            vol->SetTransparency(20);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(2);
            std::cout << "Found V layer: " << volName << std::endl;
        }
        else if (volName.Contains("target")) {
            // 靶材 - 黄色，突出显示
            vol->SetLineColor(kYellow+2);
            vol->SetFillColor(kYellow);
            vol->SetTransparency(0);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(3);
            std::cout << "Found target: " << volName << std::endl;
        }
        else if (volName.Contains("Dump")) {
            // 束流阻挡器 - 青色，半透明
            vol->SetLineColor(kCyan+2);
            vol->SetFillColor(kCyan);
            vol->SetTransparency(60);
            vol->SetVisibility(kTRUE);
            vol->SetVisDaughters(kTRUE);
            std::cout << "Found beam dump: " << volName << std::endl;
        }
        else {
            // 其他体积 - 默认设置
            vol->SetVisibility(kTRUE);
            vol->SetVisDaughters(kTRUE);
            vol->SetTransparency(40);
        }
        
        vol->SetVisDaughters(kTRUE);  // 确保显示子体积
    }
    
    // 设置几何显示选项
    gGeoManager->SetVisLevel(10);          // 显示深层级
    gGeoManager->SetVisOption(1);          // 实体模式
    gGeoManager->SetTopVisible(kFALSE);    // 隐藏顶层体积边界
    
    // 创建 EVE 几何节点
    TEveGeoTopNode* geomNode = new TEveGeoTopNode(gGeoManager, gGeoManager->GetTopNode());
    geomNode->SetVisLevel(10);
    
    // 添加到 EVE 场景
    gEve->AddGlobalElement(geomNode);
    
    // 设置 3D 视图
    TGLViewer* viewer = gEve->GetDefaultGLViewer();
    if (viewer) {
        // 设置背景色
        viewer->ColorSet().Background().SetColor(kBlack);
        
        // 设置相机位置和角度
        viewer->CurrentCamera().RotateRad(-1.2, 0.5);
        viewer->CurrentCamera().Dolly(4000, kFALSE, kFALSE);  // 拉远相机看全貌
        
        // 设置坐标轴和网格
        viewer->SetGuideState(TGLUtil::kAxesOrigin, kTRUE, kFALSE, 0);
        viewer->SetStyle(TGLRnrCtx::kFill);
        
        // 设置渲染质量
        viewer->SetLOD(TGLRnrCtx::kLODHigh);
        viewer->RequestDraw();
        
        std::cout << "3D viewer configured" << std::endl;
        std::cout << "Mouse controls:" << std::endl;
        std::cout << "  Left click + drag: Rotate view" << std::endl;
        std::cout << "  Right click + drag: Pan view" << std::endl;
        std::cout << "  Mouse wheel: Zoom in/out" << std::endl;
    }
    
    // 刷新显示
    gEve->Redraw3D(kTRUE);
    
    // 显示 EVE 浏览器
    gEve->GetBrowser()->GetTabRight()->SetTab(1);
    
    std::cout << "\n=== GEOMETRY POSITIONS ANALYSIS ===" << std::endl;
    std::cout << "From GDML file analysis:" << std::endl;
    std::cout << "1. Yoke (Magnet): 6700x4640x3500 mm at origin (0,0,0)" << std::endl;
    std::cout << "2. PDC1 & PDC2: 1700x800x190 mm (positions not explicitly given - likely at origin)" << std::endl;
    std::cout << "3. Target: 50x50x5 mm (scaled to 0,0,1 - effectively invisible)" << std::endl;
    std::cout << "4. U layer: at z=-12 mm relative to PDC" << std::endl;
    std::cout << "5. X layer: at z=0 mm relative to PDC" << std::endl;
    std::cout << "6. V layer: at z=+12 mm relative to PDC" << std::endl;
    std::cout << "7. Beam Dump: 2500x2500x3000 mm with opening 340x380x1200 mm" << std::endl;
    std::cout << "\nUse control functions:" << std::endl;
    std::cout << "  show_yoke()  - Show the yoke" << std::endl;
    std::cout << "  hide_yoke()  - Hide the yoke" << std::endl;
    std::cout << "  show_pdc()   - Highlight PDC detectors" << std::endl;
    std::cout << "  reset_view() - Reset camera view" << std::endl;
}

// 控制函数
void hide_yoke() {
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        if (TString(vol->GetName()).Contains("yoke")) {
            vol->SetVisibility(kFALSE);
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "Yoke hidden!" << std::endl;
}

void show_yoke() {
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        if (TString(vol->GetName()).Contains("yoke")) {
            vol->SetVisibility(kTRUE);
            vol->SetTransparency(80);
            vol->SetLineColor(kGray);
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "Yoke visible with transparency!" << std::endl;
}

void show_pdc() {
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
        TString name = vol->GetName();
        if (name.Contains("PDC") || name == "U" || name == "X" || name == "V") {
            vol->SetLineColor(kRed+2);
            vol->SetFillColor(kRed);
            vol->SetTransparency(10);
            vol->SetVisibility(kTRUE);
            vol->SetLineWidth(3);
            std::cout << "Highlighting: " << name << std::endl;
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "PDC detectors highlighted!" << std::endl;
}

void reset_view() {
    TGLViewer* viewer = gEve->GetDefaultGLViewer();
    if (viewer) {
        viewer->CurrentCamera().Reset();
        viewer->CurrentCamera().RotateRad(-1.2, 0.5);
        viewer->CurrentCamera().Dolly(4000, kFALSE, kFALSE);
        viewer->RequestDraw();
        std::cout << "View reset to default position" << std::endl;
    }
}

void show_positions() {
    std::cout << "\n=== COMPONENT POSITIONS (from GDML) ===" << std::endl;
    
    // 检查实际的物理体积位置
    TGeoNode* top = gGeoManager->GetTopNode();
    if (top) {
        TObjArray* nodes = top->GetNodes();
        if (nodes) {
            for (int i = 0; i < nodes->GetEntries(); i++) {
                TGeoNode* node = (TGeoNode*)nodes->At(i);
                TString nodeName = node->GetName();
                TGeoMatrix* matrix = node->GetMatrix();
                
                const Double_t* trans = matrix->GetTranslation();
                std::cout << nodeName << ": position (" 
                         << trans[0] << ", " << trans[1] << ", " << trans[2] 
                         << ") mm" << std::endl;
            }
        }
    }
}
