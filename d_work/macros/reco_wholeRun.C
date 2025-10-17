#include "TSystem.h"
#include "TFile.h"
#include "TTree.h"

#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "RecoEvent.hh"

void reco_wholeRun(const char* inputFile = "/home/tbt/workspace/dpol/smsimulator5.5/d_work/output_tree/test0000.root",
                   const char* outputFile = "reco_output.root") {
    // 1. 加载库
    gSystem->Load("libPDCAnalysisTools.so");
    
    // 2. 设置几何和重建器
    GeometryManager geo;
    geo.LoadGeometry("/home/tbt/workspace/dpol/smsimulator5.5/d_work/geometry/5deg_1.2T.mac");
    
    PDCSimAna ana(geo);
    ana.SetSmearing(0.5, 0.5);
    
    // 3. 打开输入文件
    EventDataReader reader(inputFile);
    if (!reader.IsOpen()) {
        std::cerr << "无法打开输入文件: " << inputFile << std::endl;
        return;
    }
    
    // 4. 创建输出文件和 TTree
    TFile* outFile = new TFile(outputFile, "RECREATE");
    TTree* recoTree = new TTree("recoTree", "重建事件树");
    
    // 5. 准备一个重建事件对象并设置分支
    RecoEvent* event = new RecoEvent();
    recoTree->Branch("recoEvent", &event);
    
    // 用于存储统计信息的变量
    Long64_t totalEvents = reader.GetTotalEvents();
    int processedEvents = 0;
    
    // 6. 处理所有事件
    std::cout << "开始处理 " << totalEvents << " 个事件..." << std::endl;
    
    for (Long64_t i = 0; i < totalEvents; ++i) {
        if (i % 100 == 0) {
            std::cout << "处理事件 " << i << " / " << totalEvents << " (" 
                      << (i * 100.0 / totalEvents) << "%)" << std::endl;
        }
        
        if (reader.GoToEvent(i)) {
            // 获取原始数据
            TClonesArray* hits = reader.GetHits();
            
            // 重建事件（使用输出参数版本，可重用内存）
            event->Clear();  // 清除上一次的结果
            ana.ProcessEvent(hits, *event);  // 填充重建结果
            event->eventID = i;  // 设置事件ID
            
            // 将事件写入树
            recoTree->Fill();
            processedEvents++;
        }
    }
    
    // 7. 写入文件并关闭
    outFile->Write();
    outFile->Close();
    
    std::cout << "分析完成! 处理了 " << processedEvents << " / " << totalEvents << " 个事件" << std::endl;
    std::cout << "结果保存到: " << outputFile << std::endl;
}
