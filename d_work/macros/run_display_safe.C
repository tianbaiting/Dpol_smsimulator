#include "TSystem.h"
#include "TApplication.h"

// 包含我们所有自定义类的头文件
#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "EventDisplay.hh"
#include "RecoEvent.hh"
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include "TargetReconstructor.hh"

// 主函数 - 安全版本，不会陷入循环
void run_display_safe(Long64_t event_id = 0, bool show_trajectories = true, bool interactive = false) {
    // 1. 加载我们编译好的库
    static bool loaded = false;
    if (!loaded) {
        if (gSystem->Load("libPDCAnalysisTools.so") < 0) {
            Error("run_display_safe", "无法加载 libPDCAnalysisTools.so 库");
            return;
        }
        loaded = true;
        Info("run_display_safe", "库已成功加载");
    }

    // 2. 创建并设置所有分析工具
    GeometryManager geo;
    geo.LoadGeometry("/home/tian/workspace/dpol/smsimulator5.5/d_work/geometry/5deg_1.2T.mac");

    PDCSimAna ana(geo);
    ana.SetSmearing(0.5, 0.5);

    EventDataReader reader("/home/tian/workspace/dpol/smsimulator5.5/d_work/output_tree/testry0000.root");
    if (!reader.IsOpen()) {
        Error("run_display_safe", "无法打开数据文件!");
        return;
    }

    // 使用静态持久对象，防止函数返回时被析构（这样EVE显示保持）
    static EventDisplay* s_display = nullptr;
    if (!s_display) {
        s_display = new EventDisplay("/home/tian/workspace/dpol/smsimulator5.5/d_work/detector_geometry.gdml", geo);
    }
    EventDisplay* display = s_display;

    // 加载磁场用于轨迹计算，使用静态持久对象
    static MagneticField* s_magField = nullptr;
    static bool s_magLoaded = false;
    MagneticField* magField = nullptr;
    if (show_trajectories) {
        if (!s_magField) s_magField = new MagneticField();
        if (!s_magLoaded) {
            // 使用真实磁场文件
            if (s_magField->LoadFieldMap("/home/tian/workspace/dpol/smsimulator5.5/d_work/geometry/filed_map/180626-1,20T-3000.table")) {
                s_magField->SetRotationAngle(30.0);
                s_magLoaded = true;
                Info("run_display_safe", "已加载真实磁场文件: 180626-1,20T-3000.table");
            } else {
                Warning("run_display_safe", "真实磁场文件加载失败，将不显示粒子轨迹");
                // do not delete s_magField here to keep it available for retry
                s_magLoaded = false;
            }
        }
        if (s_magLoaded) magField = s_magField;
        else show_trajectories = false;
    }

    // 3. 定位到指定事件，执行重建，然后显示
    if (reader.GoToEvent(event_id)) {
        TClonesArray* hits = reader.GetHits();
        RecoEvent event = ana.ProcessEvent(hits);
        event.eventID = event_id; // 确保显示正确的事件号
        
        // 显示事件和粒子轨迹
        if (show_trajectories && magField) {
            Info("run_display_safe", "显示事件 %lld 及入射粒子轨迹", event_id);
            // 传入 magField，并允许显示中性粒子轨迹（DisplayEventWithTrajectories 将绘制中性粒子直线）
            display->DisplayEventWithTrajectories(reader, event, magField);
        } else {
            Info("run_display_safe", "显示事件 %lld (无轨迹)", event_id);
            display->DisplayEvent(event);
        }
        
        // 打印事件信息
        Info("run_display_safe", "事件 %lld: %zu 个原始hit, %zu 个重建hit, %zu 个重建轨迹", 
             event_id, event.rawHits.size(), event.smearedHits.size(), event.tracks.size());

        // 对每条重建轨迹进行靶点重建（解耦版本）
        if (!event.tracks.empty() && magField && display) {
            TargetReconstructor targetRecon(magField);
            TVector3 targetPos(0, 0, 0); // 假设靶点在原点
            
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
            
            for (size_t i = 0; i < event.tracks.size(); ++i) {
                const RecoTrack& track = event.tracks[i];
                
                // 步骤1: 纯计算重建（高效，用于批处理）
                TLorentzVector pAtTarget = targetRecon.ReconstructAtTarget(track, targetPos, 100.0, 2000.0, 2.0, 3);
                
                // 步骤2: 如果需要可视化，单独获取详细结果（包含轨迹数据）
                bool showVisualization = true; // 可根据需要控制
                if (showVisualization) {
                    TargetReconstructionResult detailedResult = targetRecon.ReconstructAtTargetWithDetails(
                        track, targetPos, 
                        true, // saveTrajectories = true（用于可视化）
                        100.0, 2000.0, 2.0, 3);
                    
                    // 步骤3: 让EventDisplay负责可视化
                    bool showTrials = false; // 可设为true显示试探轨迹
                    display->DrawReconstructionResults(detailedResult, showTrials);
                }
                
                Info("run_display_safe", "轨迹 %zu -> 靶点动量: |p|=%.1f MeV/c, px=%.1f, py=%.1f, pz=%.1f", 
                     i, pAtTarget.Vect().Mag(), pAtTarget.Px(), pAtTarget.Py(), pAtTarget.Pz());
            }
            
            // 强制重绘以显示新添加的轨迹
            display->Redraw();
        }
    } else {
        Error("run_display_safe", "无法定位到事件 %lld", event_id);
    }

    // 注意：s_display 与 s_magField 为静态持久对象，故不在此处删除，以保证显示内容在宏返回后仍然可见
    // 如果你希望在退出后释放内存，可以手动 delete s_display 和 s_magField（并将相应静态变量置空）。

    // 4. 根据参数决定是否启动交互模式
    if (interactive) {
        Info("run_display_safe", "启动交互模式...");
        gApplication->Run();
    } else {
        Info("run_display_safe", "3D显示已完成。窗口保持打开状态。");
        Info("run_display_safe", "提示：输入 .q 退出ROOT，或使用 run_display_safe(event_id, show_traj, true) 进入交互模式");
    }
}

// 便捷函数：交互模式
void run_display_interactive(Long64_t event_id = 0, bool show_trajectories = true) {
    run_display_safe(event_id, show_trajectories, true);
}

// 便捷函数：快速显示模式
void run_display_quick(Long64_t event_id = 0, bool show_trajectories = true) {
    run_display_safe(event_id, show_trajectories, false);
}

// 自动调用函数
void run_display_safe_auto() {
    run_display_safe(0, true, false);  // 默认：显示事件0，包含轨迹，非交互模式
}