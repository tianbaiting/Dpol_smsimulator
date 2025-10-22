#include "TSystem.h"
#include "TApplication.h"

// 包含我们所有自定义类的头文件
#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "EventDisplay.hh"
#include "RecoEvent.hh"

// 主函数
void run_display(Long64_t event_id = 0) {
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

    // 3. 定位到指定事件，执行重建，然后显示
    if (reader.GoToEvent(event_id)) {
        // 获取原始 hits 并重建
        TClonesArray* hits = reader.GetHits();
        RecoEvent event = ana.ProcessEvent(hits);
        
        // 将重建结果传给显示模块
        display.DisplayEvent(event);
    }

    // 4. 启动ROOT应用程序事件循环
    // 这会使程序暂停，保持3D显示窗口打开，直到用户手动关闭它
    gApplication->Run();
}

// 自动调用函数 - 这样可以直接运行 root -l macros/run_display.C
// 如果想显示不同的事件，可以用: root -l 'macros/run_display.C(5)'
void run_display_auto() {
    run_display(0);  // 默认显示第 0 号事件
}

