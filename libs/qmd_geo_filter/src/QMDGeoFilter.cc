/**
 * @file QMDGeoFilter.cc
 * @brief QMD 数据几何接受度筛选库实现
 */

#include "QMDGeoFilter.hh"
#include "SMLogger.hh"

#include <TFile.h>
#include <TTree.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TH3D.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TMultiGraph.h>
#include <TLegend.h>
#include <TStyle.h>
#include <TLatex.h>
#include <TLine.h>
#include <TMath.h>
#include <TSystem.h>
#include <TROOT.h>
#include <TPad.h>
#include <TColor.h>
#include <TPaveText.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <sys/stat.h>

// =============================================================================
// QMDMomentumCut 实现
// =============================================================================

QMDMomentumCut::QMDMomentumCut(const std::string& polType)
    : fPolType(polType)
{
}

void QMDMomentumCut::RotateMomentum(double pxp, double pyp, double pxn, double pyn,
                                     double& pxp_rot, double& pyp_rot,
                                     double& pxn_rot, double& pyn_rot)
{
    // 计算 φ 角：arctan2(pyp + pyn, pxp + pxn)
    double phi = std::atan2(pyp + pyn, pxp + pxn);
    
    // 旋转使得 φ = 0
    double cos_phi = std::cos(-phi);
    double sin_phi = std::sin(-phi);
    
    pxp_rot = pxp * cos_phi - pyp * sin_phi;
    pyp_rot = pxp * sin_phi + pyp * cos_phi;
    pxn_rot = pxn * cos_phi - pyn * sin_phi;
    pyn_rot = pxn * sin_phi + pyn * cos_phi;
}

QMDMomentumCut::CutResult QMDMomentumCut::ApplyCut(const MomentumData& data)
{
    if (fPolType == "zpol" || fPolType == "z") {
        return ApplyZpolCut(data);
    } else {
        return ApplyYpolCut(data);
    }
}

QMDMomentumCut::CutResult QMDMomentumCut::ApplyZpolCut(const MomentumData& data)
{
    CutResult result;
    result.rotatedMomenta.reserve(data.size());
    
    for (size_t i = 0; i < data.size(); ++i) {
        double pxp = data.pxp[i], pyp = data.pyp[i], pzp = data.pzp[i];
        double pxn = data.pxn[i], pyn = data.pyn[i], pzn = data.pzn[i];
        
        // 计算各条件
        double pz_sum = pzp + pzn;
        double phi = std::atan2(pyp + pyn, pxp + pxn);
        double phi_cut = std::abs(M_PI - std::abs(phi));
        double delta_pz = std::abs(pzp - pzn);
        double px_sum = pxp + pxn;
        double pt_sum_sq = (pxp + pxn) * (pxp + pxn) + (pyp + pyn) * (pyp + pyn);
        
        // Zpol 条件（同时满足）
        bool pass = (pz_sum > fZpolParams.pzSumThreshold) &&
                    (phi_cut < fZpolParams.phiThreshold) &&
                    (delta_pz < fZpolParams.deltaPzThreshold) &&
                    (std::abs(px_sum) < fZpolParams.pxSumThreshold) &&
                    (pt_sum_sq > fZpolParams.ptSumSqThreshold);
        
        if (pass) {
            result.passedIndices.push_back(i);
            
            // 旋转动量
            double pxp_rot, pyp_rot, pxn_rot, pyn_rot;
            RotateMomentum(pxp, pyp, pxn, pyn, pxp_rot, pyp_rot, pxn_rot, pyn_rot);
            
            result.rotatedMomenta.pxp.push_back(pxp_rot);
            result.rotatedMomenta.pyp.push_back(pyp_rot);
            result.rotatedMomenta.pzp.push_back(pzp);
            result.rotatedMomenta.pxn.push_back(pxn_rot);
            result.rotatedMomenta.pyn.push_back(pyn_rot);
            result.rotatedMomenta.pzn.push_back(pzn);
        }
    }
    
    return result;
}

QMDMomentumCut::CutResult QMDMomentumCut::ApplyYpolCut(const MomentumData& data)
{
    CutResult result;
    result.rotatedMomenta.reserve(data.size());
    
    for (size_t i = 0; i < data.size(); ++i) {
        double pxp = data.pxp[i], pyp = data.pyp[i], pzp = data.pzp[i];
        double pxn = data.pxn[i], pyn = data.pyn[i], pzn = data.pzn[i];
        
        // 计算用于旋转的 phi（原始动量之和的方位角）
        double phi_for_rotation = std::atan2(pyp + pyn, pxp + pxn);
        
        // Step 1: 初始条件
        double delta_py = std::abs(pyp - pyn);
        double pt_sum_sq = (pxp + pxn) * (pxp + pxn) + (pyp + pyn) * (pyp + pyn);
        
        bool pass_step1 = (delta_py < fYpolParams.deltaPyThreshold) &&
                          (pt_sum_sq > fYpolParams.ptSumSqThreshold);
        
        if (!pass_step1) continue;
        
        // 旋转动量
        double pxp_rot, pyp_rot, pxn_rot, pyn_rot;
        RotateMomentum(pxp, pyp, pxn, pyn, pxp_rot, pyp_rot, pxn_rot, pyn_rot);
        
        // Step 2: 旋转后条件
        // 注意：使用原始的 phi_for_rotation 检查 phi 条件
        double px_sum_rot = pxp_rot + pxn_rot;  // 无绝对值，与 notebook 一致
        double phi_cut = std::abs(M_PI - std::abs(phi_for_rotation));  // 使用原始的 phi
        
        bool pass_step2 = (px_sum_rot < fYpolParams.pxSumAfterRotation) &&  // < 200
                          (phi_cut < fYpolParams.phiThreshold);              // < 0.2
        
        if (pass_step2) {
            result.passedIndices.push_back(i);
            
            result.rotatedMomenta.pxp.push_back(pxp_rot);
            result.rotatedMomenta.pyp.push_back(pyp_rot);
            result.rotatedMomenta.pzp.push_back(pzp);
            result.rotatedMomenta.pxn.push_back(pxn_rot);
            result.rotatedMomenta.pyn.push_back(pyn_rot);
            result.rotatedMomenta.pzn.push_back(pzn);
        }
    }
    
    return result;
}

MomentumData QMDMomentumCut::RotateAllMomenta(const MomentumData& data)
{
    MomentumData rotated;
    rotated.reserve(data.size());
    
    for (size_t i = 0; i < data.size(); ++i) {
        double pxp = data.pxp[i], pyp = data.pyp[i], pzp = data.pzp[i];
        double pxn = data.pxn[i], pyn = data.pyn[i], pzn = data.pzn[i];
        
        // 旋转动量到反应平面
        double pxp_rot, pyp_rot, pxn_rot, pyn_rot;
        RotateMomentum(pxp, pyp, pxn, pyn, pxp_rot, pyp_rot, pxn_rot, pyn_rot);
        
        rotated.pxp.push_back(pxp_rot);
        rotated.pyp.push_back(pyp_rot);
        rotated.pzp.push_back(pzp);
        rotated.pxn.push_back(pxn_rot);
        rotated.pyn.push_back(pyn_rot);
        rotated.pzn.push_back(pzn);
    }
    
    return rotated;
}

// =============================================================================
// QMDGeoFilter::AnalysisConfig 实现
// =============================================================================

QMDGeoFilter::AnalysisConfig::AnalysisConfig()
{
    const char* smsimdir = std::getenv("SMSIMDIR");
    std::string baseDir = smsimdir ? std::string(smsimdir) 
                                   : "/home/tian/workspace/dpol/smsimulator5.5";
    
    qmdDataPath = baseDir + "/data/qmdrawdata";
    fieldMapPath = baseDir + "/configs/simulation/geometry/filed_map";
    outputPath = baseDir + "/results/qmd_geo_filter";
    
    // 默认配置
    fieldStrengths = {1.0};
    deflectionAngles = {5.0};
    targets = {"Pb208"};
    polTypes = {"zpol", "ypol"};
    gammaValues = {"050", "060", "070", "080"};
    bMin = 5.0;
    bMax = 10.0;
}

// =============================================================================
// QMDGeoFilter 实现
// =============================================================================

QMDGeoFilter::QMDGeoFilter()
    : fMagField(nullptr),
      fCurrentFieldStrength(0),
      fCurrentDeflectionAngle(0)
{
    fGeoManager = std::make_unique<GeoAcceptanceManager>();
    fDetectorCalc = std::make_unique<DetectorAcceptanceCalculator>();
    fBeamCalc = std::make_unique<BeamDeflectionCalculator>();
}

QMDGeoFilter::~QMDGeoFilter()
{
    if (fMagField) {
        delete fMagField;
        fMagField = nullptr;
    }
}

std::string QMDGeoFilter::GetFieldMapFile(double fieldStrength, const std::string& basePath)
{
    // 根据磁场强度查找对应的 .table 文件
    // 格式：141114-0,8T-6000.table, 180626-1,00T-6000.table, etc.
    
    std::vector<std::pair<double, std::string>> fieldFiles = {
        {0.8, "141114-0,8T-6000.table"},
        {1.0, "180626-1,00T-6000.table"},
        {1.2, "180626-1,20T-3000.table"},
        {1.4, "180703-1,40T-6000.table"}
    };
    
    // 找最接近的磁场强度
    double minDiff = 1e10;
    std::string bestFile;
    
    for (const auto& [field, file] : fieldFiles) {
        double diff = std::abs(field - fieldStrength);
        if (diff < minDiff) {
            minDiff = diff;
            bestFile = file;
        }
    }
    
    return basePath + "/" + bestFile;
}

bool QMDGeoFilter::LoadFieldMap(double fieldStrength)
{
    if (fMagField) {
        delete fMagField;
        fMagField = nullptr;
    }
    
    std::string fieldFile = GetFieldMapFile(fieldStrength, fConfig.fieldMapPath);
    
    SM_INFO("Loading field map: {}", fieldFile);
    
    fMagField = new MagneticField();
    
    // MagneticField 可以从 .table 文件加载
    if (!fMagField->LoadFieldMap(fieldFile)) {
        SM_ERROR("Failed to load field map: {}", fieldFile);
        delete fMagField;
        fMagField = nullptr;
        return false;
    }
    
    fCurrentFieldStrength = fieldStrength;
    
    // 设置给其他计算器
    fBeamCalc->SetMagneticField(fMagField);
    fDetectorCalc->SetMagneticField(fMagField);
    
    return true;
}

