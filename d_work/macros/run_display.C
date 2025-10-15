#include "TSystem.h"
#include "TApplication.h"
#include "TEveManager.h"
#include "TGeoManager.h"

// 包含我们所有自定义类的头文件
#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "EventDisplay.hh"

// 主函数
void run_display(Long64_t event_id = 0) {
    // 1. 加载我们编译好的库
    gSystem->Load("libPDCAnalysisTools.so");

    // 几何配置文件路径
    const char* geomFile = "/home/tbt/workspace/dpol/smsimulator5.5/d_work/geometry/5deg_1.2T.mac";
    
    // 2. 创建并设置所有分析工具 (对象在栈上创建，函数结束时自动销毁)
    GeometryManager geo;
    geo.LoadGeometry(geomFile);

    PDCSimAna ana(geo);
    ana.SetSmearing(0.5, 0.5); // 设置模糊参数

    // 打开事件数据文件
    EventDataReader reader("/home/tbt/workspace/dpol/smsimulator5.5/d_work/output_tree/test0000.root");
    if (!reader.IsOpen()) {
        std::cout << "无法打开数据文件，请检查路径是否正确！" << std::endl;
        return; // 如果文件打不开，则退出
    }

    // 创建事件显示器
    EventDisplay display("/home/tbt/workspace/dpol/smsimulator5.5/d_work/detector_geometry.gdml", ana);
    
    // 从几何配置文件加载磁场
    std::cout << "正在从几何配置文件加载磁场..." << std::endl;
    if (!display.GetTrackPropagator().LoadMagFieldFromGeomConfig(geomFile)) {
        std::cout << "警告: 无法从几何配置加载磁场，将使用默认均匀磁场 (0, 1.2, 0) Tesla" << std::endl;
        display.GetTrackPropagator().SetMagField(0.0, 1.2, 0.0);
    }
    
    // 3. 定位到指定事件并显示
    if (reader.GoToEvent(event_id)) {
        display.DisplayEvent(reader);
        std::cout << "==== 事件 " << event_id << " 显示完成 ====" << std::endl;
        std::cout << "提示：可以使用鼠标操作视图，左键旋转，中键移动，右键缩放" << std::endl;
        std::cout << "      按住Shift键可以进行更精细的操作" << std::endl;
    } else {
        std::cout << "无法加载事件 " << event_id << "，请检查事件ID是否有效" << std::endl;
    }

    // 改善几何体的显示效果
    if (gGeoManager) {
        // 设置几何体透明度，使内部结构可见
        gGeoManager->SetVisLevel(4);
        gGeoManager->GetTopVolume()->SetTransparency(20);
    }
    
    // 如果有Eve管理器，改善显示效果
    if (gEve) {
        // 强制刷新3D视图
        gEve->Redraw3D(kTRUE);
    }

    // 4. 启动ROOT应用程序事件循环
    // 这会使程序暂停，保持3D显示窗口打开，直到用户手动关闭它
    std::cout << "事件显示器已启动，按Ctrl+C可退出程序" << std::endl;
    gApplication->Run();
}