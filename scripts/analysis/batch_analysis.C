//  批量分析宏，用于处理多个输入ROOT文件，进行PDC和NEBULA重建，并保存结果
// this is to analyze, reconstruct PDC and NEBULA data from multiple input ROOT files
// reconstrucr proton momentum at target and compare with original Geant4 data if available

#include "TSystem.h"
#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include <cstdlib>
#include <string>
#include <iostream>
#include <vector>

#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "RecoEvent.hh"
#include "NEBULAReconstructor.hh"
#include "TBeamSimData.hh"
#include "MagneticField.hh"
#include "TargetReconstructor.hh"

// 函数声明
void batch_analysis_single_file(const char* inputFile, const char* outputFile);
std::vector<TString> GetROOTFilesRecursive(const TString& directory);
TString GetParentDirName(const TString& filepath);
TString GetBaseName(const TString& filepath);

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

// 递归获取目录下所有ROOT文件的函数
std::vector<TString> GetROOTFilesRecursive(const TString& directory) {
    std::vector<TString> files;
    
    void* dirptr = gSystem->OpenDirectory(directory.Data());
    if (!dirptr) {
        std::cerr << "错误: 无法打开目录 " << directory << std::endl;
        return files;
    }
    
    const char* entry;
    while ((entry = gSystem->GetDirEntry(dirptr))) {
        TString entryName(entry);
        if (entryName == "." || entryName == "..") continue;
        
        TString fullPath = directory + "/" + entryName;
        
        // 检查是否是目录
        Long_t id, size, flags, modtime;
        if (gSystem->GetPathInfo(fullPath.Data(), &id, &size, &flags, &modtime) == 0) {
            if (flags & 2) { // 是目录
                // 递归搜索子目录
                std::vector<TString> subFiles = GetROOTFilesRecursive(fullPath);
                files.insert(files.end(), subFiles.begin(), subFiles.end());
            } else if (entryName.EndsWith(".root")) {
                // 是ROOT文件
                files.push_back(fullPath);
            }
        }
    }
    gSystem->FreeDirectory(dirptr);
    
    std::sort(files.begin(), files.end()); // 按文件名排序
    return files;
}

// 从文件路径提取基础文件名（不含路径和扩展名）
TString GetBaseName(const TString& filePath) {
    TString basename = gSystem->BaseName(filePath.Data());
    if (basename.EndsWith(".root")) {
        basename.Remove(basename.Length() - 5);
    }
    return basename;
}

// 从文件路径提取父目录名（用于组织输出）
TString GetParentDirName(const TString& filePath) {
    TString dirname = gSystem->DirName(filePath.Data());
    return gSystem->BaseName(dirname.Data());
}

// 批量处理所有文件的主函数
void batch_analysis_all() {
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir) {
        std::cerr << "错误: 环境变量 SMSIMDIR 未设置" << std::endl;
        return;
    }
    
    // 输入目录
    TString inputDir = Form("%s/d_work/output_tree/ypol_slect_rotate_back", smsDir);
    
    // 创建输出目录
    TString outputDir = Form("%s/d_work/reconp_originnp", smsDir);
    if (gSystem->mkdir(outputDir.Data(), kTRUE) != 0) {
        std::cout << "输出目录已存在或创建成功: " << outputDir << std::endl;
    }
    
    // 获取所有ROOT文件（递归搜索）
    std::vector<TString> inputFiles = GetROOTFilesRecursive(inputDir);
    
    if (inputFiles.empty()) {
        std::cerr << "错误: 在目录 " << inputDir << " 中没有找到ROOT文件" << std::endl;
        return;
    }
    
    std::cout << "找到 " << inputFiles.size() << " 个输入文件:" << std::endl;
    for (const auto& file : inputFiles) {
        std::cout << "  " << file << std::endl;
    }
    
    // 遍历每个文件
    for (const auto& inputFile : inputFiles) {
        TString parentDir = GetParentDirName(inputFile); // e.g., "Pb208_g050"
        TString basename = GetBaseName(inputFile);       // e.g., "ypol_np_Pb208_g0500000"
        TString outputFile = outputDir + "/" + parentDir + "_" + basename + "_reco.root";
        
        std::cout << "\n=== 处理文件: " << inputFile << " ===" << std::endl;
        std::cout << "父目录: " << parentDir << std::endl;
        std::cout << "文件名: " << basename << std::endl;
        std::cout << "输出到: " << outputFile << std::endl;
        
        // 调用单文件处理函数
        batch_analysis_single_file(inputFile.Data(), outputFile.Data());
    }
    
    std::cout << "\n=== 批量处理完成 ===" << std::endl;
    std::cout << "所有结果保存在目录: " << outputDir << std::endl;
}