std::string QMDGeoFilter::BuildQMDDataPath(const std::string& target, 
                                            const std::string& polType,
                                            const std::string& gamma,
                                            const std::string& polSuffix)
{
    // 构建 QMD 数据路径
    // 实际格式: qmdrawdata/qmdrawdata/z_pol/b_discrete/d+Pb208E190g050zpn/
    // zpol: z_pol/b_discrete/d+{target}E{energy}g{gamma}zpn/
    // ypol: y_pol/phi_random/d+{target}E{energy}g{gamma}{ypn|ynp}/
    
    std::string polDir = (polType == "zpol" || polType == "z") ? "z_pol" : "y_pol";
    std::string subDir = (polType == "zpol" || polType == "z") ? "b_discrete" : "phi_random";
    
    // 能量固定为 190 MeV/u
    std::string energy = "190";
    
    // 文件夹格式: d+{target}E{energy}g{gamma}{polSuffix}
    // 例如: d+Pb208E190g050zpn
    std::ostringstream folderName;
    folderName << "d+" << target << "E" << energy << "g" << gamma << polSuffix;
    
    // 注意：数据路径有双层 qmdrawdata
    return fConfig.qmdDataPath + "/qmdrawdata/" + polDir + "/" + subDir + "/" + folderName.str();
}

MomentumData QMDGeoFilter::LoadQMDData(const std::string& target, 
                                        const std::string& polType,
                                        const std::string& gamma)
{
    MomentumData data;
    
    bool isZpol = (polType == "zpol" || polType == "z");
    
    // 对于 zpol (b_discrete)，有多个文件 dbreakb01.dat ~ dbreakb12.dat，只有 zpn 目录
    // 对于 ypol (phi_random)，需要读取 ynp 和 ypn 两个目录的 dbreak.dat
    
    std::vector<std::string> dataFiles;
    
    if (isZpol) {
        std::string dataDir = BuildQMDDataPath(target, polType, gamma, "zpn");
        // zpol 有 12 个文件 dbreakb01.dat ~ dbreakb12.dat
        for (int i = 1; i <= 12; ++i) {
            std::ostringstream filename;
            filename << dataDir << "/dbreakb" << std::setfill('0') << std::setw(2) << i << ".dat";
            dataFiles.push_back(filename.str());
        }
    } else {
        // ypol 需要读取 ynp 和 ypn 两个目录
        std::vector<std::string> ypolSuffixes = {"ynp", "ypn"};
        for (const auto& suffix : ypolSuffixes) {
            std::string dataDir = BuildQMDDataPath(target, polType, gamma, suffix);
            dataFiles.push_back(dataDir + "/dbreak.dat");
        }
    }
    
    // 读取所有数据文件
    for (size_t fileIdx = 0; fileIdx < dataFiles.size(); ++fileIdx) {
        const auto& file = dataFiles[fileIdx];
        std::ifstream ifs(file);
        if (!ifs.is_open()) {
            SM_WARN("Cannot open file: {}", file);
            continue;
        }
        
        std::string line;
        
        // 跳过头两行
        std::getline(ifs, line);  // 第一行：文件信息 (包含 b 值对于 zpol)
        
        // 对于 zpol，从第一行解析 b 值
        double fileB = 0;
        if (isZpol) {
            // 格式: "zpn d+208Pb   190.00MeV/u b= 5.000fm  10000events"
            size_t bPos = line.find("b=");
            if (bPos != std::string::npos) {
                std::string bStr = line.substr(bPos + 2);
                std::istringstream bss(bStr);
                bss >> fileB;
            }
        }
        
        std::getline(ifs, line);  // 第二行：列标题
        
        // 读取数据行
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            
            std::istringstream iss(line);
            
            if (isZpol) {
                // zpol 格式: No pxp pyp pzp pxn pyn pzn (7列)
                int eventNo;
                double pxp, pyp, pzp, pxn, pyn, pzn;
                
                if (iss >> eventNo >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn) {
                    // 使用文件头中的 b 值进行筛选
                    if (fileB >= fConfig.bMin && fileB <= fConfig.bMax) {
                        data.pxp.push_back(pxp);
                        data.pyp.push_back(pyp);
                        data.pzp.push_back(pzp);
                        data.pxn.push_back(pxn);
                        data.pyn.push_back(pyn);
                        data.pzn.push_back(pzn);
                    }
                }
            } else {
                // ypol 格式: No pxp pyp pzp pxn pyn pzn b rpphi (9列)
                int eventNo;
                double pxp, pyp, pzp, pxn, pyn, pzn, b, rpphi;
                
                if (iss >> eventNo >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn >> b >> rpphi) {
                    // 使用每行的 b 值进行筛选
                    if (b >= fConfig.bMin && b <= fConfig.bMax) {
                        data.pxp.push_back(pxp);
                        data.pyp.push_back(pyp);
                        data.pzp.push_back(pzp);
                        data.pxn.push_back(pxn);
                        data.pyn.push_back(pyn);
                        data.pzn.push_back(pzn);
                    }
                }
            }
        }
    }
    
    SM_INFO("Loaded {} events for {} {} gamma={} (b range: {}-{}fm)", 
            data.size(), target, polType, gamma, fConfig.bMin, fConfig.bMax);
    
    return data;
}

std::vector<int> QMDGeoFilter::ApplyGeometryFilter(const MomentumData& data,
                                                    const TVector3& targetPos,
                                                    double targetRotationAngle)
{
    std::vector<int> passedIndices;
    
    // 使用 DetectorAcceptanceCalculator 进行几何接受度计算
    // 先获取最佳 PDC 位置
    auto pdcConfig = fDetectorCalc->CalculateOptimalPDCPosition(targetPos, targetRotationAngle);
    fDetectorCalc->SetPDCConfiguration(pdcConfig);
    
    // 检查每个事件
    for (size_t i = 0; i < data.size(); ++i) {
        // 创建粒子信息
        DetectorAcceptanceCalculator::ParticleInfo proton, neutron;
        
        proton.pdgCode = 2212;  // proton
        proton.charge = 1.0;
        proton.mass = 938.272;
        proton.momentum.SetPxPyPzE(data.pxp[i], data.pyp[i], data.pzp[i],
            std::sqrt(data.pxp[i]*data.pxp[i] + data.pyp[i]*data.pyp[i] + 
                     data.pzp[i]*data.pzp[i] + 938.272*938.272));
        proton.vertex = targetPos;
        
        neutron.pdgCode = 2112;  // neutron
        neutron.charge = 0.0;
        neutron.mass = 939.565;
        neutron.momentum.SetPxPyPzE(data.pxn[i], data.pyn[i], data.pzn[i],
            std::sqrt(data.pxn[i]*data.pxn[i] + data.pyn[i]*data.pyn[i] + 
                     data.pzn[i]*data.pzn[i] + 939.565*939.565));
        neutron.vertex = targetPos;
        
        // 检查是否同时打到 PDC 和 NEBULA
        TVector3 hitPosPDC, hitPosNEBULA;
        bool pdcHit = fDetectorCalc->CheckPDCHit(proton, hitPosPDC);
        bool nebulaHit = fDetectorCalc->CheckNEBULAHit(neutron, hitPosNEBULA);
        
        if (pdcHit && nebulaHit) {
            passedIndices.push_back(i);
        }
    }
    
    return passedIndices;
}

RatioResult QMDGeoFilter::CalculateRatio(const MomentumData& data,
                                          const std::vector<int>& indices)
{
    RatioResult result;
    
    // ratio = N(pxp - pxn > 0) / N(pxp - pxn < 0)
    
    auto countRatio = [&](const std::vector<size_t>& idxList) {
        int nPositive = 0, nNegative = 0;
        for (size_t idx : idxList) {
            double diff = data.pxp[idx] - data.pxn[idx];
            if (diff > 0) nPositive++;
            else if (diff < 0) nNegative++;
            // diff == 0 不计入任何一方
        }
        return std::make_pair(nPositive, nNegative);
    };
    
    if (indices.empty()) {
        // 使用所有数据
        if (data.size() == 0) return result;
        
        int nPositive = 0, nNegative = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            double diff = data.pxp[i] - data.pxn[i];
            if (diff > 0) nPositive++;
            else if (diff < 0) nNegative++;
        }
        
        result.nEvents = data.size();
        result.nPositive = nPositive;
        result.nNegative = nNegative;
        
        if (nNegative > 0) {
            result.ratio = static_cast<double>(nPositive) / static_cast<double>(nNegative);
            // 二项式误差估计
            double p = static_cast<double>(nPositive) / (nPositive + nNegative);
            double n = nPositive + nNegative;
            double sigma_p = std::sqrt(p * (1 - p) / n);
            // 误差传递: ratio = p / (1-p), d(ratio)/dp = 1/(1-p)^2
            if (p < 1.0) {
                result.error = sigma_p / ((1 - p) * (1 - p));
            } else {
                result.error = 0;
            }
        }
    } else {
        // 使用指定索引
        if (indices.empty()) return result;
        
        int nPositive = 0, nNegative = 0;
        for (int idx : indices) {
            double diff = data.pxp[idx] - data.pxn[idx];
            if (diff > 0) nPositive++;
            else if (diff < 0) nNegative++;
        }
        
        result.nEvents = indices.size();
        result.nPositive = nPositive;
        result.nNegative = nNegative;
        
        if (nNegative > 0) {
            result.ratio = static_cast<double>(nPositive) / static_cast<double>(nNegative);
            double p = static_cast<double>(nPositive) / (nPositive + nNegative);
            double n = nPositive + nNegative;
            double sigma_p = std::sqrt(p * (1 - p) / n);
            if (p < 1.0) {
                result.error = sigma_p / ((1 - p) * (1 - p));
            } else {
                result.error = 0;
            }
        }
    }
    
    return result;
}

