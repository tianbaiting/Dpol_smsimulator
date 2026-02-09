// Usage: root -l 'scripts/visualization/dpol_visual/display_geometry.C()'
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
#include "TEvePointSet.h"
#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include "TSimData.hh"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TString.h"
#include "TMath.h"
#include <iostream>
#include "TGLEventHandler.h"

// 新增用于真三维球体
#include "TEveGeoShape.h"
#include "TGeoSphere.h"
// #include "TEveElementList.h"

// Forward declarations for helper functions
void hide_yoke();
void show_yoke();
void show_pdc();
void reset_view();
void show_positions();

// Energy deposition visualization functions
// void load_energy_deposits_file(const char* filePath = "SMSIMDIR/d_work/output_tree/test0000.root");
// void load_energy_deposits_file(const char* filePath = "SMSIMDIR/d_work/output_tree/d+Pb208E190g050xyz_np_sametime_vis0000.root");
void load_energy_deposits_file(const char* filePath /*= SMSIMDIR/d_work/output_tree/d+Sn124E190g050ynp_same0000.root*/){


void show_event(Long64_t eventNumber = -1);
void next_event();
void prev_event();
void show_energy_deposits();
void hide_energy_deposits();
void clear_energy_deposits();
void close_energy_deposits_file();
void show_energy_deposits_usage();

// Global variables for energy deposits
// TEvePointSet* g_energyDeposits = nullptr;
// 用 TEveElementList 保存每个事件的球体元素列表（真三维球体）
TEveElementList* g_energyDeposits = nullptr;
TFile* g_currentFile = nullptr;
TTree* g_currentTree = nullptr;
TClonesArray* g_fragSimDataArray = nullptr;
Long64_t g_currentEvent = 0;
Long64_t g_totalEvents = 0;
TString g_currentFilePath = "";
 // 球半径（mm），由用户指定为 30mm
 const Double_t g_energySphereRadius = 30.0;

/**
 * @brief 最终版本的几何体显示函数
 * * 该版本包含了针对WSL环境的图形渲染修复和全面的健壮性检查。
 */
void display_geometry() {
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
    gEnv->SetValue("OpenGL.EventHandler.EnableMouseOver", 0);
    gEnv->SetValue("OpenGL.EventHandler.EnableSelection", 0);
    gEnv->SetValue("OpenGL.Timer.MouseOver", 0);
    gEnv->SetValue("OpenGL.EventHandler.SelectTimeout", 0);
    gEnv->SetValue("OpenGL.EventHandler.UseTimeOut", 0);
    gEnv->SetValue("OpenGL.SelectBuffer.Size", 0);
    gEnv->SetValue("X11.UseXft", "no");
    std::cout << "Info: Disabled OpenGL interactions for stability on WSL." << std::endl;

    //按照geant4的建议，设置TGeoManager的单位为mm-based
    TGeoManager::LockDefaultUnits(kFALSE);
    TGeoManager::SetDefaultUnits(TGeoManager::EDefaultUnits::kG4Units);
    std::cout << "Info: Set TGeoManager Default Units to kG4Units (mm-based)." << std::endl;
  
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
        
        // // WSL稳定性设置：关闭鼠标悬停选择和提示以避免OpenGL崩溃
        // if (auto handlerBase = viewer->GetEventHandler()) {
        //     if (auto handler = dynamic_cast<TGLEventHandler*>(handlerBase)) {
        //         handler->StopMouseTimer();
        //         handler->ClearMouseOver();
        //         handler->SetDoInternalSelection(kFALSE);
        //         handler->SetMouseOverSelectDelay(0);
        //         handler->SetMouseOverTooltipDelay(0);
        //         handler->SetTooltipPixelTolerance(0);
        //         handler->SetSecSelType(-1);
        //         handler->SetArcBall(kFALSE);
        //         std::cout << "Info: Disabled TGLEventHandler internal selection." << std::endl;
        //     } else {
        //         std::cout << "Info: TGLEventHandler not available; falling back to default event handler." << std::endl;
        //     }
        // }
        
        viewer->RequestDraw();
        std::cout << "3D viewer configured with WSL stability settings." << std::endl;
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
                vol->SetTransparency(99);
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

// =================================================================
//  能量沉积可视化功能
// =================================================================

/**
 * @brief 加载ROOT文件和设置数据树
 * @param filePath ROOT文件路径
 */
void load_energy_deposits_file(const char* filePath) {
    if (!CheckGlobals()) return;
    
    // 清理之前的数据
    clear_energy_deposits();
    
    // 关闭之前的文件
    if (g_currentFile) {
        g_currentFile->Close();
        delete g_currentFile;
        g_currentFile = nullptr;
    }
    
    // 加载必要的库
    gSystem->Load("libsmdata.so");
    
    g_currentFile = TFile::Open(filePath);
    if (!g_currentFile || g_currentFile->IsZombie()) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
        g_currentFile = nullptr;
        return;
    }
    
    g_currentTree = (TTree*)g_currentFile->Get("tree");
    if (!g_currentTree) {
        std::cerr << "Error: Could not find TTree with name 'tree'" << std::endl;
        g_currentFile->Close();
        delete g_currentFile;
        g_currentFile = nullptr;
        return;
    }
    
    // 设置分支地址
    g_fragSimDataArray = nullptr;
    g_currentTree->SetBranchAddress("FragSimData", &g_fragSimDataArray);
    
    g_totalEvents = g_currentTree->GetEntries();
    g_currentEvent = 0;
    g_currentFilePath = filePath;
    
    std::cout << "Successfully loaded file: " << filePath << std::endl;
    std::cout << "Total events: " << g_totalEvents << std::endl;
    
    // 显示第一个事件
    show_event(0);
}

/**
 * @brief 显示指定事件的能量沉积
 * @param eventNumber 事件号，-1表示显示当前事件
 */
void show_event(Long64_t eventNumber) {
    if (!CheckGlobals()) return;
    
    if (!g_currentTree) {
        std::cout << "No file loaded. Use load_energy_deposits_file() first." << std::endl;
        return;
    }
    
    // 设置事件号
    if (eventNumber >= 0) {
        if (eventNumber >= g_totalEvents) {
            std::cout << "Event " << eventNumber << " out of range (0-" << (g_totalEvents-1) << ")" << std::endl;
            return;
        }
        g_currentEvent = eventNumber;
    }
    
    // 清理之前的显示
    clear_energy_deposits();
    
    // 读取当前事件
    g_currentTree->GetEntry(g_currentEvent);
    
    if (!g_fragSimDataArray) {
        std::cout << "No data in event " << g_currentEvent << std::endl;
        return;
    }
    
    Int_t numHits = g_fragSimDataArray->GetEntries();
    int totalHits = 0;
     
    std::cout << "Event " << g_currentEvent << " has " << numHits << " hits" << std::endl;
     
    // 创建事件级元素列表以便后续统一管理/删除
    g_energyDeposits = new TEveElementList(Form("Event_%lld_EnergyDeposits", g_currentEvent));
    // 确保列表本身可渲染（否则子元素可能不被显示）
    g_energyDeposits->SetRnrSelf(kTRUE);
    
    // 使用 TEvePointSet 显示能量沉积点（兼容并避免 TEveGeoShape 的构造/定位差异）
    TEvePointSet* pts = new TEvePointSet(Form("Event_%lld_PDC_Points", g_currentEvent));
    pts->SetMarkerStyle(20);
    // 增大 marker 尺寸以确保在当前视图与缩放下可见（可根据需要调小/调大）
    pts->SetMarkerSize(1);
    pts->SetMainColor(kRed);
    pts->SetRnrSelf(kTRUE);
    // 禁用拾取（可选），并确保 marker 在前端渲染
    pts->SetPickable(kFALSE);
    
    // 循环读取当前事件中的击中点，显示 PDC 中的击中位置
    for (Int_t j = 0; j < numHits; ++j) {
        TSimData *hit = (TSimData*)g_fragSimDataArray->At(j);
        if (!hit) continue;
        
        // 只显示PDC中的能量沉积（不使用未声明的 energyDeposited 变量）
        if (hit->fDetectorName.Contains("PDC")) {
            TVector3 position = hit->fPrePosition;
            pts->SetNextPoint(position.X(), position.Y(), position.Z());
            std::cout << "  Hit " << j << ": " << hit->fDetectorName.Data()
                      << " at (" << position.X() << ", " << position.Y() << ", " << position.Z() << ") mm"
                      << std::endl;
            totalHits++;
        }
    }
     
    if (totalHits > 0) {
        // 将点集加入事件元素列表并加入 EVE 场景
        g_energyDeposits->AddElement(pts);
        if (gEve) {
            gEve->AddElement(g_energyDeposits);
            // 强制使用同步重绘以立刻可见（避免 kFALSE 在某些环境下不刷新）
            gEve->Redraw3D(kTRUE);
        }
        std::cout << "Displayed " << totalHits << " PDC hits for event " << g_currentEvent << std::endl;
    } else {
        std::cout << "No energy deposits found in PDC detectors for event " << g_currentEvent << std::endl;
        // 清理临时对象（如果没有命中则不将其加入全局列表）
        if (pts) { delete pts; pts = nullptr; }
        if (g_energyDeposits) { delete g_energyDeposits; g_energyDeposits = nullptr; }
    }
}

/**
 * @brief 显示下一个事件
 */
void next_event() {
    if (!g_currentTree) {
        std::cout << "No file loaded. Use load_energy_deposits_file() first." << std::endl;
        return;
    }
    
    if (g_currentEvent < g_totalEvents - 1) {
        show_event(g_currentEvent + 1);
    } else {
        std::cout << "Already at last event (" << (g_totalEvents-1) << ")" << std::endl;
    }
}

/**
 * @brief 显示上一个事件
 */
void prev_event() {
    if (!g_currentTree) {
        std::cout << "No file loaded. Use load_energy_deposits_file() first." << std::endl;
        return;
    }
    
    if (g_currentEvent > 0) {
        show_event(g_currentEvent - 1);
    } else {
        std::cout << "Already at first event (0)" << std::endl;
    }
}

/**
 * @brief 显示能量沉积点
 */
void show_energy_deposits() {
    if (!g_energyDeposits) {
        std::cout << "No energy deposits loaded. Use load_energy_deposits() first." << std::endl;
        return;
    }
    
    g_energyDeposits->SetRnrSelf(kTRUE);
    gEve->Redraw3D(kFALSE);
    std::cout << "Energy deposits are now visible" << std::endl;
}

/**
 * @brief 隐藏能量沉积点
 */
void hide_energy_deposits() {
    if (!g_energyDeposits) {
        std::cout << "No energy deposits loaded." << std::endl;
        return;
    }
    
    g_energyDeposits->SetRnrSelf(kFALSE);
    gEve->Redraw3D(kFALSE);
    std::cout << "Energy deposits are now hidden" << std::endl;
}

/**
 * @brief 清除所有能量沉积数据
 */
void clear_energy_deposits() {
    if (g_energyDeposits) {
        if (gEve) {
            g_energyDeposits->SetRnrSelf(kFALSE);
            gEve->RemoveElement(g_energyDeposits, nullptr);
        }
        delete g_energyDeposits;
        g_energyDeposits = nullptr;
    }
}

/**
 * @brief 关闭当前文件并清理所有数据
 */
void close_energy_deposits_file() {
    clear_energy_deposits();
    
    if (g_currentFile) {
        g_currentFile->Close();
        delete g_currentFile;
        g_currentFile = nullptr;
    }
    
    g_currentTree = nullptr;
    g_fragSimDataArray = nullptr;
    g_currentEvent = 0;
    g_totalEvents = 0;
    g_currentFilePath = "";
    
    std::cout << "File closed and all energy deposit data cleared" << std::endl;
}

// =================================================================
//  使用示例
// =================================================================

/**
 * @brief 显示能量沉积功能的使用方法
 */
void show_energy_deposits_usage() {
    std::cout << "\n============= 单事件能量沉积可视化使用指南 =============" << std::endl;
    std::cout << "1. 首先运行 display_geometry() 加载探测器几何" << std::endl;
    std::cout << "2. 加载ROOT文件：" << std::endl;
    std::cout << "   load_energy_deposits_file(\"output_tree/sim_output.root\")" << std::endl;
    std::cout << "\n3. 浏览事件：" << std::endl;
    std::cout << "   show_event(0)    // 显示第0个事件" << std::endl;
    std::cout << "   next_event()     // 下一个事件" << std::endl;
    std::cout << "   prev_event()     // 上一个事件" << std::endl;
    std::cout << "   show_event(123)  // 显示指定事件号" << std::endl;
    std::cout << "\n4. 控制显示：" << std::endl;
    std::cout << "   show_energy_deposits()  // 显示当前事件的能量沉积点" << std::endl;
    std::cout << "   hide_energy_deposits()  // 隐藏能量沉积点" << std::endl;
    std::cout << "   clear_energy_deposits() // 清除当前显示" << std::endl;
    std::cout << "   close_energy_deposits_file() // 关闭文件，清理所有数据" << std::endl;
    std::cout << "\n5. 特性说明：" << std::endl;
    std::cout << "   - 每次只显示一个事件的能量沉积" << std::endl;
    std::cout << "   - 只显示PDC探测器中的能量沉积点" << std::endl;
    // std::cout << "   - 能量阈值：> 0.001 MeV" << std::endl;
    std::cout << "   - 红色圆点标记能量沉积位置" << std::endl;
    std::cout << "\n6. 当前状态查询：" << std::endl;
    if (g_currentFilePath != "") {
        std::cout << "   文件：" << g_currentFilePath << std::endl;
        std::cout << "   当前事件：" << g_currentEvent << " / " << (g_totalEvents-1) << std::endl;
        std::cout << "   总事件数：" << g_totalEvents << std::endl;
    } else {
        std::cout << "   无文件加载" << std::endl;
    }
    std::cout << "=======================================================" << std::endl;
}