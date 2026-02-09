// Usage: root -l 'scripts/visualization/run_display.C(0, true)'
#include "TSystem.h"
#include "TApplication.h"
#include "TEvePointSet.h"

// 包含我们所有自定义类的头文件
#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "EventDisplay.hh"
#include "RecoEvent.hh"
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include <cstring>

// 主函数
void run_display(Long64_t event_id = 0, bool show_trajectories = true, const char* overlay_file = "") {
    // 1. 加载我们编译好的库
    // 加载库，确保只加载一次
    static bool loaded = false;
    if (!loaded) {
        if (gSystem->Load("libPDCAnalysisTools.so") < 0) {
            Error("run_display_safe", "无法加载 libPDCAnalysisTools.so 库");
            return;
        }
        loaded = true;
        Info("run_display_safe", "库已成功加载");
    }
    //   // 添加 WSL 稳定性设置  
    // gEnv->SetValue("OpenGL.Highlight.Select", 0);  
    // gEnv->SetValue("OpenGL.EventHandler.EnableMouseOver", 0);  
    // gEnv->SetValue("OpenGL.EventHandler.EnableSelection", 0);  
    // gEnv->SetValue("OpenGL.Timer.MouseOver", 0);  
      

    // 2. 创建并设置所有分析工具 (对象在栈上创建，函数结束时自动销毁)
    GeometryManager geo;
    geo.LoadGeometry("/home/tian/workspace/dpol/smsimulator5.5/d_work/geometry/5deg_1.2T.mac");

    PDCSimAna ana(geo);
    ana.SetSmearing(0.5, 0.5); // 设置模糊参数

    EventDataReader reader("/home/tian/workspace/dpol/smsimulator5.5/d_work/output_tree/testry0000.root");
    if (!reader.IsOpen()) {
        Error("run_display", "无法打开数据文件!");
        return; // 如果文件打不开，则退出
    }

    EventDisplay display("/home/tian/workspace/dpol/smsimulator5.5/d_work/detector_geometry.gdml", geo);

    // 加载磁场用于轨迹计算
    MagneticField* magField = nullptr;
    if (show_trajectories) {
        // Info("run_display", "加载磁场用于粒子轨迹计算...");
        magField = new MagneticField();
        if (magField->LoadFromROOTFile("/home/tian/workspace/dpol/smsimulator5.5/d_work/magnetic_field_test.root")) {
            magField->SetRotationAngle(30.0); // 设置磁铁旋转角度
            // Info("run_display", "磁场加载成功，旋转角度: 30度");
        } else {
            Warning("run_display", "磁场文件加载失败，将不显示粒子轨迹");
            delete magField;
            magField = nullptr;
            show_trajectories = false;
        }
    }

    // 3. 定位到指定事件，执行重建，然后显示
    if (reader.GoToEvent(event_id)) {
        // 获取原始 hits 并重建
        TClonesArray* hits = reader.GetHits();
        RecoEvent event = ana.ProcessEvent(hits);
        
        // 显示事件和粒子轨迹
        if (show_trajectories && magField) {
            Info("run_display", "显示事件 %lld 及入射粒子轨迹", event_id);
            display.DisplayEventWithTrajectories(reader, event, magField);
        } else {
            Info("run_display", "显示事件 %lld (无轨迹)", event_id);
            display.DisplayEvent(event);
        }

        // [EN] Optional overlay from step-scan results. / [CN] 可选叠加步长扫描结果。
        if (overlay_file && std::strlen(overlay_file) > 0) {
            TFile overlay(overlay_file, "READ");
            if (!overlay.IsZombie()) {
                TTree* trajTree = (TTree*)overlay.Get("traj");
                TTree* hitTree = (TTree*)overlay.Get("step_scan");

                if (trajTree) {
                    double stepSize = 0.0;
                    Long64_t evt = -1;
                    int pid = -1;
                    std::vector<double>* x = nullptr;
                    std::vector<double>* y = nullptr;
                    std::vector<double>* z = nullptr;

                    trajTree->SetBranchAddress("step_size", &stepSize);
                    trajTree->SetBranchAddress("event_id", &evt);
                    trajTree->SetBranchAddress("particle_id", &pid);
                    trajTree->SetBranchAddress("x", &x);
                    trajTree->SetBranchAddress("y", &y);
                    trajTree->SetBranchAddress("z", &z);

                    std::vector<int> colors = {kRed, kBlue, kGreen+2, kMagenta+2, kOrange+7, kCyan+2};
                    int colorIndex = 0;

                    Long64_t n = trajTree->GetEntries();
                    for (Long64_t i = 0; i < n; ++i) {
                        trajTree->GetEntry(i);
                        if (evt != event_id) continue;
                        if (!x || !y || !z) continue;
                        if (x->size() < 2) continue;

                        std::vector<TrajectoryPoint> traj;
                        traj.reserve(x->size());
                        for (size_t ip = 0; ip < x->size(); ++ip) {
                            TrajectoryPoint pt;
                            pt.position.SetXYZ((*x)[ip], (*y)[ip], (*z)[ip]);
                            traj.push_back(pt);
                        }
                        int color = colors[colorIndex % colors.size()];
                        colorIndex++;
                        TString name = Form("Step_%.1fmm_pid%d", stepSize, pid);
                        display.DrawTrajectory(traj, name.Data(), color, 1);
                    }
                }

                if (hitTree) {
                    double stepSize = 0.0;
                    Long64_t evt = -1;
                    double hx = 0.0, hy = 0.0, hz = 0.0;
                    hitTree->SetBranchAddress("step_size", &stepSize);
                    hitTree->SetBranchAddress("event_id", &evt);
                    hitTree->SetBranchAddress("hit_x", &hx);
                    hitTree->SetBranchAddress("hit_y", &hy);
                    hitTree->SetBranchAddress("hit_z", &hz);

                    TEvePointSet* pdcHits = new TEvePointSet("PDC_Hits_Overlay");
                    pdcHits->SetMarkerColor(kYellow+2);
                    pdcHits->SetMarkerStyle(20);
                    pdcHits->SetMarkerSize(2.0);

                    Long64_t n = hitTree->GetEntries();
                    for (Long64_t i = 0; i < n; ++i) {
                        hitTree->GetEntry(i);
                        if (evt != event_id) continue;
                        pdcHits->SetNextPoint(hx, hy, hz);
                    }

                    if (display.GetCurrentEventElements()) {
                        display.GetCurrentEventElements()->AddElement(pdcHits);
                    }
                    display.Redraw();
                }
            }
        }
        
        // 打印事件信息
        Info("run_display", "事件 %lld: %zu 个原始hit, %zu 个重建hit, %zu 个重建轨迹", 
             event_id, event.rawHits.size(), event.smearedHits.size(), event.tracks.size());
    } else {
        Error("run_display", "无法定位到事件 %lld", event_id);
    }

    // // 清理磁场对象
    // if (magField) {
    //     delete magField;
    // }

    // 4. 保持3D显示窗口打开，但不启动事件循环
    // 注释掉 gApplication->Run() 来避免循环执行
    // gApplication->Run();
    
    // 替代方案：只是等待用户交互
    Info("run_display", "3D显示已准备完成。窗口将保持打开状态。");
    Info("run_display", "要关闭程序，请在ROOT命令行输入 .q 或关闭窗口。");
}

// 自动调用函数 - 这样可以直接运行 root -l macros/run_display.C
// 如果想显示不同的事件，可以用: root -l 'macros/run_display.C(5)'
// 如果不想显示轨迹，可以用: root -l 'macros/run_display.C(0, false)'
void run_display_auto() {
    run_display(0, true);  // 默认显示第 0 号事件，包含粒子轨迹
}

// 辅助函数：只显示事件，不显示轨迹
void run_display_no_trajectory(Long64_t event_id = 0) {
    run_display(event_id, false);
}

// 辅助函数：显示事件和轨迹
void run_display_with_trajectory(Long64_t event_id = 0) {
    run_display(event_id, true);
}