QMDConfigurationResult QMDGeoFilter::AnalyzeSingleConfiguration(
    double fieldStrength,
    double deflectionAngle,
    const std::string& target,
    const std::string& polType)
{
    QMDConfigurationResult configResult;
    configResult.fieldStrength = fieldStrength;
    configResult.deflectionAngle = deflectionAngle;
    configResult.target = target;
    configResult.polType = polType;
    
    SM_INFO("Analyzing: {}T, {}deg, {}, {}", fieldStrength, deflectionAngle, target, polType);
    
    // 加载磁场（如果需要）
    if (std::abs(fCurrentFieldStrength - fieldStrength) > 0.01) {
        if (!LoadFieldMap(fieldStrength)) {
            SM_ERROR("Failed to load field map for {}T", fieldStrength);
            return configResult;
        }
    }
    
    // 计算 target 位置
    auto targetPos = fBeamCalc->CalculateTargetPosition(deflectionAngle);
    configResult.targetPosition = targetPos;
    
    // 获取 PDC 配置
    TVector3 targetVec(targetPos.position.X(), targetPos.position.Y(), targetPos.position.Z());
    configResult.pdcConfig = fDetectorCalc->CalculateOptimalPDCPosition(targetVec, targetPos.rotationAngle);
    configResult.nebulaConfig = fDetectorCalc->GetNEBULAConfiguration();
    
    // 创建动量 cut
    QMDMomentumCut momentumCut(polType);
    
    // 分析每个 gamma 值
    for (const auto& gamma : fConfig.gammaValues) {
        GammaAnalysisResult gammaResult;
        gammaResult.gamma = gamma;
        gammaResult.polType = polType;
        
        // 加载数据
        MomentumData rawData = LoadQMDData(target, polType, gamma);
        gammaResult.nBeforeCut = rawData.size();
        gammaResult.rawMomenta = rawData;  // 存储原始数据
        
        if (rawData.size() == 0) {
            SM_WARN("No data for gamma={}", gamma);
            configResult.gammaResults[gamma] = gammaResult;
            continue;
        }
        
        // 旋转所有原始动量到反应平面（用于计算 before cut 的 ratio）
        MomentumData rotatedRawData = momentumCut.RotateAllMomenta(rawData);
        gammaResult.rotatedMomentaRaw = rotatedRawData;  // 存储旋转后的原始数据
        gammaResult.ratioBeforeCut = CalculateRatio(rotatedRawData, {});
        
        // 应用动量 cut
        auto cutResult = momentumCut.ApplyCut(rawData);
        gammaResult.nAfterCut = cutResult.passedIndices.size();
        gammaResult.afterCutMomenta = cutResult.rotatedMomenta;  // 存储 cut 后的数据
        gammaResult.ratioAfterCut = CalculateRatio(cutResult.rotatedMomenta, {});
        gammaResult.cutEfficiency = (double)gammaResult.nAfterCut / gammaResult.nBeforeCut;
        
        // 应用几何筛选
        auto geoIndices = ApplyGeometryFilter(cutResult.rotatedMomenta, targetVec, targetPos.rotationAngle);
        gammaResult.nAfterGeometry = geoIndices.size();
        gammaResult.ratioAfterGeometry = CalculateRatio(cutResult.rotatedMomenta, geoIndices);
        gammaResult.geoEfficiency = gammaResult.nAfterCut > 0 ? 
            (double)gammaResult.nAfterGeometry / gammaResult.nAfterCut : 0;
        gammaResult.totalEfficiency = (double)gammaResult.nAfterGeometry / gammaResult.nBeforeCut;
        
        // 创建通过/未通过 geometry 的索引集合
        std::set<int> passedSet(geoIndices.begin(), geoIndices.end());
        
        // 存储通过 geometry 和未通过 geometry 的数据
        gammaResult.afterGeoMomenta.reserve(geoIndices.size());
        gammaResult.rejectedByGeoMomenta.reserve(cutResult.rotatedMomenta.size() - geoIndices.size());
        
        for (size_t idx = 0; idx < cutResult.rotatedMomenta.size(); ++idx) {
            if (passedSet.count(idx)) {
                // 通过 geometry
                gammaResult.afterGeoMomenta.pxp.push_back(cutResult.rotatedMomenta.pxp[idx]);
                gammaResult.afterGeoMomenta.pyp.push_back(cutResult.rotatedMomenta.pyp[idx]);
                gammaResult.afterGeoMomenta.pzp.push_back(cutResult.rotatedMomenta.pzp[idx]);
                gammaResult.afterGeoMomenta.pxn.push_back(cutResult.rotatedMomenta.pxn[idx]);
                gammaResult.afterGeoMomenta.pyn.push_back(cutResult.rotatedMomenta.pyn[idx]);
                gammaResult.afterGeoMomenta.pzn.push_back(cutResult.rotatedMomenta.pzn[idx]);
            } else {
                // 未通过 geometry
                gammaResult.rejectedByGeoMomenta.pxp.push_back(cutResult.rotatedMomenta.pxp[idx]);
                gammaResult.rejectedByGeoMomenta.pyp.push_back(cutResult.rotatedMomenta.pyp[idx]);
                gammaResult.rejectedByGeoMomenta.pzp.push_back(cutResult.rotatedMomenta.pzp[idx]);
                gammaResult.rejectedByGeoMomenta.pxn.push_back(cutResult.rotatedMomenta.pxn[idx]);
                gammaResult.rejectedByGeoMomenta.pyn.push_back(cutResult.rotatedMomenta.pyn[idx]);
                gammaResult.rejectedByGeoMomenta.pzn.push_back(cutResult.rotatedMomenta.pzn[idx]);
            }
        }
        
        SM_INFO("  gamma={}: {} -> {} -> {} events (cut eff: {:.1f}%, geo eff: {:.1f}%)",
                gamma, gammaResult.nBeforeCut, gammaResult.nAfterCut, gammaResult.nAfterGeometry,
                gammaResult.cutEfficiency * 100, gammaResult.geoEfficiency * 100);
        SM_INFO("    Ratio: before={:.3f} ({}+/{}-)  after_cut={:.3f} ({}+/{}-)  after_geo={:.3f} ({}+/{}-)",
                gammaResult.ratioBeforeCut.ratio, 
                gammaResult.ratioBeforeCut.nPositive, gammaResult.ratioBeforeCut.nNegative,
                gammaResult.ratioAfterCut.ratio,
                gammaResult.ratioAfterCut.nPositive, gammaResult.ratioAfterCut.nNegative,
                gammaResult.ratioAfterGeometry.ratio,
                gammaResult.ratioAfterGeometry.nPositive, gammaResult.ratioAfterGeometry.nNegative);
        SM_INFO("    Geometry: {} passed, {} rejected", 
                gammaResult.afterGeoMomenta.size(), gammaResult.rejectedByGeoMomenta.size());
        
        configResult.gammaResults[gamma] = gammaResult;
        
        // 清理临时数据（保留存储的动量数据用于后续绘图）
        rawData.clear();
        rotatedRawData.clear();
    }
    
    return configResult;
}

bool QMDGeoFilter::RunFullAnalysis()
{
    SM_INFO("╔═══════════════════════════════════════════════════════════════╗");
    SM_INFO("║        QMD Geometry Filter Analysis - Starting...            ║");
    SM_INFO("╚═══════════════════════════════════════════════════════════════╝");
    
    fAllResults.clear();
    
    CreateOutputDirectory(fConfig.outputPath);
    
    // 遍历所有配置
    for (double field : fConfig.fieldStrengths) {
        for (double angle : fConfig.deflectionAngles) {
            // 生成探测器几何布局图（每个 angle 层级一张）
            std::ostringstream geoPlotPath;
            geoPlotPath << fConfig.outputPath << "/" 
                        << std::fixed << std::setprecision(2) << field << "T/"
                        << std::fixed << std::setprecision(0) << angle << "deg/";
            CreateOutputDirectory(geoPlotPath.str());
            GenerateDetectorGeometryPlot(field, angle, geoPlotPath.str() + "detector_geometry.pdf");
            
            for (const auto& target : fConfig.targets) {
                for (const auto& polType : fConfig.polTypes) {
                    auto result = AnalyzeSingleConfiguration(field, angle, target, polType);
                    fAllResults.push_back(result);
                    
                    // 生成该配置的图表
                    std::ostringstream outputDir;
                    outputDir << fConfig.outputPath << "/" 
                              << std::fixed << std::setprecision(2) << field << "T/"
                              << std::fixed << std::setprecision(0) << angle << "deg/"
                              << target << "/" << polType << "/";
                    CreateOutputDirectory(outputDir.str());
                    GeneratePlots(result, outputDir.str());
                    
                    // 清理内存
                    ClearCurrentData();
                }
            }
            
            // 在 angle 层级生成 ratio 对比图
            std::ostringstream ratioPlotPath;
            ratioPlotPath << fConfig.outputPath << "/" 
                          << std::fixed << std::setprecision(2) << field << "T/"
                          << std::fixed << std::setprecision(0) << angle << "deg/"
                          << "ratio_comparison.pdf";
            GenerateRatioComparisonPlot(ratioPlotPath.str());
        }
    }
    
    // 生成总结报告
    GenerateSummaryTable();
    GenerateTextReport(fConfig.outputPath + "/analysis_report.txt");
    
    SM_INFO("╔═══════════════════════════════════════════════════════════════╗");
    SM_INFO("║        QMD Geometry Filter Analysis - Complete!              ║");
    SM_INFO("╚═══════════════════════════════════════════════════════════════╝");
    
    return true;
}

void QMDGeoFilter::GenerateDetectorGeometryPlot(double fieldStrength, double deflectionAngle,
                                                  const std::string& outputFile)
{
    SM_INFO("Generating detector geometry plot: {}", outputFile);
    
    // 复用 GeoAcceptanceManager 的绘图功能
    // 首先设置配置
    GeoAcceptanceManager::AnalysisConfig geoConfig;
    geoConfig.fieldStrengths = {fieldStrength};
    geoConfig.deflectionAngles = {deflectionAngle};
    geoConfig.fieldMapFiles = {GetFieldMapFile(fieldStrength, fConfig.fieldMapPath)};
    geoConfig.outputPath = fConfig.outputPath;
    
    fGeoManager->SetConfig(geoConfig);
    
    // 分析该配置以获取结果
    fGeoManager->AnalyzeFieldConfiguration(
        GetFieldMapFile(fieldStrength, fConfig.fieldMapPath), 
        fieldStrength
    );
    
    // 生成几何布局图
    fGeoManager->GenerateGeometryPlot(outputFile);
}

