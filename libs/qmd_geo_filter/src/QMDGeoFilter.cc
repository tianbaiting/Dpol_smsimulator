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
#include <TPolyMarker3D.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <random>
#include <sys/stat.h>

namespace {
// [EN] Right-handed rotation around Y-axis, consistent with Geant4::rotateY / [CN] 与Geant4::rotateY一致的右手系Y轴旋转
// [EN] x' = x*cos + z*sin, z' = -x*sin + z*cos / [CN] x' = x*cos + z*sin, z' = -x*sin + z*cos
TVector3 RotateYDeg(const TVector3& v, double angleDeg)
{
    double rad = angleDeg * TMath::DegToRad();
    double cosA = std::cos(rad);
    double sinA = std::sin(rad);
    return TVector3(v.X() * cosA + v.Z() * sinA,
                    v.Y(),
                    -v.X() * sinA + v.Z() * cosA);
}

// [EN] Parse SAMURAI PDC settings from Geant4 macro (Angle/Position1/Position2)
// [CN] 从Geant4宏文件解析SAMURAI的PDC设置（Angle/Position1/Position2）
bool ParsePdcMacro(const std::string& macroPath,
                   double& angleDegOut,
                   TVector3& pos1Out,
                   TVector3& pos2Out)
{
    std::ifstream ifs(macroPath);
    if (!ifs.is_open()) {
        return false;
    }
    
    bool hasAngle = false;
    bool hasPos1 = false;
    bool hasPos2 = false;
    
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        
        // [EN] Strip inline comments / [CN] 去除行内注释
        size_t hashPos = line.find('#');
        if (hashPos != std::string::npos) {
            line = line.substr(0, hashPos);
        }
        
        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) continue;
        
        if (cmd == "/samurai/geometry/PDC/Angle") {
            double angle = 0.0;
            std::string unit;
            if (iss >> angle) {
                if (iss >> unit) {
                    // [EN] Only "deg" is expected / [CN] 只处理deg
                }
                angleDegOut = angle;
                hasAngle = true;
            }
        } else if (cmd == "/samurai/geometry/PDC/Position1" ||
                   cmd == "/samurai/geometry/PDC/Position2") {
            double x = 0.0, y = 0.0, z = 0.0;
            std::string unit;
            if (iss >> x >> y >> z) {
                double scale = 1.0;  // [EN] default mm / [CN] 默认mm
                if (iss >> unit) {
                    if (unit == "cm") scale = 10.0;
                    else if (unit == "mm") scale = 1.0;
                }
                TVector3 pos(x * scale, y * scale, z * scale);
                if (cmd == "/samurai/geometry/PDC/Position1") {
                    pos1Out = pos;
                    hasPos1 = true;
                } else {
                    pos2Out = pos;
                    hasPos2 = true;
                }
            }
        }
    }
    
    return hasAngle && hasPos1 && hasPos2;
}
}

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
    result.momenta.reserve(data.size());
    
    // 随机数生成器用于 z 轴随机旋转
    // zpol QMD 计算时入射粒子固定轴向，需要加随机旋转模拟真实情况
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<double> dist(0.0, 2.0 * M_PI);
    
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
            
            // 对 zpol 数据，需要绕 z 轴随机旋转
            // 因为 QMD 计算时入射粒子固定轴向，需要模拟真实实验的随机方位
            double phi_rand = dist(gen);
            double cos_phi = std::cos(phi_rand);
            double sin_phi = std::sin(phi_rand);
            
            // 绕 z 轴旋转: x' = x*cos - y*sin, y' = x*sin + y*cos, z' = z
            double pxp_rand = pxp * cos_phi - pyp * sin_phi;
            double pyp_rand = pxp * sin_phi + pyp * cos_phi;
            double pxn_rand = pxn * cos_phi - pyn * sin_phi;
            double pyn_rand = pxn * sin_phi + pyn * cos_phi;
            
            result.momenta.pxp.push_back(pxp_rand);
            result.momenta.pyp.push_back(pyp_rand);
            result.momenta.pzp.push_back(pzp);  // pz 不变
            result.momenta.pxn.push_back(pxn_rand);
            result.momenta.pyn.push_back(pyn_rand);
            result.momenta.pzn.push_back(pzn);  // pz 不变
        }
    }
    
    return result;
}

