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
#include "NEBULAReconstructor.hh"

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
    const char* smsDir = getenv("SMSIMDIR"); // 确保环境变量已加载
    if (!smsDir) {
        Error("run_display_safe", "环境变量 SMSIMDIR 未设置!");
        return;
    }
    // 2. 创建并设置所有分析工具
    GeometryManager geo;
    geo.LoadGeometry(Form("%s/d_work/geometry/5deg_1.2T.mac", smsDir));

    PDCSimAna ana(geo);
    ana.SetSmearing(0.5, 0.5); // 不

    // 使用事件ID构建完整文件路径
    TString filename = Form("/home/tian/workspace/dpol/smsimulator5.5/d_work/output_tree/ypol_slect_rotate_back/Pb208_g050/ypol_np_Pb208_g0500000.root");
    // TString filename = Form("%s/d_work/output_tree/testry0000.root", smsDir);
    // TString filename = Form("%s/d_work/output_tree/test/Pb208_g050/ypol_np_Pb208_g0500000.root", smsDir);

    // TString filename = Form("%s/d_work/output_tree/ypol_np_Pb208_g0500000.root", smsDir);


    
    // 创建EventDataReader对象
    EventDataReader reader(filename.Data());
    if (!reader.IsOpen()) {
        Error("run_display_safe", "无法打开数据文件!");
        return;
    }

    // 使用静态持久对象，防止函数返回时被析构（这样EVE显示保持）
    static EventDisplay* s_display = nullptr;
    if (!s_display) {
        s_display = new EventDisplay(Form("%s/d_work/detector_geometry.gdml", smsDir), geo);
    }
    EventDisplay* display = s_display;



    // 加载磁场用于轨迹计算，使用静态持久对象
    static MagneticField* s_magField = nullptr;
    static bool s_magLoaded = false;
    MagneticField* magField = nullptr;
    if (show_trajectories) {
        if (!s_magField) s_magField = new MagneticField();
        if (!s_magLoaded) {
            // 智能磁场文件加载逻辑：优先使用ROOT格式，没有则用table格式并转换
            TString rootFieldFile = Form("%s/d_work/geometry/filed_map/180626-1,20T-3000.root", smsDir);
            TString tableFieldFile = Form("%s/d_work/geometry/filed_map/180626-1,20T-3000.table", smsDir);
            
            bool loadSuccess = false;
            
            // 检查是否存在ROOT格式磁场文件
            if (gSystem->AccessPathName(rootFieldFile.Data()) == 0) {
                // ROOT文件存在，直接加载
                Info("run_display_safe", "发现ROOT格式磁场文件，正在加载: %s", rootFieldFile.Data());
                if (s_magField->LoadFromROOTFile(rootFieldFile.Data())) {
                    s_magField->SetRotationAngle(30.0);
                    s_magLoaded = true;
                    loadSuccess = true;
                    Info("run_display_safe", "ROOT格式磁场文件加载成功");
                } else {
                    Warning("run_display_safe", "ROOT格式磁场文件加载失败，尝试table格式");
                }
            }
            
            // 如果ROOT格式加载失败或不存在，尝试table格式
            if (!loadSuccess && gSystem->AccessPathName(tableFieldFile.Data()) == 0) {
                Info("run_display_safe", "加载table格式磁场文件: %s", tableFieldFile.Data());
                if (s_magField->LoadFieldMap(tableFieldFile.Data())) {
                    s_magField->SetRotationAngle(30.0);
                    s_magLoaded = true;
                    loadSuccess = true;
                    Info("run_display_safe", "table格式磁场文件加载成功");
                    
                    // 将table格式转换并保存为ROOT格式以便下次快速加载
                    Info("run_display_safe", "正在将磁场数据保存为ROOT格式: %s", rootFieldFile.Data());
                    try {
                        s_magField->SaveAsROOTFile(rootFieldFile.Data());
                        Info("run_display_safe", "磁场数据已成功保存为ROOT格式，下次将直接使用");
                    } catch (...) {
                        Warning("run_display_safe", "保存ROOT格式磁场文件失败，但不影响当前使用");
                    }
                } else {
                    Warning("run_display_safe", "table格式磁场文件加载失败");
                }
            }
            
            // 如果两种格式都加载失败
            if (!loadSuccess) {
                Warning("run_display_safe", "无法加载任何磁场文件 (尝试了 %s 和 %s)，将不显示粒子轨迹", 
                       rootFieldFile.Data(), tableFieldFile.Data());
                s_magLoaded = false;
            }
        }
        if (s_magLoaded) magField = s_magField;
        else show_trajectories = false;
    }

    // 3. 定位到指定事件，执行重建，然后显示
    if (reader.GoToEvent(event_id)) {
        TClonesArray* hits = reader.GetHits();
        TClonesArray* nebulaHits = reader.GetNEBULAHits(); // 获取NEBULA数据
        
        // PDC重建
        RecoEvent event = ana.ProcessEvent(hits);
        event.eventID = event_id; // 确保显示正确的事件号
        
        // NEBULA中子重建
        static NEBULAReconstructor* s_nebulaRecon = nullptr;
        if (!s_nebulaRecon) {
            s_nebulaRecon = new NEBULAReconstructor(geo);
            s_nebulaRecon->SetTargetPosition(geo.GetTargetPosition());
            s_nebulaRecon->SetTimeWindow(10.0);     // 10 ns时间窗口
            s_nebulaRecon->SetEnergyThreshold(1.0); // 1 MeV能量阈值
            Info("run_display_safe", "NEBULA重建器已初始化");
        }
        
        // 处理NEBULA数据
        if (nebulaHits) {
            Info("run_display_safe", "NEBULA数据: %d 个原始hits", nebulaHits->GetEntries());
            s_nebulaRecon->ProcessEvent(nebulaHits, event);
            Info("run_display_safe", "NEBULA重建完成，发现 %zu 个中子", event.neutrons.size());
        } else {
            Info("run_display_safe", "NEBULA数据为空或未找到");
        }
        
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
            
            // 从 GeometryManager 获取 Target 位置，而不是假设在原点
            TVector3 targetPos = geo.GetTargetPosition();
            
            Info("run_display_safe", "使用 Target 位置: (%.2f, %.2f, %.2f) mm, 角度: %.2f deg", 
                 targetPos.X(), targetPos.Y(), targetPos.Z(), geo.GetTargetAngleDeg());
            
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
                
                // 步骤1: 使用TMinuit方法进行高精度重建（替代原始的网格搜索）
                TargetReconstructionResult minuitResult = targetRecon.ReconstructAtTargetMinuit(
                    track, targetPos, 
                    false,  // saveTrajectories = false（纯计算模式）
                    700.0, // 初始动量猜测 MeV/c
                    100.0,    // 容差 mm
                    5000     // 最大迭代次数
                );
                
                TLorentzVector pAtTarget = minuitResult.bestMomentum;
                
                // 步骤2: 如果需要可视化，单独获取详细结果（包含轨迹数据）
                bool showVisualization = true; // 可根据需要控制
                if (showVisualization) {
                    TargetReconstructionResult detailedResult = targetRecon.ReconstructAtTargetMinuit(
                        track, targetPos, 
                        true,   // saveTrajectories = true（用于可视化）
                        700, // 初始动量猜测 MeV/c
                        100.0,    // 容差 mm
                        5000     // 最大迭代次数
                    );
                    
                    // 步骤3: 让EventDisplay负责可视化
                    bool showTrials = false; // 可设为true显示试探轨迹
                    display->DrawReconstructionResults(detailedResult, showTrials);
                }
                
                Info("run_display_safe", "轨迹 %zu -> 靶点动量 (TMinuit): |p|=%.1f MeV/c, px=%.1f, py=%.1f, pz=%.1f, 重建成功: %s, 最终距离: %.2f mm", 
                     i, pAtTarget.Vect().Mag(), pAtTarget.Px(), pAtTarget.Py(), pAtTarget.Pz(),
                     minuitResult.success ? "是" : "否", minuitResult.finalDistance);
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