void QMDGeoFilter::GeneratePlots(const QMDConfigurationResult& result,
                                  const std::string& outputDir)
{
    // 为每个 gamma 生成详细图表
    for (const auto& [gamma, gammaResult] : result.gammaResults) {
        std::string gammaDir = outputDir + "gamma_" + gamma + "/";
        CreateOutputDirectory(gammaDir);
        
        // 生成 pxp-pxn 分布图（三个阶段）
        GeneratePxpPxnPlot(gammaResult.rotatedMomentaRaw, 
                           "Before Cut (#gamma=" + gamma + ")",
                           gammaDir + "pxp_pxn_before_cut.pdf");
        
        GeneratePxpPxnPlot(gammaResult.afterCutMomenta,
                           "After Cut (#gamma=" + gamma + ")",
                           gammaDir + "pxp_pxn_after_cut.pdf");
        
        GeneratePxpPxnPlot(gammaResult.afterGeoMomenta,
                           "After Geometry (#gamma=" + gamma + ")",
                           gammaDir + "pxp_pxn_after_geometry.pdf");
        
        // 生成综合 pxp-pxn 对比图
        GeneratePxpPxnComparisonPlot(gammaResult, gammaDir + "pxp_pxn_comparison.pdf");
        
        // 生成 3D 动量分布对比图（geometry 通过/未通过）
        Generate3DMomentumComparisonPlot(gammaResult, gammaDir + "3d_momentum_geo_comparison.pdf");
        
        // 生成 2D 投影图
        Generate3DMomentum2DProjections(gammaResult, gammaDir + "3d_momentum_geo_comparison_2d_proj.pdf");
        
        // 生成交互式 HTML 3D 图（可旋转）
        GenerateInteractive3DPlot(gammaResult, gammaDir + "3d_momentum_interactive.html");
    }
    
    // 生成跨 gamma 对比图
    GenerateGammaComparisonPlot(result, outputDir + "gamma_comparison.pdf");
}

void QMDGeoFilter::GenerateGammaComparisonPlot(const QMDConfigurationResult& result,
                                                 const std::string& outputFile)
{
    // 清除全局对象列表中的残留图例
    gROOT->GetListOfSpecials()->Clear();
    
    auto canvas = new TCanvas("c_gamma", "Gamma Comparison", 1200, 800);
    canvas->Clear();
    canvas->Divide(2, 2);
    
    // 收集数据
    std::vector<double> gammas, ratios_before, ratios_after, ratios_geo;
    std::vector<double> errors_before, errors_after, errors_geo;
    
    for (const auto& [gamma, res] : result.gammaResults) {
        double g = std::stod(gamma) / 100.0;
        gammas.push_back(g);
        ratios_before.push_back(res.ratioBeforeCut.ratio);
        ratios_after.push_back(res.ratioAfterCut.ratio);
        ratios_geo.push_back(res.ratioAfterGeometry.ratio);
        errors_before.push_back(res.ratioBeforeCut.error);
        errors_after.push_back(res.ratioAfterCut.error);
        errors_geo.push_back(res.ratioAfterGeometry.error);
    }
    
    if (gammas.empty()) {
        delete canvas;
        return;
    }
    
    // 绘制 ratio vs gamma
    canvas->cd(1);
    auto gr_before = new TGraphErrors(gammas.size(), gammas.data(), ratios_before.data(),
                                       nullptr, errors_before.data());
    gr_before->SetTitle("Before Cut;#gamma;Ratio");
    gr_before->SetMarkerStyle(20);
    gr_before->Draw("AP");
    
    canvas->cd(2);
    auto gr_after = new TGraphErrors(gammas.size(), gammas.data(), ratios_after.data(),
                                      nullptr, errors_after.data());
    gr_after->SetTitle("After Cut;#gamma;Ratio");
    gr_after->SetMarkerStyle(21);
    gr_after->SetMarkerColor(kBlue);
    gr_after->Draw("AP");
    
    canvas->cd(3);
    auto gr_geo = new TGraphErrors(gammas.size(), gammas.data(), ratios_geo.data(),
                                    nullptr, errors_geo.data());
    gr_geo->SetTitle("After Geometry;#gamma;Ratio");
    gr_geo->SetMarkerStyle(22);
    gr_geo->SetMarkerColor(kRed);
    gr_geo->Draw("AP");
    
    // 所有对比
    canvas->cd(4);
    auto mg = new TMultiGraph();
    mg->Add(gr_before, "P");
    mg->Add(gr_after, "P");
    mg->Add(gr_geo, "P");
    mg->SetTitle("Comparison;#gamma;Ratio");
    mg->Draw("A");
    
    auto legend = new TLegend(0.7, 0.7, 0.9, 0.9);
    legend->AddEntry(gr_before, "Before Cut", "p");
    legend->AddEntry(gr_after, "After Cut", "p");
    legend->AddEntry(gr_geo, "After Geometry", "p");
    legend->Draw();
    
    canvas->SaveAs(outputFile.c_str());
    delete canvas;
}

void QMDGeoFilter::GenerateRatioComparisonPlot(const std::string& outputFile)
{
    // 清除全局对象列表中的残留图例
    gROOT->GetListOfSpecials()->Clear();
    
    // 生成 ratio 对比图（before/after geometry filter）
    auto canvas = new TCanvas("c_ratio", "Ratio Comparison", 1000, 700);
    canvas->Clear();
    
    auto mg = new TMultiGraph();
    auto legend = new TLegend(0.65, 0.15, 0.9, 0.4);
    
    int colorIndex = 0;
    std::vector<int> colors = {kBlue, kRed, kGreen+2, kMagenta, kOrange, kCyan};
    
    for (const auto& result : fAllResults) {
        std::vector<double> gammas, ratios_before, ratios_after;
        std::vector<double> errors_before, errors_after;
        
        for (const auto& [gamma, res] : result.gammaResults) {
            double g = std::stod(gamma) / 100.0;
            gammas.push_back(g);
            ratios_before.push_back(res.ratioAfterCut.ratio);
            ratios_after.push_back(res.ratioAfterGeometry.ratio);
            errors_before.push_back(res.ratioAfterCut.error);
            errors_after.push_back(res.ratioAfterGeometry.error);
        }
        
        if (gammas.empty()) continue;
        
        int color = colors[colorIndex % colors.size()];
        
        // Before geometry (实线)
        auto gr_before = new TGraphErrors(gammas.size(), gammas.data(), ratios_before.data(),
                                           nullptr, errors_before.data());
        gr_before->SetMarkerStyle(20);
        gr_before->SetMarkerColor(color);
        gr_before->SetLineColor(color);
        gr_before->SetLineStyle(1);
        mg->Add(gr_before, "LP");
        
        // After geometry (虚线)
        auto gr_after = new TGraphErrors(gammas.size(), gammas.data(), ratios_after.data(),
                                          nullptr, errors_after.data());
        gr_after->SetMarkerStyle(24);
        gr_after->SetMarkerColor(color);
        gr_after->SetLineColor(color);
        gr_after->SetLineStyle(2);
        mg->Add(gr_after, "LP");
        
        std::string label = result.target + "_" + result.polType;
        legend->AddEntry(gr_before, (label + " (before geo)").c_str(), "lp");
        legend->AddEntry(gr_after, (label + " (after geo)").c_str(), "lp");
        
        colorIndex++;
    }
    
    mg->SetTitle("Ratio Comparison (Before/After Geometry Filter);#gamma;Ratio");
    mg->Draw("A");
    
    // 添加 y=1 参考线
    auto line = new TLine(mg->GetXaxis()->GetXmin(), 1.0, mg->GetXaxis()->GetXmax(), 1.0);
    line->SetLineStyle(2);
    line->SetLineColor(kGray);
    line->Draw();
    
    legend->Draw();
    
    canvas->SaveAs(outputFile.c_str());
    delete canvas;
}

void QMDGeoFilter::GenerateSummaryTable()
{
    SM_INFO("");
    SM_INFO("╔═══════════════════════════════════════════════════════════════╗");
    SM_INFO("║                    Analysis Summary Table                     ║");
    SM_INFO("╚═══════════════════════════════════════════════════════════════╝");
    
    std::cout << std::left << std::setw(10) << "Field" 
              << std::setw(8) << "Angle"
              << std::setw(10) << "Target"
              << std::setw(8) << "Pol"
              << std::setw(8) << "Gamma"
              << std::setw(10) << "Before"
              << std::setw(10) << "AfterCut"
              << std::setw(10) << "AfterGeo"
              << std::setw(10) << "CutEff"
              << std::setw(10) << "GeoEff"
              << std::endl;
    
    std::cout << std::string(94, '-') << std::endl;
    
    for (const auto& result : fAllResults) {
        for (const auto& [gamma, res] : result.gammaResults) {
            std::cout << std::left 
                      << std::setw(10) << std::fixed << std::setprecision(2) << result.fieldStrength
                      << std::setw(8) << std::fixed << std::setprecision(1) << result.deflectionAngle
                      << std::setw(10) << result.target
                      << std::setw(8) << result.polType
                      << std::setw(8) << gamma
                      << std::setw(10) << res.nBeforeCut
                      << std::setw(10) << res.nAfterCut
                      << std::setw(10) << res.nAfterGeometry
                      << std::setw(10) << std::fixed << std::setprecision(1) << (res.cutEfficiency * 100) << "%"
                      << std::setw(10) << std::fixed << std::setprecision(1) << (res.geoEfficiency * 100) << "%"
                      << std::endl;
        }
    }
}

