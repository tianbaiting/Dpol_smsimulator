#include "TEveManager.h"
#include "TGeoManager.h"
#include "TEveGeoNode.h"
#include "TGLViewer.h"
#include "TGeoVolume.h"
#include "TGeoNode.h"
#include "TEveBrowser.h"
#include "TGTab.h"
#include "TEnv.h"
#include "TROOT.h"
#include <iostream>

// Forward declarations for helper functions
void hide_yoke();
void show_yoke();
void show_pdc();
void reset_view();
void show_positions();

/**
 * @brief 最终版本的几何体显示函数
 * * 该版本包含了针对WSL环境的图形渲染修复和全面的健壮性检查。
 */
void display_geometrycopy() {
    // 检查ROOT是否已初始化
    if (!gROOT) {
        std::cout << "Error: ROOT is not initialized." << std::endl;
        return;
    }
    
    // 初始化 EVE 管理器
    TEveManager::Create();

    // ===================================================================
    //  WSL/Graphics fix: 禁用鼠标悬停高亮以防止渲染崩溃
    //  这是解决之前遇到的段错误的关键
    // ===================================================================
    gEnv->SetValue("OpenGL.Highlight.Select", 0);
    std::cout << "Info: Disabled OpenGL mouse-over highlight for stability on WSL." << std::endl;
    
    // 导入探测器几何
    const char* geom_file = "/home/tbt/workspace/dpol/smsimulator5.5/d_work/detector_geometry.gdml";
    if (!TGeoManager::Import(geom_file)) {
        std::cout << "Error: Could not import geometry from " << geom_file << std::endl;
        // 清理已创建的TEveManager
        delete gEve;
        return;
    }
    
    std::cout << "Successfully imported geometry from " << geom_file << std::endl;
    
    // 遍历所有体积并设置可视化属性
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    if (!volumes) {
        std::cout << "Warning: Could not get list of volumes from TGeoManager." << std::endl;
        return;
    }
    
    std::cout << "Total volumes: " << volumes->GetEntries() << std::endl;
    
    for (int i = 0; i < volumes->GetEntries(); i++) {
        TGeoVolume* vol = dynamic_cast<TGeoVolume*>(volumes->At(i));
        if (!vol) {
            std::cout << "Warning: Object at index " << i << " is not a TGeoVolume. Skipping." << std::endl;
            continue;
        }
        
        TString volName = vol->GetName();
        
        // 根据体积名称设置不同的属性
        if (volName.Contains("expHall")) { vol->SetVisibility(kFALSE); vol->SetTransparency(100); }
        else if (volName.Contains("yoke")) { vol->SetLineColor(kGray+2); vol->SetTransparency(95); vol->SetVisibility(kFALSE); }
        else if (volName.Contains("PDC")) { vol->SetLineColor(kRed); vol->SetFillColor(kRed-10); vol->SetTransparency(30); }
        else if (volName == "U") { vol->SetLineColor(kGreen+2); vol->SetFillColor(kGreen); vol->SetTransparency(20); vol->SetLineWidth(2); }
        else if (volName == "X") { vol->SetLineColor(kBlue+2); vol->SetFillColor(kBlue); vol->SetTransparency(20); vol->SetLineWidth(2); }
        else if (volName == "V") { vol->SetLineColor(kOrange+2); vol->SetFillColor(kOrange); vol->SetTransparency(20); vol->SetLineWidth(2); }
        else if (volName.Contains("target")) { vol->SetLineColor(kYellow+2); vol->SetFillColor(kYellow); vol->SetTransparency(0); vol->SetLineWidth(3); }
        else if (volName.Contains("Dump")) { vol->SetLineColor(kCyan+2); vol->SetFillColor(kCyan); vol->SetTransparency(60); }
        else { vol->SetTransparency(40); }

        vol->SetVisDaughters(kTRUE);
    }
    
    // 设置几何显示选项
    gGeoManager->SetVisLevel(10);
    gGeoManager->SetVisOption(1);
    gGeoManager->SetTopVisible(kFALSE);
    
    TGeoNode* topNode = gGeoManager->GetTopNode();
    if (!topNode) {
        std::cout << "Error: Could not get top node from TGeoManager." << std::endl;
        return;
    }
    TEveGeoTopNode* geomNode = new TEveGeoTopNode(gGeoManager, topNode);
    geomNode->SetVisLevel(10);
    
    gEve->AddGlobalElement(geomNode);
    
    // 配置3D视图
    TGLViewer* viewer = gEve->GetDefaultGLViewer();
    if (viewer) {
        viewer->ColorSet().Background().SetColor(kBlack);
        viewer->CurrentCamera().RotateRad(-1.2, 0.5);
        viewer->CurrentCamera().Dolly(4000, kFALSE, kFALSE);
        viewer->SetGuideState(TGLUtil::kAxesOrigin, kTRUE, kFALSE, 0);
        viewer->SetStyle(TGLRnrCtx::kFill);
        viewer->RequestDraw();
        std::cout << "3D viewer configured." << std::endl;
    } else {
        std::cout << "Warning: Could not get default TGLViewer. Running in non-GUI mode?" << std::endl;
    }
    
    gEve->Redraw3D(kTRUE);
    
    // 安全地操作 EVE 浏览器
    TEveBrowser* browser = gEve->GetBrowser();
    if (browser) {
        TGTab* tabRight = browser->GetTabRight();
        if (tabRight) {
            tabRight->SetTab(1);
        }
    }
}

