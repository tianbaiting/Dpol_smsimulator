//  批量分析宏，用于处理多个输入ROOT文件，进行PDC和NEBULA重建，并保存结果
// this is to analyze, reconstruct PDC and NEBULA data from multiple input ROOT files
// reconstrucr proton momentum at target and compare with original Geant4 data if available

#include "TSystem.h"
#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "RecoEvent.hh"
#include "NEBULAReconstructor.hh"
#include "TBeamSimData.hh"
#include "MagneticField.hh"
#include "TargetReconstructor.hh"
#include "../../libs/analysis_pdc_reco/include/PDCMomentumReconstructor.hh"
#include "../../libs/analysis_pdc_reco/include/PDCRecoRuntime.hh"

namespace reco = analysis::pdc::anaroot_like;

// 函数声明
void batch_analysis_single_file(const char* inputFile, const char* outputFile);
std::vector<TString> GetROOTFilesRecursive(const TString& directory);
TString GetParentDirName(const TString& filepath);
TString GetBaseName(const TString& filepath);
double ReadEnvDouble(const char* key, double fallback);
reco::RkFitMode ReadEnvRkFitMode(
    const char* key,
    reco::RkFitMode fallback
);

double ReadEnvDouble(const char* key, double fallback) {
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return fallback;
    }
    char* end_ptr = nullptr;
    const double parsed = std::strtod(value, &end_ptr);
    if (!end_ptr || end_ptr == value || *end_ptr != '\0' || !std::isfinite(parsed)) {
        return fallback;
    }
    return parsed;
}