void QMDGeoFilter::GenerateTextReport(const std::string& filename)
{
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        SM_ERROR("Cannot open file for writing: {}", filename);
        return;
    }
    
    ofs << "QMD Geometry Filter Analysis Report\n";
    ofs << "====================================\n\n";
    
    for (const auto& result : fAllResults) {
        ofs << "Configuration: " << result.fieldStrength << "T, " 
            << result.deflectionAngle << "deg, "
            << result.target << ", " << result.polType << "\n";
        ofs << std::string(50, '-') << "\n";
        
        for (const auto& [gamma, res] : result.gammaResults) {
            ofs << "  Gamma = " << gamma << "\n";
            ofs << "    Events: " << res.nBeforeCut << " -> " << res.nAfterCut 
                << " -> " << res.nAfterGeometry << "\n";
            ofs << "    Cut efficiency: " << std::fixed << std::setprecision(2) 
                << (res.cutEfficiency * 100) << "%\n";
            ofs << "    Geo efficiency: " << std::fixed << std::setprecision(2) 
                << (res.geoEfficiency * 100) << "%\n";
            ofs << "    Ratio (before cut): " << res.ratioBeforeCut.ratio 
                << " +/- " << res.ratioBeforeCut.error 
                << " (" << res.ratioBeforeCut.nPositive << "+/" << res.ratioBeforeCut.nNegative << "-)\n";
            ofs << "    Ratio (after cut): " << res.ratioAfterCut.ratio 
                << " +/- " << res.ratioAfterCut.error 
                << " (" << res.ratioAfterCut.nPositive << "+/" << res.ratioAfterCut.nNegative << "-)\n";
            ofs << "    Ratio (after geo): " << res.ratioAfterGeometry.ratio 
                << " +/- " << res.ratioAfterGeometry.error 
                << " (" << res.ratioAfterGeometry.nPositive << "+/" << res.ratioAfterGeometry.nNegative << "-)\n";
        }
        ofs << "\n";
    }
    
    SM_INFO("Report saved to: {}", filename);
}

void QMDGeoFilter::CreateOutputDirectory(const std::string& path)
{
    gSystem->mkdir(path.c_str(), true);
}

void QMDGeoFilter::ClearCurrentData()
{
    // 清理当前分析的临时数据，释放内存
    fDetectorCalc->ClearData();
}

QMDGeoFilter::AnalysisConfig QMDGeoFilter::CreateDefaultConfig()
{
    return AnalysisConfig();
}

// =============================================================================
// Pxp-Pxn 分布图生成函数
// =============================================================================

void QMDGeoFilter::GeneratePxpPxnPlot(const MomentumData& data,
                                       const std::string& title,
                                       const std::string& outputFile)
{
    if (data.size() == 0) {
        SM_WARN("No data for pxp-pxn plot: {}", title);
        return;
    }
    
    // 清除残留
    gROOT->GetListOfSpecials()->Clear();
    
    auto canvas = new TCanvas("c_pxp_pxn", title.c_str(), 800, 600);
    canvas->Clear();
    
    // 计算 pxp - pxn 的范围
    double minDiff = 1e10, maxDiff = -1e10;
    for (size_t i = 0; i < data.size(); ++i) {
        double diff = data.pxp[i] - data.pxn[i];
        minDiff = std::min(minDiff, diff);
        maxDiff = std::max(maxDiff, diff);
    }
    
    // 扩展范围
    double range = maxDiff - minDiff;
    minDiff -= range * 0.1;
    maxDiff += range * 0.1;
    
    // 创建直方图
    auto h_diff = new TH1D("h_pxp_pxn", (title + ";p_{x,p} - p_{x,n} [MeV/c];Counts").c_str(),
                           100, minDiff, maxDiff);
    
    int nPositive = 0, nNegative = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        double diff = data.pxp[i] - data.pxn[i];
        h_diff->Fill(diff);
        if (diff > 0) nPositive++;
        else if (diff < 0) nNegative++;
    }
    
    h_diff->SetLineColor(kBlue);
    h_diff->SetFillColor(kBlue);
    h_diff->SetFillStyle(3004);
    h_diff->Draw();
    
    // 添加 x = 0 参考线
    auto line = new TLine(0, 0, 0, h_diff->GetMaximum() * 1.05);
    line->SetLineStyle(2);
    line->SetLineColor(kRed);
    line->SetLineWidth(2);
    line->Draw();
    
    // 添加统计信息
    auto pave = new TPaveText(0.6, 0.7, 0.88, 0.88, "NDC");
    pave->SetFillColor(kWhite);
    pave->SetBorderSize(1);
    pave->AddText(Form("Total: %d", (int)data.size()));
    pave->AddText(Form("N(p_{x,p}-p_{x,n}>0): %d", nPositive));
    pave->AddText(Form("N(p_{x,p}-p_{x,n}<0): %d", nNegative));
    if (nNegative > 0) {
        pave->AddText(Form("Ratio: %.3f", (double)nPositive / nNegative));
    }
    pave->Draw();
    
    canvas->SaveAs(outputFile.c_str());
    
    delete h_diff;
    delete line;
    delete pave;
    delete canvas;
}

void QMDGeoFilter::GeneratePxpPxnComparisonPlot(const GammaAnalysisResult& gammaResult,
                                                 const std::string& outputFile)
{
    // 清除残留
    gROOT->GetListOfSpecials()->Clear();
    
    auto canvas = new TCanvas("c_pxp_pxn_comp", "Pxp-Pxn Comparison", 1200, 400);
    canvas->Clear();
    canvas->Divide(3, 1);
    
    // 计算全局范围
    double minDiff = 1e10, maxDiff = -1e10;
    
    auto updateRange = [&](const MomentumData& data) {
        for (size_t i = 0; i < data.size(); ++i) {
            double diff = data.pxp[i] - data.pxn[i];
            minDiff = std::min(minDiff, diff);
            maxDiff = std::max(maxDiff, diff);
        }
    };
    
    updateRange(gammaResult.rotatedMomentaRaw);
    updateRange(gammaResult.afterCutMomenta);
    updateRange(gammaResult.afterGeoMomenta);
    
    // 扩展范围
    double range = maxDiff - minDiff;
    if (range < 1e-10) range = 200;  // 防止除零
    minDiff -= range * 0.1;
    maxDiff += range * 0.1;
    
    // 绘制三个阶段
    auto drawStage = [&](int pad, const MomentumData& data, const std::string& stageName,
                         const RatioResult& ratio, int color) {
        canvas->cd(pad);
        gPad->Clear();
        
        auto h = new TH1D(Form("h_%d", pad), 
                          (stageName + ";p_{x,p} - p_{x,n} [MeV/c];Counts").c_str(),
                          50, minDiff, maxDiff);
        
        for (size_t i = 0; i < data.size(); ++i) {
            h->Fill(data.pxp[i] - data.pxn[i]);
        }
        
        h->SetLineColor(color);
        h->SetFillColor(color);
        h->SetFillStyle(3004);
        h->Draw();
        
        // x = 0 参考线
        auto line = new TLine(0, 0, 0, h->GetMaximum() * 1.05);
        line->SetLineStyle(2);
        line->SetLineColor(kRed);
        line->Draw();
        
        // 统计信息
        auto pave = new TPaveText(0.55, 0.65, 0.88, 0.88, "NDC");
        pave->SetFillColor(kWhite);
        pave->SetBorderSize(1);
        pave->SetTextSize(0.04);
        pave->AddText(Form("N=%d", ratio.nEvents));
        pave->AddText(Form("%d+ / %d-", ratio.nPositive, ratio.nNegative));
        pave->AddText(Form("Ratio=%.3f", ratio.ratio));
        pave->Draw();
    };
    
    drawStage(1, gammaResult.rotatedMomentaRaw, 
              "Before Cut (#gamma=" + gammaResult.gamma + ")",
              gammaResult.ratioBeforeCut, kBlue);
    
    drawStage(2, gammaResult.afterCutMomenta,
              "After Cut (#gamma=" + gammaResult.gamma + ")",
              gammaResult.ratioAfterCut, kGreen+2);
    
    drawStage(3, gammaResult.afterGeoMomenta,
              "After Geometry (#gamma=" + gammaResult.gamma + ")",
              gammaResult.ratioAfterGeometry, kRed);
    
    canvas->SaveAs(outputFile.c_str());
    delete canvas;
}

// =============================================================================
// 3D 动量分布对比图 (geometry 通过/未通过)
// =============================================================================

