#include "TSystem.h"
#include "TFile.h"
#include "TTree.h"
#include <cstdlib>
#include <string>
#include <iostream>

#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "RecoEvent.hh"
#include "NEBULAReconstructor.hh"
#include "TBeamSimData.hh"
#include "MagneticField.hh"
#include "TargetReconstructor.hh"

// 原始Geant4数据结构，用于保存原始质子和中子动量
struct OriginalParticleData {
    // 质子信息
    bool hasProton;
    TLorentzVector protonMomentum;
    TVector3 protonPosition;
    
    // 中子信息  
    bool hasNeutron;
    TLorentzVector neutronMomentum;
    TVector3 neutronPosition;
    
    // PDC重建的质子动量（在靶点处）
    std::vector<TLorentzVector> reconstructedProtonMomenta;
    
    OriginalParticleData() : hasProton(false), hasNeutron(false), 
                            protonMomentum(0,0,0,0), neutronMomentum(0,0,0,0),
                            protonPosition(0,0,0), neutronPosition(0,0,0) {}
    
    void Clear() {
        hasProton = false;
        hasNeutron = false;
        protonMomentum.SetPxPyPzE(0,0,0,0);
        neutronMomentum.SetPxPyPzE(0,0,0,0);
        protonPosition.SetXYZ(0,0,0);
        neutronPosition.SetXYZ(0,0,0);
        reconstructedProtonMomenta.clear();
    }
};