// -------------------
//  辅助控制函数
// -------------------

// 检查全局指针是否已初始化的辅助函数
bool CheckGlobals() {
    if (!gGeoManager || !gEve) {
        std::cout << "Error: Geometry not loaded. Please run display_geometry_final() first." << std::endl;
        return false;
    }
    return true;
}

void hide_yoke() {
    if (!CheckGlobals()) return;
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    if (!volumes) return;
    for (int i = 0; i < volumes->GetEntries(); i++) {
        if (auto vol = dynamic_cast<TGeoVolume*>(volumes->At(i))) {
            if (TString(vol->GetName()).Contains("yoke")) {
                vol->SetVisibility(kFALSE);
            }
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "Yoke hidden!" << std::endl;
}

void show_yoke() {
    if (!CheckGlobals()) return;
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    if (!volumes) return;
    for (int i = 0; i < volumes->GetEntries(); i++) {
        if (auto vol = dynamic_cast<TGeoVolume*>(volumes->At(i))) {
            if (TString(vol->GetName()).Contains("yoke")) {
                vol->SetVisibility(kTRUE);
                vol->SetTransparency(80);
                vol->SetLineColor(kGray);
            }
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "Yoke visible with transparency!" << std::endl;
}

void show_pdc() {
    if (!CheckGlobals()) return;
    TObjArray* volumes = gGeoManager->GetListOfVolumes();
    if (!volumes) return;
    for (int i = 0; i < volumes->GetEntries(); i++) {
        if (auto vol = dynamic_cast<TGeoVolume*>(volumes->At(i))) {
            TString name = vol->GetName();
            if (name.Contains("PDC") || name == "U" || name == "X" || name == "V") {
                vol->SetLineColor(kRed+2);
                vol->SetFillColor(kRed);
                vol->SetTransparency(10);
            }
        }
    }
    gEve->Redraw3D(kTRUE);
    std::cout << "PDC detectors highlighted!" << std::endl;
}

void reset_view() {
    if (!gEve) { CheckGlobals(); return; }
    TGLViewer* viewer = gEve->GetDefaultGLViewer();
    if (viewer) {
        viewer->CurrentCamera().Reset();
        viewer->CurrentCamera().RotateRad(-1.2, 0.5);
        viewer->CurrentCamera().Dolly(4000, kFALSE, kFALSE);
        viewer->RequestDraw();
        std::cout << "View reset to default position." << std::endl;
    }
}

void show_positions() {
    if (!gGeoManager) { CheckGlobals(); return; }
    std::cout << "\n=== COMPONENT POSITIONS (Live Query) ===" << std::endl;
    TGeoNode* top = gGeoManager->GetTopNode();
    if (top) {
        TObjArray* nodes = top->GetNodes();
        if (nodes) {
            for (int i = 0; i < nodes->GetEntries(); i++) {
                auto node = dynamic_cast<TGeoNode*>(nodes->At(i));
                if (!node) continue;
                
                TGeoMatrix* matrix = node->GetMatrix();
                if (matrix) {
                    const Double_t* trans = matrix->GetTranslation();
                    std::cout << node->GetName() << ": position (" 
                              << trans[0] << ", " << trans[1] << ", " << trans[2] 
                              << ") mm" << std::endl;
                } else {
                    std::cout << node->GetName() << ": position (0, 0, 0) mm [Default - No matrix]" << std::endl;
                }
            }
        }
    }
}