void QMDGeoFilter::Generate3DMomentumComparisonPlot(const GammaAnalysisResult& gammaResult,
                                                     const std::string& outputFile)
{
    // 清除残留
    gROOT->GetListOfSpecials()->Clear();
    
    // 创建 2x2 的 canvas：
    // 上排：Proton (passed / rejected)
    // 下排：Neutron (passed / rejected)
    auto canvas = new TCanvas("c_3d_geo", "3D Momentum: Geometry Comparison", 1400, 1200);
    canvas->Clear();
    canvas->Divide(2, 2);
    
    const MomentumData& passed = gammaResult.afterGeoMomenta;
    const MomentumData& rejected = gammaResult.rejectedByGeoMomenta;
    
    // 计算全局范围
    auto getRange = [](const std::vector<double>& v1, const std::vector<double>& v2, 
                       double& minVal, double& maxVal) {
        minVal = 1e10; maxVal = -1e10;
        for (double x : v1) { minVal = std::min(minVal, x); maxVal = std::max(maxVal, x); }
        for (double x : v2) { minVal = std::min(minVal, x); maxVal = std::max(maxVal, x); }
        double range = maxVal - minVal;
        if (range < 1e-10) range = 200;
        minVal -= range * 0.1;
        maxVal += range * 0.1;
    };
    
    double pxMin, pxMax, pyMin, pyMax, pzMin, pzMax;
    
    // Proton ranges
    std::vector<double> allPxp, allPyp, allPzp;
    allPxp.insert(allPxp.end(), passed.pxp.begin(), passed.pxp.end());
    allPxp.insert(allPxp.end(), rejected.pxp.begin(), rejected.pxp.end());
    allPyp.insert(allPyp.end(), passed.pyp.begin(), passed.pyp.end());
    allPyp.insert(allPyp.end(), rejected.pyp.begin(), rejected.pyp.end());
    allPzp.insert(allPzp.end(), passed.pzp.begin(), passed.pzp.end());
    allPzp.insert(allPzp.end(), rejected.pzp.begin(), rejected.pzp.end());
    
    getRange(passed.pxp, rejected.pxp, pxMin, pxMax);
    getRange(passed.pyp, rejected.pyp, pyMin, pyMax);
    getRange(passed.pzp, rejected.pzp, pzMin, pzMax);
    
    // Neutron ranges (use same for consistency)
    double nxMin, nxMax, nyMin, nyMax, nzMin, nzMax;
    getRange(passed.pxn, rejected.pxn, nxMin, nxMax);
    getRange(passed.pyn, rejected.pyn, nyMin, nyMax);
    getRange(passed.pzn, rejected.pzn, nzMin, nzMax);
    
    // 1. Proton - Passed (蓝色)
    canvas->cd(1);
    gPad->Clear();
    auto h3_proton_passed = new TH3D("h3_p_pass", 
        Form("Proton - Passed (%d events);p_{x} [MeV/c];p_{y} [MeV/c];p_{z} [MeV/c]", (int)passed.size()),
        30, pxMin, pxMax, 30, pyMin, pyMax, 30, pzMin, pzMax);
    
    for (size_t i = 0; i < passed.size(); ++i) {
        h3_proton_passed->Fill(passed.pxp[i], passed.pyp[i], passed.pzp[i]);
    }
    h3_proton_passed->SetMarkerColor(kBlue);
    h3_proton_passed->SetMarkerStyle(20);
    h3_proton_passed->SetMarkerSize(0.3);
    h3_proton_passed->Draw("BOX2");
    
    // 2. Proton - Rejected (红色)
    canvas->cd(2);
    gPad->Clear();
    auto h3_proton_rejected = new TH3D("h3_p_rej", 
        Form("Proton - Rejected (%d events);p_{x} [MeV/c];p_{y} [MeV/c];p_{z} [MeV/c]", (int)rejected.size()),
        30, pxMin, pxMax, 30, pyMin, pyMax, 30, pzMin, pzMax);
    
    for (size_t i = 0; i < rejected.size(); ++i) {
        h3_proton_rejected->Fill(rejected.pxp[i], rejected.pyp[i], rejected.pzp[i]);
    }
    h3_proton_rejected->SetMarkerColor(kRed);
    h3_proton_rejected->SetMarkerStyle(20);
    h3_proton_rejected->SetMarkerSize(0.3);
    h3_proton_rejected->Draw("BOX2");
    
    // 3. Neutron - Passed (蓝色)
    canvas->cd(3);
    gPad->Clear();
    auto h3_neutron_passed = new TH3D("h3_n_pass", 
        Form("Neutron - Passed (%d events);p_{x} [MeV/c];p_{y} [MeV/c];p_{z} [MeV/c]", (int)passed.size()),
        30, nxMin, nxMax, 30, nyMin, nyMax, 30, nzMin, nzMax);
    
    for (size_t i = 0; i < passed.size(); ++i) {
        h3_neutron_passed->Fill(passed.pxn[i], passed.pyn[i], passed.pzn[i]);
    }
    h3_neutron_passed->SetMarkerColor(kBlue);
    h3_neutron_passed->SetMarkerStyle(20);
    h3_neutron_passed->SetMarkerSize(0.3);
    h3_neutron_passed->Draw("BOX2");
    
    // 4. Neutron - Rejected (红色)
    canvas->cd(4);
    gPad->Clear();
    auto h3_neutron_rejected = new TH3D("h3_n_rej", 
        Form("Neutron - Rejected (%d events);p_{x} [MeV/c];p_{y} [MeV/c];p_{z} [MeV/c]", (int)rejected.size()),
        30, nxMin, nxMax, 30, nyMin, nyMax, 30, nzMin, nzMax);
    
    for (size_t i = 0; i < rejected.size(); ++i) {
        h3_neutron_rejected->Fill(rejected.pxn[i], rejected.pyn[i], rejected.pzn[i]);
    }
    h3_neutron_rejected->SetMarkerColor(kRed);
    h3_neutron_rejected->SetMarkerStyle(20);
    h3_neutron_rejected->SetMarkerSize(0.3);
    h3_neutron_rejected->Draw("BOX2");
    
    canvas->SaveAs(outputFile.c_str());
    
    // 清理
    delete h3_proton_passed;
    delete h3_proton_rejected;
    delete h3_neutron_passed;
    delete h3_neutron_rejected;
    delete canvas;
    
    // 额外：生成 2D 投影图（更清晰地看分布）
    std::string projFile = outputFile;
    size_t dotPos = projFile.rfind('.');
    if (dotPos != std::string::npos) {
        projFile = projFile.substr(0, dotPos) + "_2d_proj.pdf";
    }
    Generate3DMomentum2DProjections(gammaResult, projFile);
}

void QMDGeoFilter::Generate3DMomentum2DProjections(const GammaAnalysisResult& gammaResult,
                                                     const std::string& outputFile)
{
    // 清除残留
    gROOT->GetListOfSpecials()->Clear();
    
    // 创建 2x3 的 canvas：
    // 上排：Proton (px vs py, px vs pz, py vs pz) - 通过/未通过叠加
    // 下排：Neutron (px vs py, px vs pz, py vs pz) - 通过/未通过叠加
    auto canvas = new TCanvas("c_2d_proj", "2D Projections: Geometry Comparison", 1500, 1000);
    canvas->Clear();
    canvas->Divide(3, 2);
    
    const MomentumData& passed = gammaResult.afterGeoMomenta;
    const MomentumData& rejected = gammaResult.rejectedByGeoMomenta;
    
    // Proton px vs py
    canvas->cd(1);
    gPad->Clear();
    auto gr_p_passed_xy = new TGraph(passed.size());
    for (size_t i = 0; i < passed.size(); ++i) {
        gr_p_passed_xy->SetPoint(i, passed.pxp[i], passed.pyp[i]);
    }
    gr_p_passed_xy->SetTitle("Proton p_{x} vs p_{y};p_{x} [MeV/c];p_{y} [MeV/c]");
    gr_p_passed_xy->SetMarkerColor(kBlue);
    gr_p_passed_xy->SetMarkerStyle(20);
    gr_p_passed_xy->SetMarkerSize(0.3);
    gr_p_passed_xy->Draw("AP");
    
    auto gr_p_rejected_xy = new TGraph(rejected.size());
    for (size_t i = 0; i < rejected.size(); ++i) {
        gr_p_rejected_xy->SetPoint(i, rejected.pxp[i], rejected.pyp[i]);
    }
    gr_p_rejected_xy->SetMarkerColor(kRed);
    gr_p_rejected_xy->SetMarkerStyle(20);
    gr_p_rejected_xy->SetMarkerSize(0.3);
    gr_p_rejected_xy->Draw("P SAME");
    
    auto leg1 = new TLegend(0.7, 0.75, 0.9, 0.9);
    leg1->AddEntry(gr_p_passed_xy, "Passed", "p");
    leg1->AddEntry(gr_p_rejected_xy, "Rejected", "p");
    leg1->Draw();
    
    // Proton px vs pz
    canvas->cd(2);
    gPad->Clear();
    auto gr_p_passed_xz = new TGraph(passed.size());
    for (size_t i = 0; i < passed.size(); ++i) {
        gr_p_passed_xz->SetPoint(i, passed.pxp[i], passed.pzp[i]);
    }
    gr_p_passed_xz->SetTitle("Proton p_{x} vs p_{z};p_{x} [MeV/c];p_{z} [MeV/c]");
    gr_p_passed_xz->SetMarkerColor(kBlue);
    gr_p_passed_xz->SetMarkerStyle(20);
    gr_p_passed_xz->SetMarkerSize(0.3);
    gr_p_passed_xz->Draw("AP");
    
    auto gr_p_rejected_xz = new TGraph(rejected.size());
    for (size_t i = 0; i < rejected.size(); ++i) {
        gr_p_rejected_xz->SetPoint(i, rejected.pxp[i], rejected.pzp[i]);
    }
    gr_p_rejected_xz->SetMarkerColor(kRed);
    gr_p_rejected_xz->SetMarkerStyle(20);
    gr_p_rejected_xz->SetMarkerSize(0.3);
    gr_p_rejected_xz->Draw("P SAME");
    
    // Proton py vs pz
    canvas->cd(3);
    gPad->Clear();
    auto gr_p_passed_yz = new TGraph(passed.size());
    for (size_t i = 0; i < passed.size(); ++i) {
        gr_p_passed_yz->SetPoint(i, passed.pyp[i], passed.pzp[i]);
    }
    gr_p_passed_yz->SetTitle("Proton p_{y} vs p_{z};p_{y} [MeV/c];p_{z} [MeV/c]");
    gr_p_passed_yz->SetMarkerColor(kBlue);
    gr_p_passed_yz->SetMarkerStyle(20);
    gr_p_passed_yz->SetMarkerSize(0.3);
    gr_p_passed_yz->Draw("AP");
    
    auto gr_p_rejected_yz = new TGraph(rejected.size());
    for (size_t i = 0; i < rejected.size(); ++i) {
        gr_p_rejected_yz->SetPoint(i, rejected.pyp[i], rejected.pzp[i]);
    }
    gr_p_rejected_yz->SetMarkerColor(kRed);
    gr_p_rejected_yz->SetMarkerStyle(20);
    gr_p_rejected_yz->SetMarkerSize(0.3);
    gr_p_rejected_yz->Draw("P SAME");
    
    // Neutron px vs py
    canvas->cd(4);
    gPad->Clear();
    auto gr_n_passed_xy = new TGraph(passed.size());
    for (size_t i = 0; i < passed.size(); ++i) {
        gr_n_passed_xy->SetPoint(i, passed.pxn[i], passed.pyn[i]);
    }
    gr_n_passed_xy->SetTitle("Neutron p_{x} vs p_{y};p_{x} [MeV/c];p_{y} [MeV/c]");
    gr_n_passed_xy->SetMarkerColor(kBlue);
    gr_n_passed_xy->SetMarkerStyle(20);
    gr_n_passed_xy->SetMarkerSize(0.3);
    gr_n_passed_xy->Draw("AP");
    
    auto gr_n_rejected_xy = new TGraph(rejected.size());
    for (size_t i = 0; i < rejected.size(); ++i) {
        gr_n_rejected_xy->SetPoint(i, rejected.pxn[i], rejected.pyn[i]);
    }
    gr_n_rejected_xy->SetMarkerColor(kRed);
    gr_n_rejected_xy->SetMarkerStyle(20);
    gr_n_rejected_xy->SetMarkerSize(0.3);
    gr_n_rejected_xy->Draw("P SAME");
    
    auto leg4 = new TLegend(0.7, 0.75, 0.9, 0.9);
    leg4->AddEntry(gr_n_passed_xy, "Passed", "p");
    leg4->AddEntry(gr_n_rejected_xy, "Rejected", "p");
    leg4->Draw();
    
    // Neutron px vs pz
    canvas->cd(5);
    gPad->Clear();
    auto gr_n_passed_xz = new TGraph(passed.size());
    for (size_t i = 0; i < passed.size(); ++i) {
        gr_n_passed_xz->SetPoint(i, passed.pxn[i], passed.pzn[i]);
    }
    gr_n_passed_xz->SetTitle("Neutron p_{x} vs p_{z};p_{x} [MeV/c];p_{z} [MeV/c]");
    gr_n_passed_xz->SetMarkerColor(kBlue);
    gr_n_passed_xz->SetMarkerStyle(20);
    gr_n_passed_xz->SetMarkerSize(0.3);
    gr_n_passed_xz->Draw("AP");
    
    auto gr_n_rejected_xz = new TGraph(rejected.size());
    for (size_t i = 0; i < rejected.size(); ++i) {
        gr_n_rejected_xz->SetPoint(i, rejected.pxn[i], rejected.pzn[i]);
    }
    gr_n_rejected_xz->SetMarkerColor(kRed);
    gr_n_rejected_xz->SetMarkerStyle(20);
    gr_n_rejected_xz->SetMarkerSize(0.3);
    gr_n_rejected_xz->Draw("P SAME");
    
    // Neutron py vs pz
    canvas->cd(6);
    gPad->Clear();
    auto gr_n_passed_yz = new TGraph(passed.size());
    for (size_t i = 0; i < passed.size(); ++i) {
        gr_n_passed_yz->SetPoint(i, passed.pyn[i], passed.pzn[i]);
    }
    gr_n_passed_yz->SetTitle("Neutron p_{y} vs p_{z};p_{y} [MeV/c];p_{z} [MeV/c]");
    gr_n_passed_yz->SetMarkerColor(kBlue);
    gr_n_passed_yz->SetMarkerStyle(20);
    gr_n_passed_yz->SetMarkerSize(0.3);
    gr_n_passed_yz->Draw("AP");
    
    auto gr_n_rejected_yz = new TGraph(rejected.size());
    for (size_t i = 0; i < rejected.size(); ++i) {
        gr_n_rejected_yz->SetPoint(i, rejected.pyn[i], rejected.pzn[i]);
    }
    gr_n_rejected_yz->SetMarkerColor(kRed);
    gr_n_rejected_yz->SetMarkerStyle(20);
    gr_n_rejected_yz->SetMarkerSize(0.3);
    gr_n_rejected_yz->Draw("P SAME");
    
    canvas->SaveAs(outputFile.c_str());
    delete canvas;
}