void batch_analysis(const char* inputFile = nullptr,
                    const char* outputFile = "reco_output.root") {
    // 1. 加载库
    if (gSystem->Load("libPDCAnalysisTools.so") < 0) {
        std::cerr << "错误: 无法加载 libPDCAnalysisTools.so 库" << std::endl;
        return;
    }
    
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir && (inputFile == nullptr || inputFile[0] == '\0')) {
        std::cerr << "错误: 环境变量 SMSIMDIR 未设置且未提供输入文件" << std::endl;
        return;
    }
    
    std::string smsBase = smsDir ? std::string(smsDir) : std::string();
    std::string inputPath = (inputFile && inputFile[0] != '\0') ? 
        std::string(inputFile) : 
        (smsBase + "/d_work/output_tree/ypol_5000events/Pb208_g050/ypol_np_Pb208_g0500000.root");

    // 2. 设置几何和重建器
    GeometryManager geo;
    if (!geo.LoadGeometry((smsBase + "/d_work/geometry/5deg_1.2T.mac").c_str())) {
        std::cerr << "错误: 无法加载几何配置文件" << std::endl;
        return;
    }
    
    PDCSimAna ana(geo);
    ana.SetSmearing(0.5, 0.5);  // 设置位置分辨率
    
    // 3. 初始化NEBULA重建器
    NEBULAReconstructor nebulaRecon(geo);
    nebulaRecon.SetTargetPosition(geo.GetTargetPosition());
    nebulaRecon.SetTimeWindow(10.0);     // 10 ns时间窗口
    nebulaRecon.SetEnergyThreshold(1.0); // 1 MeV能量阈值
    
    // 4. 初始化磁场和质子动量重建器
    MagneticField* magField = new MagneticField();
    TargetReconstructor* targetRecon = nullptr;
    bool magFieldLoaded = false;
    
    // 加载磁场用于质子动量重建
    if (magField->LoadFieldMap((smsBase + "/d_work/geometry/filed_map/180626-1,20T-3000.table").c_str())) {
        magField->SetRotationAngle(30.0);
        targetRecon = new TargetReconstructor(magField);
        magFieldLoaded = true;
        std::cout << "磁场已加载，将进行质子动量重建" << std::endl;
    } else {
        std::cout << "磁场加载失败，跳过质子动量重建" << std::endl;
    }
    
    // 5. 打开输入文件
    EventDataReader reader(inputPath.c_str());
    if (!reader.IsOpen()) {
        std::cerr << "错误: 无法打开输入文件: " << inputPath << std::endl;
        return;
    }
    
    // 6. 创建输出文件和 TTree
    TFile* outFile = new TFile(outputFile, "RECREATE");
    TTree* recoTree = new TTree("recoTree", "重建事件树");
    
    // 7. 准备数据结构并设置分支
    RecoEvent* recoEvent = new RecoEvent();
    OriginalParticleData* originalData = new OriginalParticleData();
    
    recoTree->Branch("recoEvent", &recoEvent);
    recoTree->Branch("originalData", &originalData);
    
    // 用于存储统计信息的变量
    Long64_t totalEvents = reader.GetTotalEvents();
    int processedEvents = 0;
    int pdcHitEvents = 0;
    int nebulaHitEvents = 0;
    int originalDataEvents = 0;
    int protonReconEvents = 0;
    int totalReconProtons = 0;
    
    // 8. 处理所有事件
    std::cout << "开始处理 " << totalEvents << " 个事件..." << std::endl;
    std::cout << "输入文件: " << inputPath << std::endl;
    std::cout << "输出文件: " << outputFile << std::endl;
    
    for (Long64_t i = 0; i < totalEvents; ++i) {
        if (i % 500 == 0) {
            std::cout << "处理事件 " << i << " / " << totalEvents << " (" 
                      << (i * 100.0 / totalEvents) << "%%)" << std::endl;
        }
        
        if (reader.GoToEvent(i)) {
            // 清除上一次的结果
            recoEvent->Clear();
            originalData->Clear();
            
            // 获取原始探测器数据
            TClonesArray* hits = reader.GetHits();
            TClonesArray* nebulaHits = reader.GetNEBULAHits();
            
            // PDC重建
            if (hits && hits->GetEntries() > 0) {
                ana.ProcessEvent(hits, *recoEvent);
                pdcHitEvents++;
            }
            
            // NEBULA重建
            if (nebulaHits && nebulaHits->GetEntries() > 0) {
                nebulaRecon.ProcessEvent(nebulaHits, *recoEvent);
                nebulaHitEvents++;
            }
            
            // PDC质子动量重建（在靶点处）
            if (magFieldLoaded && targetRecon && !recoEvent->tracks.empty()) {
                TVector3 targetPos = geo.GetTargetPosition();
                
                for (size_t j = 0; j < recoEvent->tracks.size(); ++j) {
                    const RecoTrack& track = recoEvent->tracks[j];
                    
                    // 质子动量重建 - 仅计算，无可视化
                    TLorentzVector pAtTarget = targetRecon->ReconstructAtTarget(
                        track, targetPos, 
                        100.0,  // 最小动量 MeV/c
                        2000.0, // 最大动量 MeV/c 
                        2.0,    // 步长 MeV/c
                        3       // 最大迭代次数
                    );
                    
                    // 保存重建的质子动量
                    if (pAtTarget.Vect().Mag() > 0) {
                        originalData->reconstructedProtonMomenta.push_back(pAtTarget);
                        totalReconProtons++;
                    }
                }
                
                // 统计有质子重建的事件
                if (!originalData->reconstructedProtonMomenta.empty()) {
                    protonReconEvents++;
                }
            }
            
            // 获取原始Geant4粒子数据
            const std::vector<TBeamSimData>* beamData = reader.GetBeamData();
            if (beamData && !beamData->empty()) {
                originalDataEvents++;
                
                for (const auto& particle : *beamData) {
                    if (particle.fParticleName == "proton" || 
                        (particle.fZ == 1 && particle.fA == 1)) {
                        originalData->hasProton = true;
                        originalData->protonMomentum = particle.fMomentum;
                        originalData->protonPosition = particle.fPosition;
                    }
                    else if (particle.fParticleName == "neutron" || 
                            (particle.fZ == 0 && particle.fA == 1)) {
                        originalData->hasNeutron = true;
                        originalData->neutronMomentum = particle.fMomentum;
                        originalData->neutronPosition = particle.fPosition;
                    }
                }
            }
            
            // 设置事件ID
            recoEvent->eventID = i;
            
            // 将事件写入树
            recoTree->Fill();
            processedEvents++;
        }
    }
    
    // 9. 写入统计信息到文件
    TNamed info1("InputFile", inputPath.c_str());
    TNamed info2("TotalEvents", Form("%lld", totalEvents));
    TNamed info3("ProcessedEvents", Form("%d", processedEvents));
    TNamed info4("PDCHitEvents", Form("%d", pdcHitEvents));
    TNamed info5("NEBULAHitEvents", Form("%d", nebulaHitEvents));
    TNamed info6("OriginalDataEvents", Form("%d", originalDataEvents));
    TNamed info7("ProtonReconEvents", Form("%d", protonReconEvents));
    TNamed info8("TotalReconProtons", Form("%d", totalReconProtons));
    TNamed info9("MagFieldLoaded", magFieldLoaded ? "true" : "false");
    
    info1.Write();
    info2.Write();
    info3.Write(); 
    info4.Write();
    info5.Write();
    info6.Write();
    info7.Write();
    info8.Write();
    info9.Write();
    
    // 10. 写入并关闭文件
    outFile->Write();
    outFile->Close();
    
    // 11. 打印统计信息
    std::cout << "\n=== 批处理分析完成 ===" << std::endl;
    std::cout << "输入文件: " << inputPath << std::endl;
    std::cout << "输出文件: " << outputFile << std::endl;
    std::cout << "总事件数: " << totalEvents << std::endl;
    std::cout << "处理事件数: " << processedEvents << std::endl;
    std::cout << "PDC击中事件: " << pdcHitEvents << " (" << (pdcHitEvents*100.0/processedEvents) << "%%)" << std::endl;
    std::cout << "NEBULA击中事件: " << nebulaHitEvents << " (" << (nebulaHitEvents*100.0/processedEvents) << "%%)" << std::endl;
    std::cout << "包含原始粒子数据的事件: " << originalDataEvents << " (" << (originalDataEvents*100.0/processedEvents) << "%%)" << std::endl;
    
    if (magFieldLoaded) {
        std::cout << "质子动量重建事件: " << protonReconEvents << " (" << (protonReconEvents*100.0/processedEvents) << "%%)" << std::endl;
        std::cout << "重建质子总数: " << totalReconProtons << std::endl;
        std::cout << "平均每事件重建质子数: " << (protonReconEvents > 0 ? totalReconProtons*1.0/protonReconEvents : 0.0) << std::endl;
    } else {
        std::cout << "磁场未加载，跳过了质子动量重建" << std::endl;
    }
    
    // 清理内存
    delete recoEvent;
    delete originalData;
    if (magField) delete magField;
    if (targetRecon) delete targetRecon;
}