// 单文件处理函数（修改后的batch_analysis）
void batch_analysis_single_file(const char* inputFile, const char* outputFile) {
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
    std::string inputPath = inputFile ? std::string(inputFile) : 
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
    
    // 4. 初始化磁场和质子动量重建器（使用智能加载）
    MagneticField* magField = new MagneticField();
    TargetReconstructor* targetRecon = nullptr;
    bool magFieldLoaded = false;
    
    // 智能磁场文件加载逻辑：优先使用ROOT格式，没有则用table格式并转换
    TString rootFieldFile = Form("%s/d_work/geometry/filed_map/180626-1,20T-3000.root", smsBase.c_str());
    TString tableFieldFile = Form("%s/d_work/geometry/filed_map/180626-1,20T-3000.table", smsBase.c_str());
    
    // 检查是否存在ROOT格式磁场文件
    if (gSystem->AccessPathName(rootFieldFile.Data()) == 0) {
        std::cout << "发现ROOT格式磁场文件，正在加载: " << rootFieldFile << std::endl;
        if (magField->LoadFromROOTFile(rootFieldFile.Data())) {
            magField->SetRotationAngle(30.0);
            targetRecon = new TargetReconstructor(magField);
            magFieldLoaded = true;
            std::cout << "ROOT格式磁场文件加载成功，将使用TMinuit进行质子动量重建" << std::endl;
        } else {
            std::cout << "ROOT格式磁场文件加载失败，尝试table格式" << std::endl;
        }
    }
    
    // 如果ROOT格式加载失败或不存在，尝试table格式
    if (!magFieldLoaded && gSystem->AccessPathName(tableFieldFile.Data()) == 0) {
        std::cout << "加载table格式磁场文件: " << tableFieldFile << std::endl;
        if (magField->LoadFieldMap(tableFieldFile.Data())) {
            magField->SetRotationAngle(30.0);
            targetRecon = new TargetReconstructor(magField);
            magFieldLoaded = true;
            std::cout << "table格式磁场文件加载成功，将使用TMinuit进行质子动量重建" << std::endl;
            
            // 将table格式转换并保存为ROOT格式以便下次快速加载
            std::cout << "正在将磁场数据保存为ROOT格式: " << rootFieldFile << std::endl;
            try {
                magField->SaveAsROOTFile(rootFieldFile.Data());
                std::cout << "磁场数据已成功保存为ROOT格式，下次将直接使用" << std::endl;
            } catch (...) {
                std::cout << "保存ROOT格式磁场文件失败，但不影响当前使用" << std::endl;
            }
        } else {
            std::cout << "table格式磁场文件加载失败" << std::endl;
        }
    }
    
    if (!magFieldLoaded) {
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
            
            // PDC质子动量重建（在靶点处）- 使用TMinuit优化算法
            if (magFieldLoaded && targetRecon && !recoEvent->tracks.empty()) {
                TVector3 targetPos = geo.GetTargetPosition();
                
                for (size_t j = 0; j < recoEvent->tracks.size(); ++j) {
                    const RecoTrack& track = recoEvent->tracks[j];
                    
                    // 使用TMinuit方法进行质子动量重建 - 更快更精确
                    TargetReconstructionResult result = targetRecon->ReconstructAtTargetMinuit(
                        track, targetPos, 
                        false,  // 不保存轨迹数据（批处理模式）
                        1000.0, // 初始动量猜测 MeV/c
                        1.0,    // 容差 mm
                        500     // 最大迭代次数
                    );
                    
                    // 保存重建的质子动量
                    if (result.success && result.bestMomentum.Vect().Mag() > 0) {
                        originalData->reconstructedProtonMomenta.push_back(result.bestMomentum);
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

// 保持原有函数接口的兼容性
void batch_analysis(const char* inputFile = nullptr,
                    const char* outputFile = "reco_output.root") {
    if (inputFile == nullptr || strlen(inputFile) == 0) {
        // 如果没有指定输入文件，执行批量处理
        batch_analysis_all();
    } else {
        // 如果指定了输入文件，处理单个文件
        batch_analysis_single_file(inputFile, outputFile);
    }
}