reco::RkFitMode ReadEnvRkFitMode(
    const char* key,
    reco::RkFitMode fallback
) {
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return fallback;
    }
    return reco::ParseRkFitModeOrFallback(std::string(value), fallback);
}

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
    
    const char* inputDirEnv = std::getenv("BATCH_ANALYSIS_INPUT_DIR");
    const char* outputDirEnv = std::getenv("BATCH_ANALYSIS_OUTPUT_DIR");

    // 输入目录
    TString inputDir = inputDirEnv && inputDirEnv[0] != '\0'
        ? TString(inputDirEnv)
        : TString::Format("%s/data/simulation/g4output", smsDir);
    
    // 创建输出目录
    TString outputDir = outputDirEnv && outputDirEnv[0] != '\0'
        ? TString(outputDirEnv)
        : TString::Format("%s/data/reconstruction", smsDir);
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
    const int loadNewReco = gSystem->Load("libanalysis_pdc_reco.so");
    int loadLegacyReco = gSystem->Load("libpdcanalysis.so");
    if (loadLegacyReco < 0) {
        loadLegacyReco = gSystem->Load("libPDCAnalysisTools.so");
    }
    if (loadNewReco < 0 && loadLegacyReco < 0) {
        std::cerr << "错误: 无法加载重建库 (libanalysis_pdc_reco.so / libpdcanalysis.so)" << std::endl;
        return;
    }
    
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir && (inputFile == nullptr || inputFile[0] == '\0')) {
        std::cerr << "错误: 环境变量 SMSIMDIR 未设置且未提供输入文件" << std::endl;
        return;
    }
    
    std::string smsBase = smsDir ? std::string(smsDir) : std::string();
    std::string inputPath = (inputFile && inputFile[0] != '\0')
        ? std::string(inputFile)
        : (smsBase + "/data/simulation/g4output/test/Pb208_g050/ypol_np_Pb208_g050_test0000.root");

    std::string backendMode = "auto";
    if (const char* backend = std::getenv("PDC_RECO_BACKEND")) {
        backendMode = reco::ToLowerCopy(std::string(backend));
        if (backendMode == "anaroot_like") {
            backendMode = "auto";
        }
    }
    reco::RuntimeBackend runtimeBackend = reco::RuntimeBackend::kAuto;
    try {
        runtimeBackend = reco::ParseRuntimeBackend(backendMode);
    } catch (const std::exception& ex) {
        std::cerr << "错误: 无效的 PDC_RECO_BACKEND='" << backendMode
                  << "' (" << ex.what() << ")" << std::endl;
        return;
    }
    bool useAnarootLikeReco = reco::RuntimeBackendUsesNewFramework(runtimeBackend);
    const bool forceNNBackend = (runtimeBackend == reco::RuntimeBackend::kNeuralNetwork);

    // 2. 设置几何和重建器
    GeometryManager geo;
    const char* geometryEnv = std::getenv("BATCH_ANALYSIS_GEOMETRY_MAC");
    std::string geometryMacro = (geometryEnv && geometryEnv[0] != '\0')
        ? std::string(geometryEnv)
        : (smsBase + "/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac");
    if (gSystem->AccessPathName(geometryMacro.c_str()) != 0) {
        geometryMacro = smsBase + "/configs/simulation/geometry/5deg_1.2T.mac";
    }
    if (!reco::LoadGeometryFromMacro(geo, geometryMacro)) {
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
    auto magField = std::make_unique<MagneticField>();
    std::unique_ptr<TargetReconstructor> targetReconLegacy;
    std::unique_ptr<reco::PDCMomentumReconstructor> targetReconAnarootLike;
    bool magFieldLoaded = false;
    bool protonRecoEnabled = false;
    std::string recoBackendLabel = (runtimeBackend == reco::RuntimeBackend::kAuto)
        ? "anaroot_like"
        : reco::RuntimeBackendName(runtimeBackend);
    
    // 智能磁场文件加载逻辑：优先使用ROOT格式，没有则用table格式并转换
    const char* rootFieldEnv = std::getenv("BATCH_ANALYSIS_MAG_ROOT");
    const char* tableFieldEnv = std::getenv("BATCH_ANALYSIS_MAG_TABLE");
    TString rootFieldFile = rootFieldEnv && rootFieldEnv[0] != '\0'
        ? TString(rootFieldEnv)
        : TString::Format("%s/configs/simulation/geometry/filed_map/180703-1,15T-3000.root", smsBase.c_str());
    TString tableFieldFile = tableFieldEnv && tableFieldEnv[0] != '\0'
        ? TString(tableFieldEnv)
        : TString::Format("%s/configs/simulation/geometry/filed_map/180703-1,15T-3000.table", smsBase.c_str());
    if (gSystem->AccessPathName(tableFieldFile.Data()) != 0) {
        tableFieldFile = TString::Format("%s/configs/simulation/geometry/filed_map/180626-1,20T-3000.table", smsBase.c_str());
    }
    if (gSystem->AccessPathName(rootFieldFile.Data()) != 0) {
        rootFieldFile = TString::Format("%s/configs/simulation/geometry/filed_map/180626-1,20T-3000.root", smsBase.c_str());
    }
    const double magRotationDeg = ReadEnvDouble("BATCH_ANALYSIS_MAG_ROT_DEG", 30.0);
    
    const reco::MagneticFieldLoadResult fieldLoad = reco::LoadMagneticFieldWithFallback(
        *magField,
        rootFieldFile.Data(),
        tableFieldFile.Data(),
        magRotationDeg,
        true
    );
    magFieldLoaded = fieldLoad.success;
    if (fieldLoad.loaded_from_root) {
        std::cout << "发现ROOT格式磁场文件，正在加载: " << fieldLoad.loaded_path << std::endl;
        std::cout << "ROOT格式磁场文件加载成功" << std::endl;
    } else if (fieldLoad.loaded_from_table) {
        std::cout << "加载table格式磁场文件: " << fieldLoad.loaded_path << std::endl;
        std::cout << "table格式磁场文件加载成功" << std::endl;
    }
    
    if (!magFieldLoaded && !forceNNBackend) {
        std::cout << "磁场加载失败，将跳过 legacy 质子动量重建" << std::endl;
    }

    if (useAnarootLikeReco && loadNewReco < 0) {
        if (forceNNBackend) {
            std::cerr << "错误: NN后端请求失败，libanalysis_pdc_reco.so 未加载成功" << std::endl;
            return;
        }
        std::cout << "未加载到 libanalysis_pdc_reco.so，自动回退到 legacy 后端" << std::endl;
        useAnarootLikeReco = false;
        recoBackendLabel = "legacy";
    }

    if (useAnarootLikeReco) {
        // [EN] NN mode can run without field map because model directly predicts target momentum from two PDC points. / [CN] NN模式可在无磁场图时运行，因为模型直接由两个PDC点预测靶点动量。
        targetReconAnarootLike = std::make_unique<reco::PDCMomentumReconstructor>(magFieldLoaded ? magField.get() : nullptr);
        protonRecoEnabled = true;
        if (forceNNBackend) {
            recoBackendLabel = "nn";
        } else {
            recoBackendLabel = (runtimeBackend == reco::RuntimeBackend::kAuto)
                ? "anaroot_like"
                : reco::RuntimeBackendName(runtimeBackend);
        }
        std::cout << "质子动量重建后端: " << recoBackendLabel << std::endl;
    } else if (magFieldLoaded) {
        targetReconLegacy = std::make_unique<TargetReconstructor>(magField.get());
        protonRecoEnabled = true;
        recoBackendLabel = "legacy";
        std::cout << "质子动量重建后端: legacy TargetReconstructor" << std::endl;
    } else {
        std::cout << "未启用可用的质子重建后端（磁场加载失败且未启用NN）" << std::endl;
    }

    if (forceNNBackend && !magFieldLoaded) {
        std::cout << "NN后端运行中：磁场文件缺失不影响NN动量解算" << std::endl;
    }
    if (forceNNBackend && !std::getenv("PDC_NN_MODEL_JSON")) {
        std::cerr << "警告: PDC_NN_MODEL_JSON 未设置，NN重建将返回不可用" << std::endl;
    }

    if (!protonRecoEnabled) {
        std::cout << "当前配置下将跳过质子动量重建" << std::endl;
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

    const char* nnModelEnv = std::getenv("PDC_NN_MODEL_JSON");
    const std::string nnModelJsonPath = (nnModelEnv && nnModelEnv[0] != '\0') ? std::string(nnModelEnv) : std::string();
    const double recoPMin = ReadEnvDouble("PDC_RECO_P_MIN_MEVC", 50.0);
    const double recoPMax = ReadEnvDouble("PDC_RECO_P_MAX_MEVC", 5000.0);
    const double recoToleranceMM = ReadEnvDouble("PDC_RECO_TOLERANCE_MM", 5.0);
    const double recoRKStepMM = ReadEnvDouble("PDC_RECO_RK_STEP_MM", 5.0);
    const reco::RkFitMode recoRKMode =
        ReadEnvRkFitMode("PDC_RECO_RK_MODE", reco::RkFitMode::kThreePointFree);
    reco::RuntimeOptions runtimeOptions;
    runtimeOptions.backend = runtimeBackend;
    runtimeOptions.p_min_mevc = recoPMin;
    runtimeOptions.p_max_mevc = recoPMax;
    runtimeOptions.tolerance_mm = recoToleranceMM;
    runtimeOptions.max_iterations = 50;
    runtimeOptions.rk_step_mm = recoRKStepMM;
    runtimeOptions.pdc_sigma_mm = 2.0;
    runtimeOptions.target_sigma_xy_mm = 5.0;
    runtimeOptions.nn_model_json_path = nnModelJsonPath;
    runtimeOptions.rk_fit_mode = recoRKMode;
    const reco::RecoConfig recoConfig = reco::BuildRecoConfig(runtimeOptions, magFieldLoaded);
    const reco::TargetConstraint targetConstraint = reco::BuildTargetConstraint(geo, runtimeOptions);
    
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
            if (protonRecoEnabled && !recoEvent->tracks.empty()) {
                TVector3 targetPos = geo.GetTargetPosition();
                
                for (size_t j = 0; j < recoEvent->tracks.size(); ++j) {
                    const RecoTrack& track = recoEvent->tracks[j];

                    TLorentzVector recoMomentum(0.0, 0.0, 0.0, 0.0);
                    bool recoSuccess = false;

                    if (useAnarootLikeReco && targetReconAnarootLike) {
                        reco::PDCInputTrack pdcInput;
                        pdcInput.pdc1 = track.start;
                        pdcInput.pdc2 = track.end;

                        const reco::RecoResult recoResult =
                            targetReconAnarootLike->Reconstruct(pdcInput, targetConstraint, recoConfig);
                        if ((recoResult.status == reco::SolverStatus::kSuccess ||
                             recoResult.status == reco::SolverStatus::kNotConverged) &&
                            recoResult.p4_at_target.P() > 0.0) {
                            recoMomentum = recoResult.p4_at_target;
                            recoSuccess = true;
                        }
                    } else if (targetReconLegacy) {
                        // 使用TMinuit方法进行质子动量重建 - 更快更精确
                        TargetReconstructionResult result = targetReconLegacy->ReconstructAtTargetMinuit(
                            track, targetPos,
                            false,  // 不保存轨迹数据（批处理模式）
                            1000.0, // 初始动量猜测 MeV/c
                            1.0,    // 容差 mm
                            500     // 最大迭代次数
                        );

                        if (result.success && result.bestMomentum.Vect().Mag() > 0) {
                            recoMomentum = result.bestMomentum;
                            recoSuccess = true;
                        }
                    }

                    // 保存重建的质子动量
                    if (recoSuccess) {
                        originalData->reconstructedProtonMomenta.push_back(recoMomentum);
                        ++totalReconProtons;
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
    TNamed info10("RecoBackend", recoBackendLabel.c_str());
    const char* recoRKModeLabel = "three_point_free";
    if (recoRKMode == reco::RkFitMode::kTwoPointBackprop) {
        recoRKModeLabel = "two_point_backprop";
    } else if (recoRKMode == reco::RkFitMode::kFixedTargetPdcOnly) {
        recoRKModeLabel = "fixed_target_pdc_only";
    }
    TNamed info11("RecoRKMode", recoRKModeLabel);
    
    info1.Write();
    info2.Write();
    info3.Write(); 
    info4.Write();
    info5.Write();
    info6.Write();
    info7.Write();
    info8.Write();
    info9.Write();
    info10.Write();
    info11.Write();
    
    // 10. 写入并关闭文件
    outFile->Write();
    outFile->Close();
    
    // 11. 打印统计信息
    std::cout << "\n=== 批处理分析完成 ===" << std::endl;
    std::cout << "输入文件: " << inputPath << std::endl;
    std::cout << "输出文件: " << outputFile << std::endl;
    std::cout << "总事件数: " << totalEvents << std::endl;
    std::cout << "处理事件数: " << processedEvents << std::endl;
    const double denom = (processedEvents > 0) ? static_cast<double>(processedEvents) : 1.0;
    std::cout << "PDC击中事件: " << pdcHitEvents << " (" << (pdcHitEvents * 100.0 / denom) << "%%)" << std::endl;
    std::cout << "NEBULA击中事件: " << nebulaHitEvents << " (" << (nebulaHitEvents * 100.0 / denom) << "%%)" << std::endl;
    std::cout << "包含原始粒子数据的事件: " << originalDataEvents << " (" << (originalDataEvents * 100.0 / denom) << "%%)" << std::endl;
    
    if (protonRecoEnabled) {
        std::cout << "质子动量重建事件: " << protonReconEvents << " (" << (protonReconEvents * 100.0 / denom) << "%%)" << std::endl;
        std::cout << "重建质子总数: " << totalReconProtons << std::endl;
        std::cout << "平均每事件重建质子数: " << (protonReconEvents > 0 ? totalReconProtons*1.0/protonReconEvents : 0.0) << std::endl;
    } else {
        std::cout << "当前配置未启用质子动量重建" << std::endl;
    }
    
    // 清理内存
    delete recoEvent;
    delete originalData;
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
