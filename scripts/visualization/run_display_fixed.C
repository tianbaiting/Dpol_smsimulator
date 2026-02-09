// Usage: root -l 'scripts/visualization/run_display_fixed.C(0)'
#include "TSystem.h"
#include "TApplication.h"
#include "TROOT.h"
#include "TStyle.h"
#include "TError.h"
#include "TEveManager.h"
#include <cstdlib>
#include <string>

// 包含我们所有自定义类的头文件
#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "EventDisplay.hh"
#include "RecoEvent.hh"

// 主函数 - 增加错误处理和资源安全管理
void run_display_fixed(Long64_t event_id = 0) {
    // 设置 ROOT 错误处理
    gErrorIgnoreLevel = kWarning; // 只显示警告和错误

    // 设置更加现代的样式
    gStyle->SetOptStat(0);
    
    // 检查是否已加载库
    if (!TClass::GetClass("PDCSimAna")) {
        // 1. 加载我们编译好的库
        if (gSystem->Load("libPDCAnalysisTools.so") < 0) {
            Error("run_display_fixed", "无法加载 libPDCAnalysisTools.so 库");
            return;
        }
    }
    
    try {
        // 2. 创建并设置所有分析工具 (对象在栈上创建，函数结束时自动销毁)
        const char* smsDir = getenv("SMSIMDIR");
        if (!smsDir) {
            Error("run_display_fixed", "环境变量 SMSIMDIR 未设置!");
            return;
        }
        std::string smsBase(smsDir);

        GeometryManager geo;
        if (!geo.LoadGeometry((smsBase + "/d_work/geometry/5deg_1.2T.mac").c_str())) {
            Error("run_display_fixed", "无法加载几何文件");
            return;
        }
        
        // 首先确保清理可能存在的 Eve 管理器
        if (gEve) {
            delete gEve;
            gEve = nullptr;
        }

        PDCSimAna ana(geo);
        ana.SetSmearing(0.5, 0.5); // 设置模糊参数

        EventDataReader reader((smsBase + "/d_work/output_tree/test0000.root").c_str());
        if (!reader.IsOpen()) {
            Error("run_display_fixed", "无法打开数据文件");
            return;
        }

        EventDisplay display((smsBase + "/d_work/detector_geometry.gdml").c_str(), geo);

        // 3. 定位到指定事件，执行重建，然后显示
        if (reader.GoToEvent(event_id)) {
            // 获取原始 hits 并重建
            TClonesArray* hits = reader.GetHits();
            if (!hits) {
                Error("run_display_fixed", "无法获取事件 hits");
                return;
            }
            
            RecoEvent event = ana.ProcessEvent(hits);
            
            // 将重建结果传给显示模块
            display.DisplayEvent(event);
            
            // 强制更新
            if (gEve) {
                gEve->Redraw3D(kTRUE);
                gEve->FullRedraw3D(kTRUE);
            }
        } else {
            Error("run_display_fixed", "无法定位到指定事件 %lld", event_id);
            return;
        }

        Info("run_display_fixed", "显示已完成，进入事件循环");
        // 4. 启动ROOT应用程序事件循环
        // 这会使程序暂停，保持3D显示窗口打开，直到用户手动关闭它
        gApplication->Run(kTRUE);
        
    } catch (const std::exception& e) {
        Error("run_display_fixed", "发生异常: %s", e.what());
    } catch (...) {
        Error("run_display_fixed", "发生未知异常");
    }
}