QMDMomentumCut::CutResult QMDMomentumCut::ApplyYpolCut(const MomentumData& data)
{
    CutResult result;
    result.momenta.reserve(data.size());
    
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
        
        // 旋转动量到反应平面（仅用于判断 cut 条件）
        double pxp_rot, pyp_rot, pxn_rot, pyn_rot;
        RotateMomentum(pxp, pyp, pxn, pyn, pxp_rot, pyp_rot, pxn_rot, pyn_rot);
        
        // Step 2: 旋转后条件
        double px_sum_rot = pxp_rot + pxn_rot;  // 无绝对值，与 notebook 一致
        double phi_cut = std::abs(M_PI - std::abs(phi_for_rotation));
        
        bool pass_step2 = (px_sum_rot < fYpolParams.pxSumAfterRotation) &&  // < 200
                          (phi_cut < fYpolParams.phiThreshold);              // < 0.2
        
        if (pass_step2) {
            result.passedIndices.push_back(i);
            
            // ypol 数据直接保存原始动量
            // QMD 数据生成时已经包含了随机方位角（rpphi），不需要额外旋转
            // ratio 计算时会临时旋转到反应平面
            result.momenta.pxp.push_back(pxp);
            result.momenta.pyp.push_back(pyp);
            result.momenta.pzp.push_back(pzp);
            result.momenta.pxn.push_back(pxn);
            result.momenta.pyn.push_back(pyn);
            result.momenta.pzn.push_back(pzn);
        }
    }
    
    return result;
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
    
    // std::vector<std::pair<double, std::string>> fieldFiles = {
    //     {0.8, "141114-0,8T-6000.table"},
    //     {1.0, "180626-1,00T-6000.table"},
    //     {1.2, "180626-1,20T-3000.table"},
    //     {1.4, "180703-1,40T-6000.table"},
        
    // };
   std::vector<std::pair<double, std::string>> fieldFiles = {
        {0.8, "141114-0,8T-3000.table"},
        {1.0, "180626-1,00T-3000.table"},
        {1.2, "180626-1,20T-3000.table"},
        {1.4, "180703-1,40T-3000.table"},
        {1.6, "180626-1,60T-3000.table"},
        {1.8, "180703-1,80T-3000.table"},
        {2.0, "180627-2,00T-3000.table"},
        {2.2, "180628-2,20T-3000.table"},
        {2.4, "180702-2,40T-3000.table"},
        {2.6, "180703-2,60T-3000.table"},
        {2.8, "180628-2,80T-3000.table"},
        {3.0, "180629-3,00T-3000.table"}
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

/**
 * @brief 加载 QMD 原始数据
 * 
 * 【重要】坐标系说明：
 * 读取的动量数据 (pxp, pyp, pzp, pxn, pyn, pzn) 是在【靶子局部坐标系】中定义的。
 * 
 * 靶子坐标系定义：
 *   - z 轴：沿靶子表面法向（即入射束流方向在靶子处的切线方向）
 *   - x 轴：垂直于 z 轴，在水平面内
 *   - y 轴：竖直向上
 * 
 * 与实验室坐标系的关系：
 *   靶子坐标系相对于实验室坐标系绕 y 轴旋转了 targetRotationAngle（通常为 -5°）。
 *   要将靶子系中的动量转换到实验室系，需要绕 y 轴旋转 +targetRotationAngle。
 * 
 * 后续处理：
 *   - 物理分析（如 pxp-pxn 分布）在靶子坐标系中进行，这是反应平面的自然定义。
 *   - 几何接收度检查（CheckPDCHit, CheckNEBULAHit）需要在实验室坐标系中进行，
 *     因为探测器位置是在实验室坐标系中定义的。
 * 
 * 数据格式：
 *   - zpol: 7 列 (No pxp pyp pzp pxn pyn pzn)，b 值在文件名或头信息中
 *   - ypol: 9 列 (No pxp pyp pzp pxn pyn pzn b rpphi)
 * 
 * @param target 靶材料名称 (如 "Pb208")
 * @param polType 极化类型 ("zpol" 或 "ypol")
 * @param gamma gamma 参数值 (如 "050")
 * @return MomentumData 包含所有事件动量的数据结构（靶子坐标系）
 */
MomentumData QMDGeoFilter::LoadQMDData(const std::string& target, 
                                        const std::string& polType,
                                        const std::string& gamma)
{
    MomentumData data;
    
    bool isZpol = (polType == "zpol" || polType == "z");
    
    // 数据文件结构：
    // - zpol (b_discrete): 12 个文件 dbreakb01.dat ~ dbreakb12.dat，只有 zpn 目录
    //   每个文件对应一个固定的 b 值
    // - ypol (phi_random): 需要读取 ynp 和 ypn 两个目录的 dbreak.dat
    //   ynp: 随机 y 方向极化（正负都有）
    //   ypn: 另一种 y 方向极化配置
    
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

/**
 * @brief 应用几何接收度筛选
 * 
 * 【坐标系转换说明】
 * 输入的动量数据 (data) 是在靶子坐标系中定义的。
 * 但是探测器 (PDC, NEBULA) 的位置是在实验室坐标系中定义的。
 * 因此在检查探测器 hit 之前，需要将动量从靶子坐标系旋转到实验室坐标系。
 * 
 * 旋转方法：绕 y 轴旋转 +targetRotationAngle
 *   px_lab = px_target * cos(angle) + pz_target * sin(angle)
 *   py_lab = py_target (不变)
 *   pz_lab = -px_target * sin(angle) + pz_target * cos(angle)
 * 
 * @param data 动量数据（靶子坐标系）
 * @param targetPos 靶位置（实验室坐标系）
 * @param targetRotationAngle 靶旋转角度（度），通常为负值如 -5°
 * @return 通过几何筛选的事件索引列表
 */
std::vector<int> QMDGeoFilter::ApplyGeometryFilter(const MomentumData& data,
                                                    const TVector3& targetPos,
                                                    double targetRotationAngle)
{
    // 使用详细版本并只返回 bothAccepted
    auto detailed = ApplyGeometryFilterDetailed(data, targetPos, targetRotationAngle);
    return detailed.bothAccepted;
}

GeometryFilterResult QMDGeoFilter::ApplyGeometryFilterDetailed(const MomentumData& data,
                                                                const TVector3& targetPos,
                                                                double targetRotationAngle)
{
    GeometryFilterResult result;
    
    // [EN] Use the already-set PDC configuration (either fixed or optimized)
    // [CN] 使用已经设置好的PDC配置（固定或优化的）
    // [EN] Note: The PDC config should be set in AnalyzeSingleConfiguration before calling this
    // [CN] 注意：PDC配置应该在调用此函数之前在AnalyzeSingleConfiguration中设置
    
    // [EN] If PDC config is not set, calculate it now (fallback for direct calls)
    // [CN] 如果PDC配置未设置，现在计算它（用于直接调用的回退）
    auto currentPDCConfig = fDetectorCalc->GetPDCConfiguration();
    if (!currentPDCConfig.isFixed && !currentPDCConfig.isOptimal) {
        // [EN] PDC config not set, calculate optimal position
        // [CN] PDC配置未设置，计算最佳位置
        auto pdcConfig = fDetectorCalc->CalculateOptimalPDCPosition(
            targetPos, targetRotationAngle, fConfig.pxRange);
        fDetectorCalc->SetPDCConfiguration(pdcConfig);
    }
    
    // [EN] Calculate rotation parameters: target frame -> lab frame
    // [CN] 计算旋转参数：从靶子坐标系到实验室坐标系
    // [EN] Use +targetRotationAngle (same as DetectorAcceptanceCalculator) / [CN] 使用+targetRotationAngle（与DetectorAcceptanceCalculator一致）
    double rotAngleRad = targetRotationAngle * TMath::DegToRad();
    double cosA = std::cos(rotAngleRad);
    double sinA = std::sin(rotAngleRad);
    
    // 检查每个事件
    for (size_t i = 0; i < data.size(); ++i) {
        // === 坐标系转换：靶子系 -> 实验室系 ===
        // 绕 y 轴旋转 targetRotationAngle
        double pxp_target = data.pxp[i];
        double pyp_target = data.pyp[i];
        double pzp_target = data.pzp[i];
        double pxn_target = data.pxn[i];
        double pyn_target = data.pyn[i];
        double pzn_target = data.pzn[i];
        
        // 旋转到实验室坐标系
        double pxp_lab = pxp_target * cosA + pzp_target * sinA;
        double pyp_lab = pyp_target;
        double pzp_lab = -pxp_target * sinA + pzp_target * cosA;
        
        double pxn_lab = pxn_target * cosA + pzn_target * sinA;
        double pyn_lab = pyn_target;
        double pzn_lab = -pxn_target * sinA + pzn_target * cosA;
        
        // 创建粒子信息（使用实验室坐标系的动量）
        DetectorAcceptanceCalculator::ParticleInfo proton, neutron;
        
        proton.pdgCode = 2212;  // proton
        proton.charge = 1.0;
        proton.mass = 938.272;
        proton.momentum.SetPxPyPzE(pxp_lab, pyp_lab, pzp_lab,
            std::sqrt(pxp_lab*pxp_lab + pyp_lab*pyp_lab + 
                     pzp_lab*pzp_lab + 938.272*938.272));
        proton.vertex = targetPos;
        
        neutron.pdgCode = 2112;  // neutron
        neutron.charge = 0.0;
        neutron.mass = 939.565;
        neutron.momentum.SetPxPyPzE(pxn_lab, pyn_lab, pzn_lab,
            std::sqrt(pxn_lab*pxn_lab + pyn_lab*pyn_lab + 
                     pzn_lab*pzn_lab + 939.565*939.565));
        neutron.vertex = targetPos;
        
        // 检查是否打到 PDC 和 NEBULA（实验室坐标系中检查）
        TVector3 hitPosPDC, hitPosNEBULA;
        bool pdcHit = fDetectorCalc->CheckPDCHit(proton, hitPosPDC);
        bool nebulaHit = fDetectorCalc->CheckNEBULAHit(neutron, hitPosNEBULA);
        
        // 分类
        if (pdcHit && nebulaHit) {
            result.bothAccepted.push_back(i);
        } else if (pdcHit && !nebulaHit) {
            result.pdcOnlyAccepted.push_back(i);
        } else if (!pdcHit && nebulaHit) {
            result.nebulaOnlyAccepted.push_back(i);
        } else {
            result.bothRejected.push_back(i);
        }
    }
    
    return result;
}

RatioResult QMDGeoFilter::CalculateRatio(const MomentumData& data,
                                          const std::vector<int>& indices)
{
    RatioResult result;
    
    // ratio = N(pxp_rot - pxn_rot > 0) / N(pxp_rot - pxn_rot < 0)
    // 其中 pxp_rot, pxn_rot 是旋转到反应平面后的动量
    // 旋转角度 phi = atan2(pyp + pyn, pxp + pxn)
    
    // 计算旋转后的 pxp - pxn
    auto getRotatedPxDiff = [](double pxp, double pyp, double pxn, double pyn) -> double {
        // 计算总动量的方位角
        double phi = std::atan2(pyp + pyn, pxp + pxn);
        double cosPhi = std::cos(phi);
        double sinPhi = std::sin(phi);
        
        // 旋转到反应平面 (φ = 0)
        double pxp_rot = pxp * cosPhi + pyp * sinPhi;
        double pxn_rot = pxn * cosPhi + pyn * sinPhi;
        
        return pxp_rot - pxn_rot;
    };
    
    if (indices.empty()) {
        // 使用所有数据
        if (data.size() == 0) return result;
        
        int nPositive = 0, nNegative = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            double diff = getRotatedPxDiff(data.pxp[i], data.pyp[i], 
                                           data.pxn[i], data.pyn[i]);
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
            double diff = getRotatedPxDiff(data.pxp[idx], data.pyp[idx], 
                                           data.pxn[idx], data.pyn[idx]);
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
    
    // [EN] Load magnetic field (if needed) / [CN] 加载磁场（如果需要）
    if (std::abs(fCurrentFieldStrength - fieldStrength) > 0.01) {
        if (!LoadFieldMap(fieldStrength)) {
            SM_ERROR("Failed to load field map for {}T", fieldStrength);
            return configResult;
        }
    }
    
    // [EN] Calculate target position / [CN] 计算 target 位置
    auto targetPos = fBeamCalc->CalculateTargetPosition(deflectionAngle);
    configResult.targetPosition = targetPos;
    
    // [EN] Get PDC configuration based on mode (fixed or optimized)
    // [CN] 根据模式获取 PDC 配置（固定或优化）
    TVector3 targetVec(targetPos.position.X(), targetPos.position.Y(), targetPos.position.Z());
    
    if (fConfig.useFixedPDC) {
        // [EN] Use fixed PDC position mode / [CN] 使用固定PDC位置模式
        // [EN] Convert SAMURAI definition to lab frame (rotate around origin) / [CN] 将SAMURAI定义转换到实验室坐标系（绕原点旋转）
        // [EN] SAMURAI uses clockwise positive, so lab rotation = -samuraiAngle / [CN] SAMURAI顺时针为正，因此实验室角度 = -samurai角度
        double samuraiAngle = fConfig.fixedPDCRotationAngle;
        TVector3 samuraiPos1 = fConfig.fixedPDCPosition1;
        TVector3 samuraiPos2 = fConfig.fixedPDCPosition2;
        
        // [EN] Optional macro override (e.g. B100T.mac) / [CN] 可选宏文件覆盖
        if (!fConfig.pdcMacroPath.empty()) {
            double macroAngle = 0.0;
            TVector3 pos1, pos2;
            if (ParsePdcMacro(fConfig.pdcMacroPath, macroAngle, pos1, pos2)) {
                samuraiAngle = macroAngle;
                samuraiPos1 = pos1;
                samuraiPos2 = pos2;
                SM_INFO("  Using PDC macro: {}", fConfig.pdcMacroPath);
                SM_INFO("    PDC angle (SAMURAI): {:.2f} deg", samuraiAngle);
                SM_INFO("    PDC pos1 (SAMURAI): ({:.1f}, {:.1f}, {:.1f}) mm",
                        pos1.X(), pos1.Y(), pos1.Z());
                SM_INFO("    PDC pos2 (SAMURAI): ({:.1f}, {:.1f}, {:.1f}) mm",
                        pos2.X(), pos2.Y(), pos2.Z());
            } else {
                SM_WARN("  Failed to parse PDC macro: {} (using fixed values)",
                        fConfig.pdcMacroPath);
            }
        }
        
        double labRotation = -samuraiAngle;
        TVector3 labPdcPos1 = RotateYDeg(samuraiPos1, labRotation);
        TVector3 labPdcPos2 = RotateYDeg(samuraiPos2, labRotation);
        SM_INFO("  Using FIXED PDC position mode (dual planes, require both hits)");
        SM_INFO("    PDC1 position (SAMURAI): ({:.1f}, {:.1f}, {:.1f}) mm", 
                samuraiPos1.X(), samuraiPos1.Y(), samuraiPos1.Z());
        SM_INFO("    PDC2 position (SAMURAI): ({:.1f}, {:.1f}, {:.1f}) mm", 
                samuraiPos2.X(), samuraiPos2.Y(), samuraiPos2.Z());
        SM_INFO("    PDC rotation (SAMURAI): {:.2f} deg", samuraiAngle);
        SM_INFO("    PDC1 position (LAB): ({:.1f}, {:.1f}, {:.1f}) mm",
                labPdcPos1.X(), labPdcPos1.Y(), labPdcPos1.Z());
        SM_INFO("    PDC2 position (LAB): ({:.1f}, {:.1f}, {:.1f}) mm",
                labPdcPos2.X(), labPdcPos2.Y(), labPdcPos2.Z());
        SM_INFO("    PDC rotation (LAB): {:.2f} deg", labRotation);
        SM_INFO("    Px range: +/-{:.1f} MeV/c", fConfig.pxRange);
        
        configResult.pdcConfig = fDetectorCalc->CreateFixedPDCConfiguration(
            labPdcPos1,
            labRotation,
            -fConfig.pxRange,
            fConfig.pxRange
        );
        configResult.pdcConfig2 = fDetectorCalc->CreateFixedPDCConfiguration(
            labPdcPos2,
            labRotation,
            -fConfig.pxRange,
            fConfig.pxRange
        );
        configResult.usePdcPair = true;
    } else {
        // [EN] Use optimized PDC position mode / [CN] 使用优化PDC位置模式
        SM_INFO("  Using OPTIMIZED PDC position mode (pxRange=+/-{:.1f})", fConfig.pxRange);
        configResult.pdcConfig = fDetectorCalc->CalculateOptimalPDCPosition(
            targetVec, targetPos.rotationAngle, fConfig.pxRange);
        configResult.usePdcPair = false;
    }
    
    // [EN] Set PDC configuration to detector calculator / [CN] 将PDC配置设置到探测器计算器
    if (configResult.usePdcPair) {
        fDetectorCalc->SetPDCConfigurationPair(configResult.pdcConfig, configResult.pdcConfig2, true);
    } else {
        fDetectorCalc->SetPDCConfiguration(configResult.pdcConfig);
    }
    configResult.nebulaConfig = fDetectorCalc->GetNEBULAConfiguration();
    
    // 创建动量 cut
    QMDMomentumCut momentumCut(polType);
    
    // 分析每个 gamma 值
    for (const auto& gamma : fConfig.gammaValues) {
        GammaAnalysisResult gammaResult;
        gammaResult.gamma = gamma;
        gammaResult.polType = polType;
        
        // 加载数据（原始靶子系动量）
        MomentumData rawData = LoadQMDData(target, polType, gamma);
        gammaResult.nBeforeCut = rawData.size();
        gammaResult.rawMomenta = rawData;
        
        if (rawData.size() == 0) {
            SM_WARN("No data for gamma={}", gamma);
            configResult.gammaResults[gamma] = gammaResult;
            continue;
        }
        
        // 计算 before cut 的 ratio（CalculateRatio 内部会旋转到反应平面）
        gammaResult.ratioBeforeCut = CalculateRatio(rawData, {});
        
        // 应用动量 cut（返回原始动量）
        auto cutResult = momentumCut.ApplyCut(rawData);
        gammaResult.nAfterCut = cutResult.passedIndices.size();
        gammaResult.afterCutMomenta = cutResult.momenta;  // 原始动量
        gammaResult.ratioAfterCut = CalculateRatio(cutResult.momenta, {});
        gammaResult.cutEfficiency = (double)gammaResult.nAfterCut / gammaResult.nBeforeCut;
        
        // 应用几何筛选（使用原始动量，内部转到 lab 坐标系检查 hit）
        // 使用详细版本获取分类结果
        auto geoResult = ApplyGeometryFilterDetailed(cutResult.momenta, targetVec, targetPos.rotationAngle);
        gammaResult.nAfterGeometry = geoResult.bothAccepted.size();
        gammaResult.ratioAfterGeometry = CalculateRatio(cutResult.momenta, geoResult.bothAccepted);
        gammaResult.geoEfficiency = gammaResult.nAfterCut > 0 ? 
            (double)gammaResult.nAfterGeometry / gammaResult.nAfterCut : 0;
        gammaResult.totalEfficiency = (double)gammaResult.nAfterGeometry / gammaResult.nBeforeCut;
        
        // 辅助函数：按索引列表提取动量数据
        auto extractMomenta = [&cutResult](const std::vector<int>& indices) -> MomentumData {
            MomentumData result;
            result.reserve(indices.size());
            for (int idx : indices) {
                result.pxp.push_back(cutResult.momenta.pxp[idx]);
                result.pyp.push_back(cutResult.momenta.pyp[idx]);
                result.pzp.push_back(cutResult.momenta.pzp[idx]);
                result.pxn.push_back(cutResult.momenta.pxn[idx]);
                result.pyn.push_back(cutResult.momenta.pyn[idx]);
                result.pzn.push_back(cutResult.momenta.pzn[idx]);
            }
            return result;
        };
        
        // 存储各分类的动量数据
        gammaResult.bothAcceptedMomenta = extractMomenta(geoResult.bothAccepted);
        gammaResult.pdcOnlyMomenta = extractMomenta(geoResult.pdcOnlyAccepted);
        gammaResult.nebulaOnlyMomenta = extractMomenta(geoResult.nebulaOnlyAccepted);
        gammaResult.bothRejectedMomenta = extractMomenta(geoResult.bothRejected);
        
        // 兼容旧接口：afterGeoMomenta = bothAccepted, rejectedByGeoMomenta = 其他所有
        gammaResult.afterGeoMomenta = gammaResult.bothAcceptedMomenta;
        
        // rejectedByGeoMomenta 包含所有未通过的（pdcOnly + nebulaOnly + bothRejected）
        gammaResult.rejectedByGeoMomenta.clear();
        for (const auto* src : {&gammaResult.pdcOnlyMomenta, 
                                &gammaResult.nebulaOnlyMomenta, 
                                &gammaResult.bothRejectedMomenta}) {
            for (size_t i = 0; i < src->size(); ++i) {
                gammaResult.rejectedByGeoMomenta.pxp.push_back(src->pxp[i]);
                gammaResult.rejectedByGeoMomenta.pyp.push_back(src->pyp[i]);
                gammaResult.rejectedByGeoMomenta.pzp.push_back(src->pzp[i]);
                gammaResult.rejectedByGeoMomenta.pxn.push_back(src->pxn[i]);
                gammaResult.rejectedByGeoMomenta.pyn.push_back(src->pyn[i]);
                gammaResult.rejectedByGeoMomenta.pzn.push_back(src->pzn[i]);
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
        SM_INFO("    Geometry: both={}, pdcOnly={}, nebulaOnly={}, none={}", 
                gammaResult.bothAcceptedMomenta.size(),
                gammaResult.pdcOnlyMomenta.size(),
                gammaResult.nebulaOnlyMomenta.size(),
                gammaResult.bothRejectedMomenta.size());
        
        configResult.gammaResults[gamma] = gammaResult;
        
        // 清理临时数据（保留存储的动量数据用于后续绘图）
        rawData.clear();
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
    
    // [EN] Configure GeoAcceptanceManager / [CN] 配置 GeoAcceptanceManager
    GeoAcceptanceManager::AnalysisConfig geoConfig;
    geoConfig.fieldStrengths = {fieldStrength};
    geoConfig.deflectionAngles = {deflectionAngle};
    geoConfig.fieldMapFiles = {GetFieldMapFile(fieldStrength, fConfig.fieldMapPath)};
    geoConfig.outputPath = fConfig.outputPath;
    
    // [EN] Pass PDC configuration from QMDGeoFilter config / [CN] 从QMDGeoFilter配置传递PDC配置
    geoConfig.useFixedPDC = fConfig.useFixedPDC;
    geoConfig.fixedPDCPosition1 = fConfig.fixedPDCPosition1;
    geoConfig.fixedPDCPosition2 = fConfig.fixedPDCPosition2;
    geoConfig.fixedPDCRotationAngle = fConfig.fixedPDCRotationAngle;
    geoConfig.pxRange = fConfig.pxRange;
    
    if (geoConfig.useFixedPDC && !fConfig.pdcMacroPath.empty()) {
        double macroAngle = 0.0;
        TVector3 pos1, pos2;
        if (ParsePdcMacro(fConfig.pdcMacroPath, macroAngle, pos1, pos2)) {
            // [EN] Sync geometry plot with macro-defined PDC / [CN] 用宏定义同步几何绘图的PDC
            geoConfig.fixedPDCPosition1 = pos1;
            geoConfig.fixedPDCPosition2 = pos2;
            geoConfig.fixedPDCRotationAngle = macroAngle;
            SM_INFO("Geometry plot uses PDC macro: {}", fConfig.pdcMacroPath);
        } else {
            SM_WARN("Geometry plot: failed to parse PDC macro: {}", fConfig.pdcMacroPath);
        }
    }
    
    fGeoManager->SetConfig(geoConfig);
    
    // [EN] Configure track plotting with deuteron + proton tracks
    // [CN] 配置轨迹绘图：氘核 + 质子轨迹
    TrackPlotConfig trackConfig;
    trackConfig.drawDeuteronTrack = true;       // [EN] Draw deuteron beam / [CN] 绘制氘核束流
    trackConfig.drawCenterProtonTrack = true;   // [EN] Draw Px=0 proton / [CN] 绘制Px=0质子
    trackConfig.pxEdgeValues = {100.0, fConfig.pxRange};  // [EN] Default 100 + configured range
    trackConfig.pzProton = 627.0;
    fGeoManager->SetTrackPlotConfig(trackConfig);
    
    // [EN] Analyze field configuration / [CN] 分析磁场配置
    fGeoManager->AnalyzeFieldConfiguration(
        GetFieldMapFile(fieldStrength, fConfig.fieldMapPath), 
        fieldStrength
    );
    
    // [EN] Get results and generate plot with tracks / [CN] 获取结果并生成带轨迹的图
    auto results = fGeoManager->GetResults();
    if (!results.empty()) {
        fGeoManager->GenerateSingleConfigPlotWithTracks(results[0], outputFile, trackConfig);
    } else {
        // [EN] Fallback to basic geometry plot / [CN] 回退到基本几何图
        fGeoManager->GenerateGeometryPlot(outputFile);
    }
}

void QMDGeoFilter::GeneratePlots(const QMDConfigurationResult& result,
                                  const std::string& outputDir)
{
    // 为每个 gamma 生成详细图表
    for (const auto& [gamma, gammaResult] : result.gammaResults) {
        std::string gammaDir = outputDir + "gamma_" + gamma + "/";
        CreateOutputDirectory(gammaDir);
        
        // 生成 pxp-pxn 分布图（三个阶段）
        // 注意：GeneratePxpPxnPlot 内部会旋转到反应平面计算 pxp - pxn
        GeneratePxpPxnPlot(gammaResult.rawMomenta, 
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
        // 注意：3D 图使用原始动量（随机方位角），真实反映探测器覆盖
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
        
        // 标签格式: "1.00T_5deg_Pb208_ypol"
        std::ostringstream labelStream;
        labelStream << std::fixed << std::setprecision(2) << result.fieldStrength << "T_"
                    << std::fixed << std::setprecision(0) << result.deflectionAngle << "deg_"
                    << result.target << "_" << result.polType;
        std::string label = labelStream.str();
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
    
    // 计算旋转后的 pxp - pxn（旋转到反应平面 φ = 0）
    // phi = atan2(pyp + pyn, pxp + pxn)
    auto getRotatedPxDiff = [](double pxp, double pyp, double pxn, double pyn) -> double {
        double phi = std::atan2(pyp + pyn, pxp + pxn);
        double cosPhi = std::cos(phi);
        double sinPhi = std::sin(phi);
        double pxp_rot = pxp * cosPhi + pyp * sinPhi;
        double pxn_rot = pxn * cosPhi + pyn * sinPhi;
        return pxp_rot - pxn_rot;
    };
    
    // 计算 pxp - pxn 的范围（旋转后）
    double minDiff = 1e10, maxDiff = -1e10;
    for (size_t i = 0; i < data.size(); ++i) {
        double diff = getRotatedPxDiff(data.pxp[i], data.pyp[i], data.pxn[i], data.pyn[i]);
        minDiff = std::min(minDiff, diff);
        maxDiff = std::max(maxDiff, diff);
    }
    
    // 扩展范围
    double range = maxDiff - minDiff;
    minDiff -= range * 0.1;
    maxDiff += range * 0.1;
    
    // 创建直方图
    auto h_diff = new TH1D("h_pxp_pxn", (title + ";p_{x,p} - p_{x,n} (rotated) [MeV/c];Counts").c_str(),
                           100, minDiff, maxDiff);
    
    int nPositive = 0, nNegative = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        double diff = getRotatedPxDiff(data.pxp[i], data.pyp[i], data.pxn[i], data.pyn[i]);
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
    
    // 计算旋转后的 pxp - pxn（旋转到反应平面 φ = 0）
    auto getRotatedPxDiff = [](double pxp, double pyp, double pxn, double pyn) -> double {
        double phi = std::atan2(pyp + pyn, pxp + pxn);
        double cosPhi = std::cos(phi);
        double sinPhi = std::sin(phi);
        double pxp_rot = pxp * cosPhi + pyp * sinPhi;
        double pxn_rot = pxn * cosPhi + pyn * sinPhi;
        return pxp_rot - pxn_rot;
    };
    
    // 计算全局范围（使用旋转后的 diff）
    double minDiff = 1e10, maxDiff = -1e10;
    
    auto updateRange = [&](const MomentumData& data) {
        for (size_t i = 0; i < data.size(); ++i) {
            double diff = getRotatedPxDiff(data.pxp[i], data.pyp[i], data.pxn[i], data.pyn[i]);
            minDiff = std::min(minDiff, diff);
            maxDiff = std::max(maxDiff, diff);
        }
    };
    
    updateRange(gammaResult.rawMomenta);
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
                          (stageName + ";p_{x,p} - p_{x,n} (rotated) [MeV/c];Counts").c_str(),
                          50, minDiff, maxDiff);
        
        for (size_t i = 0; i < data.size(); ++i) {
            double diff = getRotatedPxDiff(data.pxp[i], data.pyp[i], data.pxn[i], data.pyn[i]);
            h->Fill(diff);
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
    
    drawStage(1, gammaResult.rawMomenta, 
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
// 3D 动量分布对比图 (geometry 分类显示)
// [EN] Color coding: / [CN] 颜色说明：
//   - Green: Both PDC and NEBULA accepted (bothAccepted) / 绿色: PDC 和 NEBULA 都接受
//   - Red: Only PDC accepted proton (pdcOnly) / 红色: 只有 PDC 接受 proton
//   - Blue: Only NEBULA accepted neutron (nebulaOnly) / 蓝色: 只有 NEBULA 接受 neutron
//   - Gray: Neither accepted (bothRejected) / 灰色: 都不接受
// =============================================================================

void QMDGeoFilter::Generate3DMomentumComparisonPlot(const GammaAnalysisResult& gammaResult,
                                                     const std::string& outputFile)
{
    // 清除残留
    gROOT->GetListOfSpecials()->Clear();
    
    // 创建 1x2 的 canvas：
    // 左：Proton (绿色=both, 红色=pdcOnly, 灰色=rejected)
    // 右：Neutron (绿色=both, 蓝色=nebulaOnly, 灰色=rejected)
    auto canvas = new TCanvas("c_3d_geo", "3D Momentum: Geometry Classification", 1600, 800);
    canvas->Clear();
    canvas->Divide(2, 1);
    
    // 获取各分类数据
    const MomentumData& both = gammaResult.bothAcceptedMomenta;
    const MomentumData& pdcOnly = gammaResult.pdcOnlyMomenta;
    const MomentumData& nebulaOnly = gammaResult.nebulaOnlyMomenta;
    const MomentumData& none = gammaResult.bothRejectedMomenta;
    
    // 计算全局范围 (proton)
    auto getRangeMulti = [](const std::vector<std::vector<double>*>& vecs, 
                            double& minVal, double& maxVal) {
        minVal = 1e10; maxVal = -1e10;
        for (const auto* v : vecs) {
            for (double x : *v) { 
                minVal = std::min(minVal, x); 
                maxVal = std::max(maxVal, x); 
            }
        }
        double range = maxVal - minVal;
        if (range < 1e-10) range = 200;
        minVal -= range * 0.1;
        maxVal += range * 0.1;
    };
    
    double pxpMin, pxpMax, pypMin, pypMax, pzpMin, pzpMax;
    std::vector<double> allPxp, allPyp, allPzp;
    for (const auto* data : {&both, &pdcOnly, &nebulaOnly, &none}) {
        allPxp.insert(allPxp.end(), data->pxp.begin(), data->pxp.end());
        allPyp.insert(allPyp.end(), data->pyp.begin(), data->pyp.end());
        allPzp.insert(allPzp.end(), data->pzp.begin(), data->pzp.end());
    }
    getRangeMulti({&allPxp}, pxpMin, pxpMax);
    getRangeMulti({&allPyp}, pypMin, pypMax);
    getRangeMulti({&allPzp}, pzpMin, pzpMax);
    
    double pxnMin, pxnMax, pynMin, pynMax, pznMin, pznMax;
    std::vector<double> allPxn, allPyn, allPzn;
    for (const auto* data : {&both, &pdcOnly, &nebulaOnly, &none}) {
        allPxn.insert(allPxn.end(), data->pxn.begin(), data->pxn.end());
        allPyn.insert(allPyn.end(), data->pyn.begin(), data->pyn.end());
        allPzn.insert(allPzn.end(), data->pzn.begin(), data->pzn.end());
    }
    getRangeMulti({&allPxn}, pxnMin, pxnMax);
    getRangeMulti({&allPyn}, pynMin, pynMax);
    getRangeMulti({&allPzn}, pznMin, pznMax);
    
    // ========== Proton 图 ==========
    canvas->cd(1);
    gPad->Clear();
    
    // 创建用于确定坐标轴的空 TH3D
    auto h3_proton_frame = new TH3D("h3_p_frame", 
        Form("Proton Momentum;p_{x} [MeV/c];p_{y} [MeV/c];p_{z} [MeV/c]"),
        2, pxpMin, pxpMax, 2, pypMin, pypMax, 2, pzpMin, pzpMax);
    h3_proton_frame->SetStats(0);
    h3_proton_frame->Draw();
    
    // [EN] Green: bothAccepted / [CN] 绿色: 都接受
    auto pm_p_both = new TPolyMarker3D(both.size(), 8);  // 8 = full circle
    pm_p_both->SetMarkerColor(kGreen+2);
    pm_p_both->SetMarkerSize(0.5);
    for (size_t i = 0; i < both.size(); ++i) {
        pm_p_both->SetPoint(i, both.pxp[i], both.pyp[i], both.pzp[i]);
    }
    pm_p_both->Draw();
    
    // [EN] Red: pdcOnly (proton accepted by PDC) / [CN] 红色: 只有PDC接受
    auto pm_p_pdcOnly = new TPolyMarker3D(pdcOnly.size(), 8);
    pm_p_pdcOnly->SetMarkerColor(kRed);
    pm_p_pdcOnly->SetMarkerSize(0.5);
    for (size_t i = 0; i < pdcOnly.size(); ++i) {
        pm_p_pdcOnly->SetPoint(i, pdcOnly.pxp[i], pdcOnly.pyp[i], pdcOnly.pzp[i]);
    }
    pm_p_pdcOnly->Draw();
    
    // [EN] Gray: bothRejected + nebulaOnly (proton not accepted by PDC)
    // [CN] 灰色: 都不接受 + 只有NEBULA接受（proton不被PDC接受）
    size_t nRejectedP = none.size() + nebulaOnly.size();
    auto pm_p_rejected = new TPolyMarker3D(nRejectedP, 8);
    pm_p_rejected->SetMarkerColor(kGray+1);
    pm_p_rejected->SetMarkerSize(0.3);
    size_t idx = 0;
    for (size_t i = 0; i < none.size(); ++i, ++idx) {
        pm_p_rejected->SetPoint(idx, none.pxp[i], none.pyp[i], none.pzp[i]);
    }
    for (size_t i = 0; i < nebulaOnly.size(); ++i, ++idx) {
        pm_p_rejected->SetPoint(idx, nebulaOnly.pxp[i], nebulaOnly.pyp[i], nebulaOnly.pzp[i]);
    }
    pm_p_rejected->Draw();
    
    // 图例
    auto leg_p = new TLegend(0.60, 0.70, 0.95, 0.95);
    leg_p->SetHeader("Proton (PDC)", "C");
    leg_p->AddEntry(pm_p_both, Form("Both accepted (%zu) - Green", both.size()), "p");
    leg_p->AddEntry(pm_p_pdcOnly, Form("PDC only (%zu) - Red", pdcOnly.size()), "p");
    leg_p->AddEntry(pm_p_rejected, Form("PDC rejected (%zu) - Gray", nRejectedP), "p");
    leg_p->Draw();
    
    // ========== Neutron 图 ==========
    canvas->cd(2);
    gPad->Clear();
    
    auto h3_neutron_frame = new TH3D("h3_n_frame", 
        Form("Neutron Momentum;p_{x} [MeV/c];p_{y} [MeV/c];p_{z} [MeV/c]"),
        2, pxnMin, pxnMax, 2, pynMin, pynMax, 2, pznMin, pznMax);
    h3_neutron_frame->SetStats(0);
    h3_neutron_frame->Draw();
    
    // [EN] Green: bothAccepted / [CN] 绿色: 都接受
    auto pm_n_both = new TPolyMarker3D(both.size(), 8);
    pm_n_both->SetMarkerColor(kGreen+2);
    pm_n_both->SetMarkerSize(0.5);
    for (size_t i = 0; i < both.size(); ++i) {
        pm_n_both->SetPoint(i, both.pxn[i], both.pyn[i], both.pzn[i]);
    }
    pm_n_both->Draw();
    
    // [EN] Blue: nebulaOnly (neutron accepted by NEBULA) / [CN] 蓝色: 只有NEBULA接受
    auto pm_n_nebulaOnly = new TPolyMarker3D(nebulaOnly.size(), 8);
    pm_n_nebulaOnly->SetMarkerColor(kBlue);
    pm_n_nebulaOnly->SetMarkerSize(0.5);
    for (size_t i = 0; i < nebulaOnly.size(); ++i) {
        pm_n_nebulaOnly->SetPoint(i, nebulaOnly.pxn[i], nebulaOnly.pyn[i], nebulaOnly.pzn[i]);
    }
    pm_n_nebulaOnly->Draw();
    
    // [EN] Gray: bothRejected + pdcOnly (neutron not accepted by NEBULA)
    // [CN] 灰色: 都不接受 + 只有PDC接受（neutron不被NEBULA接受）
    size_t nRejectedN = none.size() + pdcOnly.size();
    auto pm_n_rejected = new TPolyMarker3D(nRejectedN, 8);
    pm_n_rejected->SetMarkerColor(kGray+1);
    pm_n_rejected->SetMarkerSize(0.3);
    idx = 0;
    for (size_t i = 0; i < none.size(); ++i, ++idx) {
        pm_n_rejected->SetPoint(idx, none.pxn[i], none.pyn[i], none.pzn[i]);
    }
    for (size_t i = 0; i < pdcOnly.size(); ++i, ++idx) {
        pm_n_rejected->SetPoint(idx, pdcOnly.pxn[i], pdcOnly.pyn[i], pdcOnly.pzn[i]);
    }
    pm_n_rejected->Draw();
    
    // 图例
    auto leg_n = new TLegend(0.60, 0.70, 0.95, 0.95);
    leg_n->SetHeader("Neutron (NEBULA)", "C");
    leg_n->AddEntry(pm_n_both, Form("Both accepted (%zu) - Green", both.size()), "p");
    leg_n->AddEntry(pm_n_nebulaOnly, Form("NEBULA only (%zu) - Blue", nebulaOnly.size()), "p");
    leg_n->AddEntry(pm_n_rejected, Form("NEBULA rejected (%zu) - Gray", nRejectedN), "p");
    leg_n->Draw();
    
    canvas->SaveAs(outputFile.c_str());
    
    // 清理
    delete h3_proton_frame;
    delete h3_neutron_frame;
    delete pm_p_both;
    delete pm_p_pdcOnly;
    delete pm_p_rejected;
    delete pm_n_both;
    delete pm_n_nebulaOnly;
    delete pm_n_rejected;
    delete leg_p;
    delete leg_n;
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
    
    // [EN] 2x3 canvas: / [CN] 2x3 画布：
    // [EN] Top row: Proton (px vs py, px vs pz, py vs pz) / [CN] 上排：Proton
    // [EN] Bottom row: Neutron (px vs py, px vs pz, py vs pz) / [CN] 下排：Neutron
    // [EN] Colors: Green=both, Blue=NEBULA only, Red=PDC only, Gray=none
    // [CN] 颜色：绿色=都接受，蓝色=只有NEBULA接受，红色=只有PDC接受，灰色=都不接受
    auto canvas = new TCanvas("c_2d_proj", "2D Projections: Geometry Classification", 1500, 1000);
    canvas->Clear();
    canvas->Divide(3, 2);
    
    // 获取各分类数据
    const MomentumData& both = gammaResult.bothAcceptedMomenta;
    const MomentumData& pdcOnly = gammaResult.pdcOnlyMomenta;
    const MomentumData& nebulaOnly = gammaResult.nebulaOnlyMomenta;
    const MomentumData& none = gammaResult.bothRejectedMomenta;
    
    // 辅助函数：创建 TGraph 并设置样式
    auto makeGraph = [](const std::vector<double>& x, const std::vector<double>& y, 
                        int color, int style = 20, double size = 0.3) -> TGraph* {
        auto gr = new TGraph(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            gr->SetPoint(i, x[i], y[i]);
        }
        gr->SetMarkerColor(color);
        gr->SetMarkerStyle(style);
        gr->SetMarkerSize(size);
        return gr;
    };
    
    // 辅助函数：合并两个 vector
    auto concat = [](const std::vector<double>& a, const std::vector<double>& b) {
        std::vector<double> result = a;
        result.insert(result.end(), b.begin(), b.end());
        return result;
    };
    
    // ===== Proton px vs py =====
    canvas->cd(1);
    gPad->Clear();
    
    // [EN] Gray: none (neither accepted) / [CN] 灰色: 都不接受
    auto gr_p_none_xy = makeGraph(none.pxp, none.pyp, kGray+1);
    gr_p_none_xy->SetTitle("Proton p_{x} vs p_{y};p_{x} [MeV/c];p_{y} [MeV/c]");
    gr_p_none_xy->Draw("AP");
    
    // [EN] Blue: nebulaOnly (proton not accepted by PDC, but neutron accepted by NEBULA)
    // [CN] 蓝色: 只有NEBULA接受
    auto gr_p_neb_xy = makeGraph(nebulaOnly.pxp, nebulaOnly.pyp, kBlue);
    gr_p_neb_xy->Draw("P SAME");
    
    // [EN] Red: pdcOnly (proton accepted by PDC) / [CN] 红色: 只有PDC接受
    auto gr_p_pdc_xy = makeGraph(pdcOnly.pxp, pdcOnly.pyp, kRed);
    gr_p_pdc_xy->Draw("P SAME");
    
    // [EN] Green: both / [CN] 绿色: 都接受
    auto gr_p_both_xy = makeGraph(both.pxp, both.pyp, kGreen+2);
    gr_p_both_xy->Draw("P SAME");
    
    auto leg1 = new TLegend(0.60, 0.68, 0.95, 0.95);
    leg1->AddEntry(gr_p_both_xy, Form("Both (%zu) Green", both.size()), "p");
    leg1->AddEntry(gr_p_pdc_xy, Form("PDC only (%zu) Red", pdcOnly.size()), "p");
    leg1->AddEntry(gr_p_neb_xy, Form("NEBULA only (%zu) Blue", nebulaOnly.size()), "p");
    leg1->AddEntry(gr_p_none_xy, Form("None (%zu) Gray", none.size()), "p");
    leg1->Draw();
    
    // ===== Proton px vs pz =====
    canvas->cd(2);
    gPad->Clear();
    
    auto gr_p_none_xz = makeGraph(none.pxp, none.pzp, kGray+1);
    gr_p_none_xz->SetTitle("Proton p_{x} vs p_{z};p_{x} [MeV/c];p_{z} [MeV/c]");
    gr_p_none_xz->Draw("AP");
    
    auto gr_p_neb_xz = makeGraph(nebulaOnly.pxp, nebulaOnly.pzp, kBlue);
    gr_p_neb_xz->Draw("P SAME");
    
    auto gr_p_pdc_xz = makeGraph(pdcOnly.pxp, pdcOnly.pzp, kRed);
    gr_p_pdc_xz->Draw("P SAME");
    
    auto gr_p_both_xz = makeGraph(both.pxp, both.pzp, kGreen+2);
    gr_p_both_xz->Draw("P SAME");
    
    // ===== Proton py vs pz =====
    canvas->cd(3);
    gPad->Clear();
    
    auto gr_p_none_yz = makeGraph(none.pyp, none.pzp, kGray+1);
    gr_p_none_yz->SetTitle("Proton p_{y} vs p_{z};p_{y} [MeV/c];p_{z} [MeV/c]");
    gr_p_none_yz->Draw("AP");
    
    auto gr_p_neb_yz = makeGraph(nebulaOnly.pyp, nebulaOnly.pzp, kBlue);
    gr_p_neb_yz->Draw("P SAME");
    
    auto gr_p_pdc_yz = makeGraph(pdcOnly.pyp, pdcOnly.pzp, kRed);
    gr_p_pdc_yz->Draw("P SAME");
    
    auto gr_p_both_yz = makeGraph(both.pyp, both.pzp, kGreen+2);
    gr_p_both_yz->Draw("P SAME");
    
    // ===== Neutron px vs py =====
    canvas->cd(4);
    gPad->Clear();
    
    // [EN] Gray: none (neither accepted) / [CN] 灰色: 都不接受
    auto gr_n_none_xy = makeGraph(none.pxn, none.pyn, kGray+1);
    gr_n_none_xy->SetTitle("Neutron p_{x} vs p_{y};p_{x} [MeV/c];p_{y} [MeV/c]");
    gr_n_none_xy->Draw("AP");
    
    // [EN] Red: pdcOnly (neutron not accepted by NEBULA, but proton accepted by PDC)
    // [CN] 红色: 只有PDC接受
    auto gr_n_pdc_xy = makeGraph(pdcOnly.pxn, pdcOnly.pyn, kRed);
    gr_n_pdc_xy->Draw("P SAME");
    
    // [EN] Blue: nebulaOnly (neutron accepted by NEBULA) / [CN] 蓝色: 只有NEBULA接受
    auto gr_n_neb_xy = makeGraph(nebulaOnly.pxn, nebulaOnly.pyn, kBlue);
    gr_n_neb_xy->Draw("P SAME");
    
    // [EN] Green: both / [CN] 绿色: 都接受
    auto gr_n_both_xy = makeGraph(both.pxn, both.pyn, kGreen+2);
    gr_n_both_xy->Draw("P SAME");
    
    auto leg4 = new TLegend(0.60, 0.68, 0.95, 0.95);
    leg4->AddEntry(gr_n_both_xy, Form("Both (%zu) Green", both.size()), "p");
    leg4->AddEntry(gr_n_neb_xy, Form("NEBULA only (%zu) Blue", nebulaOnly.size()), "p");
    leg4->AddEntry(gr_n_pdc_xy, Form("PDC only (%zu) Red", pdcOnly.size()), "p");
    leg4->AddEntry(gr_n_none_xy, Form("None (%zu) Gray", none.size()), "p");
    leg4->Draw();
    
    // ===== Neutron px vs pz =====
    canvas->cd(5);
    gPad->Clear();
    
    auto gr_n_none_xz = makeGraph(none.pxn, none.pzn, kGray+1);
    gr_n_none_xz->SetTitle("Neutron p_{x} vs p_{z};p_{x} [MeV/c];p_{z} [MeV/c]");
    gr_n_none_xz->Draw("AP");
    
    auto gr_n_pdc_xz = makeGraph(pdcOnly.pxn, pdcOnly.pzn, kRed);
    gr_n_pdc_xz->Draw("P SAME");
    
    auto gr_n_neb_xz = makeGraph(nebulaOnly.pxn, nebulaOnly.pzn, kBlue);
    gr_n_neb_xz->Draw("P SAME");
    
    auto gr_n_both_xz = makeGraph(both.pxn, both.pzn, kGreen+2);
    gr_n_both_xz->Draw("P SAME");
    
    // ===== Neutron py vs pz =====
    canvas->cd(6);
    gPad->Clear();
    
    auto gr_n_none_yz = makeGraph(none.pyn, none.pzn, kGray+1);
    gr_n_none_yz->SetTitle("Neutron p_{y} vs p_{z};p_{y} [MeV/c];p_{z} [MeV/c]");
    gr_n_none_yz->Draw("AP");
    
    auto gr_n_pdc_yz = makeGraph(pdcOnly.pyn, pdcOnly.pzn, kRed);
    gr_n_pdc_yz->Draw("P SAME");
    
    auto gr_n_neb_yz = makeGraph(nebulaOnly.pyn, nebulaOnly.pzn, kBlue);
    gr_n_neb_yz->Draw("P SAME");
    
    auto gr_n_both_yz = makeGraph(both.pyn, both.pzn, kGreen+2);
    gr_n_both_yz->Draw("P SAME");
    
    canvas->SaveAs(outputFile.c_str());
    delete canvas;
}

//==============================================================================
// GenerateInteractive3DPlot - 生成交互式 HTML 3D 散点图 (使用 Three.js)
//==============================================================================
void QMDGeoFilter::GenerateInteractive3DPlot(const GammaAnalysisResult& gammaResult,
                                              const std::string& outputFile)
{
    // 获取各分类数据
    const auto& both = gammaResult.bothAcceptedMomenta;
    const auto& pdcOnly = gammaResult.pdcOnlyMomenta;
    const auto& nebulaOnly = gammaResult.nebulaOnlyMomenta;
    const auto& none = gammaResult.bothRejectedMomenta;
    
    std::ofstream html(outputFile);
    if (!html.is_open()) {
        SM_WARN("Failed to create HTML file: {}", outputFile);
        return;
    }
    
    // 计算数据范围用于归一化
    double maxP = 0;
    for (const auto* data : {&both, &pdcOnly, &nebulaOnly, &none}) {
        for (size_t i = 0; i < data->size(); ++i) {
            maxP = std::max(maxP, std::abs(data->pxp[i]));
            maxP = std::max(maxP, std::abs(data->pyp[i]));
            maxP = std::max(maxP, std::abs(data->pzp[i]));
            maxP = std::max(maxP, std::abs(data->pxn[i]));
            maxP = std::max(maxP, std::abs(data->pyn[i]));
            maxP = std::max(maxP, std::abs(data->pzn[i]));
        }
    }
    if (maxP < 1e-10) maxP = 200;  // 防止除以零
    double scale = 5.0 / maxP;  // 归一化到 [-5, 5]
    
    size_t totalEvents = both.size() + pdcOnly.size() + nebulaOnly.size() + none.size();
    double geoEff = totalEvents > 0 ? 100.0 * both.size() / totalEvents : 0;
    
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
    html << ".sidebar { width: 320px; padding: 20px; background: rgba(0,0,0,0.3); border-left: 1px solid rgba(255,255,255,0.1); overflow-y: auto; }\n";
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
    html << "<div class=\"canvas-wrapper\" id=\"proton-container\"><div class=\"canvas-label\">Proton (PDC acceptance)</div></div>\n";
    html << "<div class=\"canvas-wrapper\" id=\"neutron-container\"><div class=\"canvas-label\">Neutron (NEBULA acceptance)</div></div>\n";
    html << "<div class=\"canvas-wrapper\" id=\"combined-container\"><div class=\"canvas-label\">Combined View</div></div>\n";
    html << "<div class=\"canvas-wrapper\" id=\"diff-container\"><div class=\"canvas-label\">Delta p = pp - pn</div></div>\n";
    html << "</div>\n";
    
    // Sidebar
    html << "<div class=\"sidebar\">\n";
    html << "<div class=\"legend\"><h3>Legend</h3>\n";
    html << "<div class=\"legend-item\"><div class=\"legend-color\" style=\"background: #00ff88;\"></div><span>Both Accepted (" << both.size() << ") - Green</span></div>\n";
    html << "<div class=\"legend-item\"><div class=\"legend-color\" style=\"background: #ff4444;\"></div><span>PDC only (" << pdcOnly.size() << ") - Red</span></div>\n";
    html << "<div class=\"legend-item\"><div class=\"legend-color\" style=\"background: #4488ff;\"></div><span>NEBULA only (" << nebulaOnly.size() << ") - Blue</span></div>\n";
    html << "<div class=\"legend-item\"><div class=\"legend-color\" style=\"background: #888888;\"></div><span>Both Rejected (" << none.size() << ") - Gray</span></div>\n";
    html << "</div>\n";
    html << "<div class=\"stats\"><h3>Statistics</h3>\n";
    html << "<div class=\"stat-row\"><span>Both Accepted:</span><span class=\"stat-value\">" << both.size() << "</span></div>\n";
    html << "<div class=\"stat-row\"><span>PDC Only:</span><span class=\"stat-value\">" << pdcOnly.size() << "</span></div>\n";
    html << "<div class=\"stat-row\"><span>NEBULA Only:</span><span class=\"stat-value\">" << nebulaOnly.size() << "</span></div>\n";
    html << "<div class=\"stat-row\"><span>Both Rejected:</span><span class=\"stat-value\">" << none.size() << "</span></div>\n";
    html << "<div class=\"stat-row\"><span>Geo Efficiency:</span><span class=\"stat-value\">" << std::fixed << std::setprecision(1) << geoEff << "%</span></div>\n";
    html << "<div class=\"stat-row\"><span>Ratio (after geo):</span><span class=\"stat-value\">" << std::fixed << std::setprecision(3) << gammaResult.ratioAfterGeometry.ratio << "</span></div>\n";
    html << "</div>\n";
    html << "<div class=\"controls\"><h3>Controls</h3>\n";
    html << "<div class=\"control-group\"><label>Point Size</label><input type=\"range\" class=\"slider\" id=\"pointSize\" min=\"1\" max=\"10\" value=\"3\"></div>\n";
    html << "<div class=\"control-group\"><label>Point Opacity</label><input type=\"range\" class=\"slider\" id=\"opacity\" min=\"10\" max=\"100\" value=\"70\"></div>\n";
    html << "<div class=\"checkbox-group\"><input type=\"checkbox\" id=\"showBoth\" checked><label for=\"showBoth\">Show Both Accepted (Green)</label></div>\n";
    html << "<div class=\"checkbox-group\"><input type=\"checkbox\" id=\"showPDCOnly\" checked><label for=\"showPDCOnly\">Show PDC Only (Red)</label></div>\n";
    html << "<div class=\"checkbox-group\"><input type=\"checkbox\" id=\"showNEBULAOnly\" checked><label for=\"showNEBULAOnly\">Show NEBULA Only (Blue)</label></div>\n";
    html << "<div class=\"checkbox-group\"><input type=\"checkbox\" id=\"showRejected\" checked><label for=\"showRejected\">Show Rejected (Gray)</label></div>\n";
    html << "<div class=\"checkbox-group\"><input type=\"checkbox\" id=\"autoRotate\"><label for=\"autoRotate\">Auto Rotate</label></div>\n";
    html << "<button class=\"btn\" id=\"resetBtn\">Reset View</button>\n";
    html << "<button class=\"btn secondary\" id=\"syncBtn\">Sync All Views</button>\n";
    html << "</div></div></div></div>\n";
    
    // Scripts
    html << "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js\"></script>\n";
    html << "<script src=\"https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js\"></script>\n";
    html << "<script>\n";
    
    // Output data - 分类输出
    // bothProton (绿色)
    html << "const bothProton = [\n";
    for (size_t i = 0; i < both.size(); ++i) {
        html << "[" << both.pxp[i] * scale << "," << both.pyp[i] * scale << "," << both.pzp[i] * scale << "]";
        if (i < both.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    // pdcOnlyProton (红色 - PDC only proton)
    html << "const pdcOnlyProton = [\n";
    for (size_t i = 0; i < pdcOnly.size(); ++i) {
        html << "[" << pdcOnly.pxp[i] * scale << "," << pdcOnly.pyp[i] * scale << "," << pdcOnly.pzp[i] * scale << "]";
        if (i < pdcOnly.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    // nebulaOnlyProton (蓝色 - NEBULA only 的 proton, 即 proton 未被 PDC 接受)
    html << "const nebulaOnlyProton = [\n";
    for (size_t i = 0; i < nebulaOnly.size(); ++i) {
        html << "[" << nebulaOnly.pxp[i] * scale << "," << nebulaOnly.pyp[i] * scale << "," << nebulaOnly.pzp[i] * scale << "]";
        if (i < nebulaOnly.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    // noneProton (灰色 - 都不接受的 proton)
    html << "const noneProton = [\n";
    for (size_t i = 0; i < none.size(); ++i) {
        html << "[" << none.pxp[i] * scale << "," << none.pyp[i] * scale << "," << none.pzp[i] * scale << "]";
        if (i < none.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    // bothNeutron (绿色)
    html << "const bothNeutron = [\n";
    for (size_t i = 0; i < both.size(); ++i) {
        html << "[" << both.pxn[i] * scale << "," << both.pyn[i] * scale << "," << both.pzn[i] * scale << "]";
        if (i < both.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    // nebulaOnlyNeutron (蓝色 - NEBULA only neutron)
    html << "const nebulaOnlyNeutron = [\n";
    for (size_t i = 0; i < nebulaOnly.size(); ++i) {
        html << "[" << nebulaOnly.pxn[i] * scale << "," << nebulaOnly.pyn[i] * scale << "," << nebulaOnly.pzn[i] * scale << "]";
        if (i < nebulaOnly.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    // pdcOnlyNeutron (红色 - PDC only 的 neutron, 即 neutron 未被 NEBULA 接受)
    html << "const pdcOnlyNeutron = [\n";
    for (size_t i = 0; i < pdcOnly.size(); ++i) {
        html << "[" << pdcOnly.pxn[i] * scale << "," << pdcOnly.pyn[i] * scale << "," << pdcOnly.pzn[i] * scale << "]";
        if (i < pdcOnly.size() - 1) html << ",";
        html << "\n";
    }
    html << "];\n";
    
    // noneNeutron (灰色 - 都不接受的 neutron)
    html << "const noneNeutron = [\n";
    for (size_t i = 0; i < none.size(); ++i) {
        html << "[" << none.pxn[i] * scale << "," << none.pyn[i] * scale << "," << none.pzn[i] * scale << "]";
        if (i < none.size() - 1) html << ",";
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

// Proton view: 绿色=both, 红色=pdcOnly, 蓝色=nebulaOnly, 灰色=none
scenes.proton = createScene('proton-container');
pointGroups.proton = {
    both: createPoints(bothProton, 0x00ff88),
    pdcOnly: createPoints(pdcOnlyProton, 0xff4444),
    nebulaOnly: createPoints(nebulaOnlyProton, 0x4488ff),
    none: createPoints(noneProton, 0x888888, 0.4)
};
scenes.proton.scene.add(pointGroups.proton.both);
scenes.proton.scene.add(pointGroups.proton.pdcOnly);
scenes.proton.scene.add(pointGroups.proton.nebulaOnly);
scenes.proton.scene.add(pointGroups.proton.none);

// Neutron view: 绿色=both, 蓝色=nebulaOnly, 红色=pdcOnly, 灰色=none
scenes.neutron = createScene('neutron-container');
pointGroups.neutron = {
    both: createPoints(bothNeutron, 0x00ff88),
    nebulaOnly: createPoints(nebulaOnlyNeutron, 0x4488ff),
    pdcOnly: createPoints(pdcOnlyNeutron, 0xff4444),
    none: createPoints(noneNeutron, 0x888888, 0.4)
};
scenes.neutron.scene.add(pointGroups.neutron.both);
scenes.neutron.scene.add(pointGroups.neutron.nebulaOnly);
scenes.neutron.scene.add(pointGroups.neutron.pdcOnly);
scenes.neutron.scene.add(pointGroups.neutron.none);

// Combined view: proton 和 neutron 一起显示
scenes.combined = createScene('combined-container');
pointGroups.combined = {
    bothP: createPoints(bothProton, 0x00ff88),
    bothN: createPoints(bothNeutron, 0x00ff88, 0.5),
    pdcOnlyP: createPoints(pdcOnlyProton, 0xff4444),
    nebulaOnlyN: createPoints(nebulaOnlyNeutron, 0x4488ff)
};
scenes.combined.scene.add(pointGroups.combined.bothP);
scenes.combined.scene.add(pointGroups.combined.bothN);
scenes.combined.scene.add(pointGroups.combined.pdcOnlyP);
scenes.combined.scene.add(pointGroups.combined.nebulaOnlyN);

// Diff view: pp - pn (只用 both accepted 的事件)
const bothDiff = bothProton.map((p, i) => [
    p[0] - bothNeutron[i][0],
    p[1] - bothNeutron[i][1],
    p[2] - bothNeutron[i][2]
]);

scenes.diff = createScene('diff-container');
pointGroups.diff = {
    both: createPoints(bothDiff, 0x00ff88)
};
scenes.diff.scene.add(pointGroups.diff.both);

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

document.getElementById('showBoth').addEventListener('change', (e) => {
    const visible = e.target.checked;
    pointGroups.proton.both.visible = visible;
    pointGroups.neutron.both.visible = visible;
    pointGroups.combined.bothP.visible = visible;
    pointGroups.combined.bothN.visible = visible;
    pointGroups.diff.both.visible = visible;
});

document.getElementById('showPDCOnly').addEventListener('change', (e) => {
    const visible = e.target.checked;
    pointGroups.proton.pdcOnly.visible = visible;
    pointGroups.neutron.pdcOnly.visible = visible;
    pointGroups.combined.pdcOnlyP.visible = visible;
});

document.getElementById('showNEBULAOnly').addEventListener('change', (e) => {
    const visible = e.target.checked;
    pointGroups.proton.nebulaOnly.visible = visible;
    pointGroups.neutron.nebulaOnly.visible = visible;
    pointGroups.combined.nebulaOnlyN.visible = visible;
});

document.getElementById('showRejected').addEventListener('change', (e) => {
    const visible = e.target.checked;
    pointGroups.proton.none.visible = visible;
    pointGroups.neutron.none.visible = visible;
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