//==============================================================================
// GenerateInteractive3DPlot - 生成交互式 HTML 3D 散点图 (使用 Three.js)
//==============================================================================
void QMDGeoFilter::GenerateInteractive3DPlot(const GammaAnalysisResult& gammaResult,
                                              const std::string& outputFile)
{
    const auto& passed = gammaResult.afterGeoMomenta;
    const auto& rejected = gammaResult.rejectedByGeoMomenta;
    
    std::ofstream html(outputFile);
    if (!html.is_open()) {
        SM_WARN("Failed to create HTML file: {}", outputFile);
        return;
    }
    
    // 计算数据范围用于归一化
    double maxP = 0;
    for (size_t i = 0; i < passed.size(); ++i) {
        maxP = std::max(maxP, std::abs(passed.pxp[i]));
        maxP = std::max(maxP, std::abs(passed.pyp[i]));
        maxP = std::max(maxP, std::abs(passed.pzp[i]));
        maxP = std::max(maxP, std::abs(passed.pxn[i]));
        maxP = std::max(maxP, std::abs(passed.pyn[i]));
        maxP = std::max(maxP, std::abs(passed.pzn[i]));
    }
    for (size_t i = 0; i < rejected.size(); ++i) {
        maxP = std::max(maxP, std::abs(rejected.pxp[i]));
        maxP = std::max(maxP, std::abs(rejected.pyp[i]));
        maxP = std::max(maxP, std::abs(rejected.pzp[i]));
        maxP = std::max(maxP, std::abs(rejected.pxn[i]));
        maxP = std::max(maxP, std::abs(rejected.pyn[i]));
        maxP = std::max(maxP, std::abs(rejected.pzn[i]));
    }
    double scale = 5.0 / maxP;  // 归一化到 [-5, 5]
    
    double geoEff = 100.0 * passed.size() / (passed.size() + rejected.size());
    
    // 写入 HTML - 使用分段写入避免 raw string 问题
    html << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    html << "<meta charset=\"UTF-8\">\n";
    html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "<title>3D Momentum Distribution - " << gammaResult.gamma << "</title>\n";
    html << "<style>\n";
    html << "* { margin: 0; padding: 0; box-sizing: border-box; }\n";
    html << "body { font-family: 'Segoe UI', Arial, sans-serif; background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%); min-height: 100vh; color: #fff; }\n";
    html << ".container { display: flex; flex-direction: column; height: 100vh; }\n";
    html << ".header { padding: 15px 30px; background: rgba(0,0,0,0.3); border-bottom: 1px solid rgba(255,255,255,0.1); }\n";
    html << ".header h1 { font-size: 1.5em; font-weight: 300; }\n";
    html << ".header .subtitle { color: #888; font-size: 0.9em; margin-top: 5px; }\n";
    html << ".main-content { display: flex; flex: 1; overflow: hidden; }\n";
    html << ".canvas-container { flex: 1; display: grid; grid-template-columns: 1fr 1fr; grid-template-rows: 1fr 1fr; gap: 2px; background: #000; }\n";
    html << ".canvas-wrapper { position: relative; background: #0a0a15; }\n";
    html << ".canvas-wrapper canvas { width: 100% !important; height: 100% !important; }\n";
    html << ".canvas-label { position: absolute; top: 10px; left: 10px; background: rgba(0,0,0,0.7); padding: 8px 15px; border-radius: 5px; font-size: 14px; z-index: 10; }\n";
    html << ".sidebar { width: 280px; padding: 20px; background: rgba(0,0,0,0.3); border-left: 1px solid rgba(255,255,255,0.1); overflow-y: auto; }\n";
    html << ".legend { margin-bottom: 25px; }\n";
    html << ".legend h3 { margin-bottom: 15px; font-weight: 400; font-size: 1em; }\n";
    html << ".legend-item { display: flex; align-items: center; margin: 10px 0; font-size: 0.9em; }\n";
    html << ".legend-color { width: 20px; height: 20px; border-radius: 50%; margin-right: 12px; box-shadow: 0 0 10px rgba(255,255,255,0.3); }\n";
    html << ".stats { background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px; }\n";
    html << ".stats h3 { margin-bottom: 12px; font-weight: 400; font-size: 1em; }\n";
    html << ".stat-row { display: flex; justify-content: space-between; margin: 8px 0; font-size: 0.85em; }\n";
    html << ".stat-value { color: #4fc3f7; font-weight: 500; }\n";
    html << ".controls { margin-top: 20px; }\n";
    html << ".controls h3 { margin-bottom: 12px; font-weight: 400; font-size: 1em; }\n";
    html << ".control-group { margin: 15px 0; }\n";
    html << ".control-group label { display: block; margin-bottom: 8px; font-size: 0.85em; color: #aaa; }\n";
    html << ".slider { width: 100%; -webkit-appearance: none; height: 4px; background: #333; border-radius: 2px; outline: none; }\n";
    html << ".slider::-webkit-slider-thumb { -webkit-appearance: none; width: 16px; height: 16px; background: #4fc3f7; border-radius: 50%; cursor: pointer; }\n";
    html << ".checkbox-group { display: flex; align-items: center; margin: 10px 0; }\n";
    html << ".checkbox-group input { margin-right: 10px; }\n";
    html << ".btn { width: 100%; padding: 10px; margin: 5px 0; border: none; border-radius: 5px; background: #4fc3f7; color: #000; cursor: pointer; font-size: 0.9em; transition: background 0.2s; }\n";
    html << ".btn:hover { background: #29b6f6; }\n";
    html << ".btn.secondary { background: #444; color: #fff; }\n";
    html << ".btn.secondary:hover { background: #555; }\n";
    html << "</style>\n</head>\n<body>\n";
    
    // Body content
    html << "<div class=\"container\">\n";
    html << "<div class=\"header\">\n";
    html << "<h1>3D Momentum Distribution Viewer</h1>\n";
    html << "<div class=\"subtitle\">Gamma = " << gammaResult.gamma << " | Polarization: " << gammaResult.polType << " | Drag to rotate, scroll to zoom</div>\n";
    html << "</div>\n";
    html << "<div class=\"main-content\">\n";
    html << "<div class=\"canvas-container\">\n";
    html << "<div class=\"canvas-wrapper\" id=\"proton-container\"><div class=\"canvas-label\">Proton (px, py, pz)</div></div>\n";
    html << "<div class=\"canvas-wrapper\" id=\"neutron-container\"><div class=\"canvas-label\">Neutron (px, py, pz)</div></div>\n";
    html << "<div class=\"canvas-wrapper\" id=\"combined-container\"><div class=\"canvas-label\">Combined View</div></div>\n";
    html << "<div class=\"canvas-wrapper\" id=\"diff-container\"><div class=\"canvas-label\">Delta p = pp - pn</div></div>\n";
    html << "</div>\n";
    
    // Sidebar
    html << "<div class=\"sidebar\">\n";
    html << "<div class=\"legend\"><h3>Legend</h3>\n";
    html << "<div class=\"legend-item\"><div class=\"legend-color\" style=\"background: #00ff88;\"></div><span>Passed Geometry</span></div>\n";
    html << "<div class=\"legend-item\"><div class=\"legend-color\" style=\"background: #ff4444;\"></div><span>Rejected by Geometry</span></div>\n";
    html << "</div>\n";
    html << "<div class=\"stats\"><h3>Statistics</h3>\n";
    html << "<div class=\"stat-row\"><span>Passed Events:</span><span class=\"stat-value\">" << passed.size() << "</span></div>\n";
    html << "<div class=\"stat-row\"><span>Rejected Events:</span><span class=\"stat-value\">" << rejected.size() << "</span></div>\n";
    html << "<div class=\"stat-row\"><span>Geo Efficiency:</span><span class=\"stat-value\">" << std::fixed << std::setprecision(1) << geoEff << "%</span></div>\n";
    html << "<div class=\"stat-row\"><span>Ratio (after geo):</span><span class=\"stat-value\">" << std::fixed << std::setprecision(3) << gammaResult.ratioAfterGeometry.ratio << "</span></div>\n";
    html << "</div>\n";
    html << "<div class=\"controls\"><h3>Controls</h3>\n";
    html << "<div class=\"control-group\"><label>Point Size</label><input type=\"range\" class=\"slider\" id=\"pointSize\" min=\"1\" max=\"10\" value=\"3\"></div>\n";
    html << "<div class=\"control-group\"><label>Point Opacity</label><input type=\"range\" class=\"slider\" id=\"opacity\" min=\"10\" max=\"100\" value=\"70\"></div>\n";
    html << "<div class=\"checkbox-group\"><input type=\"checkbox\" id=\"showPassed\" checked><label for=\"showPassed\">Show Passed</label></div>\n";
    html << "<div class=\"checkbox-group\"><input type=\"checkbox\" id=\"showRejected\" checked><label for=\"showRejected\">Show Rejected</label></div>\n";
    html << "<div class=\"checkbox-group\"><input type=\"checkbox\" id=\"autoRotate\"><label for=\"autoRotate\">Auto Rotate</label></div>\n";
    html << "<button class=\"btn\" id=\"resetBtn\">Reset View</button>\n";
    html << "<button class=\"btn secondary\" id=\"syncBtn\">Sync All Views</button>\n";
    html << "</div></div></div></div>\n";
    
    // Scripts
    html << "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js\"></script>\n";
    html << "<script src=\"https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js\"></script>\n";
    html << "<script>\n";
    
    // Output data
    html << "const passedProton = [\n";
    for (size_t i = 0; i < passed.size(); ++i) {
        html << "[" << passed.pxp[i] * scale << "," << passed.pyp[i] * scale << "," << passed.pzp[i] * scale << "]";
        if (i < passed.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    html << "const passedNeutron = [\n";
    for (size_t i = 0; i < passed.size(); ++i) {
        html << "[" << passed.pxn[i] * scale << "," << passed.pyn[i] * scale << "," << passed.pzn[i] * scale << "]";
        if (i < passed.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    html << "const rejectedProton = [\n";
    for (size_t i = 0; i < rejected.size(); ++i) {
        html << "[" << rejected.pxp[i] * scale << "," << rejected.pyp[i] * scale << "," << rejected.pzp[i] * scale << "]";
        if (i < rejected.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    html << "const rejectedNeutron = [\n";
    for (size_t i = 0; i < rejected.size(); ++i) {
        html << "[" << rejected.pxn[i] * scale << "," << rejected.pyn[i] * scale << "," << rejected.pzn[i] * scale << "]";
        if (i < rejected.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    html << "const dataScale = " << scale << ";\n";
    html << "const maxP = " << maxP << ";\n\n";
    
    // JavaScript code - using escaped strings
    html << R"JSEND(
function createScene(containerId) {
    const container = document.getElementById(containerId);
    const width = container.clientWidth;
    const height = container.clientHeight;
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x0a0a15);
    const camera = new THREE.PerspectiveCamera(60, width / height, 0.1, 1000);
    camera.position.set(12, 8, 12);
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setSize(width, height);
    container.appendChild(renderer.domElement);
    const controls = new THREE.OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    const axesHelper = new THREE.AxesHelper(6);
    scene.add(axesHelper);
    const gridHelper = new THREE.GridHelper(10, 10, 0x444444, 0x222222);
    scene.add(gridHelper);
    addAxisLabels(scene);
    return { scene, camera, renderer, controls, container };
}

function addAxisLabels(scene) {
    const createLabel = (text, position) => {
        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');
        canvas.width = 128;
        canvas.height = 64;
        ctx.fillStyle = '#ffffff';
        ctx.font = '24px Arial';
        ctx.textAlign = 'center';
        ctx.fillText(text, 64, 40);
        const texture = new THREE.CanvasTexture(canvas);
        const material = new THREE.SpriteMaterial({ map: texture });
        const sprite = new THREE.Sprite(material);
        sprite.position.copy(position);
        sprite.scale.set(2, 1, 1);
        return sprite;
    };
    scene.add(createLabel('Px', new THREE.Vector3(7, 0, 0)));
    scene.add(createLabel('Py', new THREE.Vector3(0, 7, 0)));
    scene.add(createLabel('Pz', new THREE.Vector3(0, 0, 7)));
}

function createPoints(data, color, opacity = 0.7, size = 0.08) {
    const geometry = new THREE.BufferGeometry();
    const positions = new Float32Array(data.length * 3);
    for (let i = 0; i < data.length; i++) {
        positions[i * 3] = data[i][0];
        positions[i * 3 + 1] = data[i][1];
        positions[i * 3 + 2] = data[i][2];
    }
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    const material = new THREE.PointsMaterial({
        color: color,
        size: size,
        transparent: true,
        opacity: opacity,
        sizeAttenuation: true
    });
    return new THREE.Points(geometry, material);
}

const scenes = {};
const pointGroups = {};

scenes.proton = createScene('proton-container');
pointGroups.proton = {
    passed: createPoints(passedProton, 0x00ff88),
    rejected: createPoints(rejectedProton, 0xff4444)
};
scenes.proton.scene.add(pointGroups.proton.passed);
scenes.proton.scene.add(pointGroups.proton.rejected);

scenes.neutron = createScene('neutron-container');
pointGroups.neutron = {
    passed: createPoints(passedNeutron, 0x00ff88),
    rejected: createPoints(rejectedNeutron, 0xff4444)
};
scenes.neutron.scene.add(pointGroups.neutron.passed);
scenes.neutron.scene.add(pointGroups.neutron.rejected);

scenes.combined = createScene('combined-container');
pointGroups.combined = {
    passedP: createPoints(passedProton, 0x4488ff),
    passedN: createPoints(passedNeutron, 0xffaa00),
    rejectedP: createPoints(rejectedProton, 0xff4444, 0.3),
    rejectedN: createPoints(rejectedNeutron, 0xff4444, 0.3)
};
scenes.combined.scene.add(pointGroups.combined.passedP);
scenes.combined.scene.add(pointGroups.combined.passedN);
scenes.combined.scene.add(pointGroups.combined.rejectedP);
scenes.combined.scene.add(pointGroups.combined.rejectedN);

const passedDiff = passedProton.map((p, i) => [
    p[0] - passedNeutron[i][0],
    p[1] - passedNeutron[i][1],
    p[2] - passedNeutron[i][2]
]);
const rejectedDiff = rejectedProton.map((p, i) => [
    p[0] - rejectedNeutron[i][0],
    p[1] - rejectedNeutron[i][1],
    p[2] - rejectedNeutron[i][2]
]);

scenes.diff = createScene('diff-container');
pointGroups.diff = {
    passed: createPoints(passedDiff, 0x00ff88),
    rejected: createPoints(rejectedDiff, 0xff4444)
};
scenes.diff.scene.add(pointGroups.diff.passed);
scenes.diff.scene.add(pointGroups.diff.rejected);

function animate() {
    requestAnimationFrame(animate);
    Object.values(scenes).forEach(s => {
        s.controls.update();
        s.renderer.render(s.scene, s.camera);
    });
}
animate();

window.addEventListener('resize', () => {
    Object.values(scenes).forEach(s => {
        const width = s.container.clientWidth;
        const height = s.container.clientHeight;
        s.camera.aspect = width / height;
        s.camera.updateProjectionMatrix();
        s.renderer.setSize(width, height);
    });
});

document.getElementById('pointSize').addEventListener('input', (e) => {
    const size = e.target.value / 30;
    Object.values(pointGroups).forEach(group => {
        Object.values(group).forEach(points => {
            points.material.size = size;
        });
    });
});

document.getElementById('opacity').addEventListener('input', (e) => {
    const opacity = e.target.value / 100;
    Object.values(pointGroups).forEach(group => {
        Object.values(group).forEach(points => {
            points.material.opacity = opacity;
        });
    });
});

document.getElementById('showPassed').addEventListener('change', (e) => {
    const visible = e.target.checked;
    pointGroups.proton.passed.visible = visible;
    pointGroups.neutron.passed.visible = visible;
    pointGroups.combined.passedP.visible = visible;
    pointGroups.combined.passedN.visible = visible;
    pointGroups.diff.passed.visible = visible;
});

document.getElementById('showRejected').addEventListener('change', (e) => {
    const visible = e.target.checked;
    pointGroups.proton.rejected.visible = visible;
    pointGroups.neutron.rejected.visible = visible;
    pointGroups.combined.rejectedP.visible = visible;
    pointGroups.combined.rejectedN.visible = visible;
    pointGroups.diff.rejected.visible = visible;
});

document.getElementById('autoRotate').addEventListener('change', (e) => {
    Object.values(scenes).forEach(s => {
        s.controls.autoRotate = e.target.checked;
        s.controls.autoRotateSpeed = 2;
    });
});

document.getElementById('resetBtn').addEventListener('click', () => {
    Object.values(scenes).forEach(s => {
        s.camera.position.set(12, 8, 12);
        s.controls.reset();
    });
});

document.getElementById('syncBtn').addEventListener('click', () => {
    const ref = scenes.proton;
    Object.values(scenes).forEach(s => {
        if (s !== ref) {
            s.camera.position.copy(ref.camera.position);
            s.camera.rotation.copy(ref.camera.rotation);
            s.controls.target.copy(ref.controls.target);
        }
    });
});
)JSEND";
    
    html << "</script>\n</body>\n</html>\n";
    
    html.close();
    SM_INFO("Interactive 3D plot saved to: {}", outputFile);
}
