#include "DetectorAcceptanceCalculator.hh"
#include "SMLogger.hh"
#include "TMath.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TRandom3.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

// 粒子常数
const double DetectorAcceptanceCalculator::kProtonMass = 938.272;    // MeV/c²
const double DetectorAcceptanceCalculator::kNeutronMass = 939.565;   // MeV/c²
const double DetectorAcceptanceCalculator::kProtonCharge = 1.0;      // e
const double DetectorAcceptanceCalculator::kNeutronCharge = 0.0;     // e

DetectorAcceptanceCalculator::DetectorAcceptanceCalculator()
    : fMagField(nullptr), fTrajectory(nullptr)
{
    // 设置默认NEBULA配置 (固定位置)
    // 基于NEBULA_Detectors_Dayone.csv和TNEBULASimParameter.cc
    // 单个Neut: 120×1800×120 mm (实际是12×180×12 cm)
    // 阵列覆盖: X约3600mm, Y约1800mm (单层高度), Z约600mm (多层深度)
    fNEBULAConfig.position.SetXYZ(0, 0, 5000);
    fNEBULAConfig.width = 3600;   // X方向宽度
    fNEBULAConfig.height = 1800;  // Y方向高度
    fNEBULAConfig.depth = 600;    // Z方向深度
}

DetectorAcceptanceCalculator::~DetectorAcceptanceCalculator()
{
    if (fTrajectory) {
        delete fTrajectory;
        fTrajectory = nullptr;
    }
    ClearData();
}

void DetectorAcceptanceCalculator::SetMagneticField(MagneticField* magField)
{
    fMagField = magField;
    
    if (fTrajectory) {
        delete fTrajectory;
    }
    fTrajectory = new ParticleTrajectory(fMagField);
    fTrajectory->SetStepSize(5.0);       // 5mm步长
    fTrajectory->SetMaxDistance(10000.0); // 10m
    fTrajectory->SetMaxTime(500.0);       // 500ns
}

bool DetectorAcceptanceCalculator::LoadQMDData(const std::string& dataFile,
                                               const std::string& polType)
{
    // QMD数据文件格式 (dbreak.dat 或 dbreakbXX.dat):
    // 第1行: 信息行
    // 第2行: 表头行
    // 数据行: eventNo pxp pyp pzp pxn pyn pzn [b rpphi_deg]
    //
    // 数据处理流程 (参考 zpol_ypol_show_approx_P.ipynb):
    // 1. 读取原始动量 (pxp_orig, pyp_orig, pzp_orig) 和 (pxn_orig, pyn_orig, pzn_orig)
    // 2. 计算两粒子动量和的方位角 phi
    // 3. 绕Z轴旋转 -phi 角度，使反应平面对准X轴
    // 4. 应用筛选条件
    // 5. 再添加一个随机旋转角度模拟真实入射
    
    SM_INFO("Loading QMD data from: {}", dataFile);
    
    std::ifstream infile(dataFile);
    if (!infile.is_open()) {
        SM_ERROR("Cannot open file {}", dataFile);
        return false;
    }
    
    // 跳过头两行
    std::string line;
    std::getline(infile, line); // 信息行
    std::getline(infile, line); // 表头行
    
    int loadedEvents = 0;
    int passedEvents = 0;
    
    // 随机数生成器用于Z轴随机旋转
    TRandom3 rnd(0);
    
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        
        int eventNo;
        double pxp_orig, pyp_orig, pzp_orig;
        double pxn_orig, pyn_orig, pzn_orig;
        
        if (!(iss >> eventNo >> pxp_orig >> pyp_orig >> pzp_orig 
                            >> pxn_orig >> pyn_orig >> pzn_orig)) {
            continue;
        }
        
        loadedEvents++;

        // 计算总动量的方位角
        double sumPx = pxp_orig + pxn_orig;
        double sumPy = pyp_orig + pyn_orig;
        double sumPz = pzp_orig + pzn_orig;
        double phi_for_rotation = TMath::ATan2(sumPy, sumPx);

        // 决定使用哪组筛选条件: "y" 或 "z"。
        std::string pol = polType;
        if (pol.empty()) {
            // 如果未传入 polType，尝试通过文件路径自动检测
            if (dataFile.find("y_pol") != std::string::npos) pol = "y";
            else if (dataFile.find("z_pol") != std::string::npos) pol = "z";
            else pol = "y"; // 默认为 Y 极化的旧行为
        }

        // 默认旋转矩阵（针对两种情况都需要）
        double cos_phi = TMath::Cos(-phi_for_rotation);
        double sin_phi = TMath::Sin(-phi_for_rotation);

        // 旋转到反应平面（Z 轴不变）
        double pxp = cos_phi * pxp_orig - sin_phi * pyp_orig;
        double pyp = sin_phi * pxp_orig + cos_phi * pyp_orig;
        double pzp = pzp_orig;

        double pxn = cos_phi * pxn_orig - sin_phi * pyn_orig;
        double pyn = sin_phi * pxn_orig + cos_phi * pyn_orig;
        double pzn = pzn_orig;

        // 根据不同的极化类型应用不同的筛选条件（按 notebook 中的实现）
        if (pol == "y") {
            // Y 极化筛选：在旋转前对 y 分量和横向总动量做初步筛选
            bool cond_y1 = TMath::Abs(pyp_orig - pyn_orig) < 150;
            bool cond_y2 = (sumPx*sumPx + sumPy*sumPy) > 2500;
            if (!cond_y1 || !cond_y2) continue;

            // 旋转后再应用额外条件
            bool cond_y3 = (pxp + pxn) < 200;
            bool cond_y4 = (TMath::Pi() - TMath::Abs(phi_for_rotation)) < 0.2;
            if (!cond_y3 || !cond_y4) continue;
        } else if (pol == "z") {
            // Z 极化筛选（按 notebook 中的综合条件）
            // 使用旋转后的分量检查
            double sum_pz = pzp + pzn;
            double diff_pz = TMath::Abs(pzp - pzn);
            double transverse_sum = TMath::Sqrt((pxn + pxp)*(pxn + pxp) + (pyn + pyp)*(pyn + pyp));

            if (!(sum_pz > 1150.0
                  && (TMath::Pi() - TMath::Abs(phi_for_rotation)) < 0.5
                  && diff_pz < 150.0
                  && (pxp + pxn) < 200.0
                  && transverse_sum > 50.0)) {
                continue;
            }
        } else {
            // 未知类型：保持老的 Y 风格行为以兼容
            bool cond1_part1 = TMath::Abs(pyp_orig - pyn_orig) < 150;
            bool cond1_part2 = (sumPx*sumPx + sumPy*sumPy) > 2500;
            if (!cond1_part1 || !cond1_part2) continue;

            bool cond2_part1 = (pxp + pxn) < 200;
            bool cond2_part2 = (TMath::Pi() - TMath::Abs(phi_for_rotation)) < 0.2;
            if (!cond2_part1 || !cond2_part2) continue;
        }
        
        // 添加绕Z轴的随机旋转 (模拟真实入射的随机方位角)
        double random_phi = rnd.Uniform(0, 2.0 * TMath::Pi());
        double cos_rand = TMath::Cos(random_phi);
        double sin_rand = TMath::Sin(random_phi);
        
        double pxp_final = cos_rand * pxp - sin_rand * pyp;
        double pyp_final = sin_rand * pxp + cos_rand * pyp;
        
        double pxn_final = cos_rand * pxn - sin_rand * pyn;
        double pyn_final = sin_rand * pxn + cos_rand * pyn;
        
        passedEvents++;
        
        // 存储质子数据
        ParticleInfo proton;
        proton.eventId = eventNo;
        proton.pdgCode = 2212;
        proton.charge = kProtonCharge;
        proton.mass = kProtonMass;
        proton.vertex.SetXYZ(0, 0, 0); // target位置在后面设置
        
        TVector3 pMom(pxp_final, pyp_final, pzp);
        double Ep = TMath::Sqrt(pMom.Mag2() + kProtonMass * kProtonMass);
        proton.momentum.SetPxPyPzE(pxp_final, pyp_final, pzp, Ep);
        fProtons.push_back(proton);
        
        // 存储中子数据
        ParticleInfo neutron;
        neutron.eventId = eventNo;
        neutron.pdgCode = 2112;
        neutron.charge = kNeutronCharge;
        neutron.mass = kNeutronMass;
        neutron.vertex.SetXYZ(0, 0, 0);
        
        TVector3 nMom(pxn_final, pyn_final, pzn);
        double En = TMath::Sqrt(nMom.Mag2() + kNeutronMass * kNeutronMass);
        neutron.momentum.SetPxPyPzE(pxn_final, pyn_final, pzn, En);
        fNeutrons.push_back(neutron);
    }
    
    infile.close();
    
    SM_INFO("  Total events read: {}", loadedEvents);
    SM_INFO("  Events passed cuts: {}", passedEvents);
    SM_INFO("  Loaded {} protons and {} neutrons", fProtons.size(), fNeutrons.size());
    
    return (passedEvents > 0);
}

bool DetectorAcceptanceCalculator::LoadQMDDataFromDirectory(const std::string& directory)
{
    // QMD数据目录结构 (参考zpol_ypol_show_approx_P.ipynb):
    //
    // Y极化数据:
    //   {dir}/qmdrawdata/y_pol/phi_random/d+{target}E190g{gamma}{pol}/dbreak.dat
    //   其中 target=Pb208, gamma=050/060/070/080, pol=ynp/ypn
    //
    // Z极化数据:
    //   {dir}/qmdrawdata/z_pol/b_discrete/d+{target}E190g{gamma}{pol}/dbreakb{b:02d}.dat
    //   其中 b=01-10
    
    SM_INFO("Loading QMD data from directory: {}", directory);
    
    namespace fs = std::filesystem;
    
    if (!fs::exists(directory)) {
        SM_ERROR("Directory does not exist: {}", directory);
        return false;
    }
    
    ClearData(); // 清空之前的数据
    
    // 配置参数
    std::vector<std::string> targets = {"Pb208"};
    std::vector<std::string> gammas = {"050", "060", "070", "080"};
    std::vector<std::string> y_pols = {"ynp", "ypn"};
    std::vector<std::string> z_pols = {"znp", "zpn"};
    int bmin = 1, bmax = 10;
    
    int totalFiles = 0;
    int loadedFiles = 0;
    
    // 加载Y极化数据
    SM_INFO("--- Loading Y-polarization data ---");
    for (const auto& target : targets) {
        for (const auto& gamma : gammas) {
            for (const auto& pol : y_pols) {
                std::string subdir = "qmdrawdata/y_pol/phi_random/d+" + target 
                                    + "E190g" + gamma + pol;
                fs::path filepath = fs::path(directory) / subdir / "dbreak.dat";
                
                totalFiles++;
                if (fs::exists(filepath)) {
                    if (LoadQMDData(filepath.string(), "y")) {
                        loadedFiles++;
                    }
                } else {
                    SM_DEBUG("  Not found: {}", filepath.string());
                }
            }
        }
    }
    
    // 加载Z极化数据
    SM_INFO("--- Loading Z-polarization data ---");
    for (const auto& target : targets) {
        for (const auto& gamma : gammas) {
            for (const auto& pol : z_pols) {
                for (int b = bmin; b <= bmax; b++) {
                    std::string subdir = "qmdrawdata/z_pol/b_discrete/d+" + target 
                                        + "E190g" + gamma + pol;
                    char bstr[16];
                    snprintf(bstr, sizeof(bstr), "dbreakb%02d.dat", b);
                    fs::path filepath = fs::path(directory) / subdir / bstr;
                    
                    totalFiles++;
                    if (fs::exists(filepath)) {
                        if (LoadQMDData(filepath.string(), "z")) {
                            loadedFiles++;
                        }
                    }
                    // 不打印每个缺失的b文件，太多了
                }
            }
        }
    }
    
    SM_INFO("=== QMD Data Loading Summary ===");
    SM_INFO("  Total data files found: {}/{}", loadedFiles, totalFiles);
    SM_INFO("  Total protons: {}", fProtons.size());
    SM_INFO("  Total neutrons: {}", fNeutrons.size());
    
    return (fProtons.size() > 0 && fNeutrons.size() > 0);
}

DetectorAcceptanceCalculator::PDCConfiguration 
DetectorAcceptanceCalculator::CalculateOptimalPDCPosition(const TVector3& targetPos,
                                                          double targetRotationAngle,
                                                          double pxRange)
{
    SM_INFO("=== Calculating Optimal PDC Position ===");
    SM_INFO("  Target position: ({:.1f}, {:.1f}, {:.1f}) mm", 
            targetPos.X(), targetPos.Y(), targetPos.Z());
    SM_INFO("  Target rotation angle: {:.2f}°", targetRotationAngle);
    SM_INFO("  Px range for optimization: +/-{:.1f} MeV/c", pxRange);
    
    PDCConfiguration optimalConfig;
    
    if (!fMagField || !fTrajectory) {
        SM_ERROR("Magnetic field not set!");
        return optimalConfig;
    }
    
    // [EN] PDC sensitive area half-width (from PDCConstruction.cc: 840 mm)
    // [CN] PDC敏感区域半宽度 (从PDCConstruction.cc: 840 mm)
    const double pdcHalfWidth = 840.0;  // mm
    
    // ============================================================
    // 1. [EN] Prepare momentum transformation: target local -> lab frame
    //    [CN] 准备动量转换：target局部坐标系 -> 实验室坐标系
    // ============================================================
    double theta_rad = targetRotationAngle * TMath::DegToRad();
    double cos_theta = TMath::Cos(theta_rad);
    double sin_theta = TMath::Sin(theta_rad);
    
    // [EN] Rotation matrix R_y(θ): transform local momentum to lab frame
    // [CN] 旋转矩阵 R_y(θ): 将局部坐标系动量转换到实验室坐标系
    // px_lab = px_local*cos(θ) + pz_local*sin(θ)
    // py_lab = py_local
    // pz_lab = -px_local*sin(θ) + pz_local*cos(θ)
    
    // ============================================================
    // 2. [EN] Create reference proton (Px=0, Pz=627 MeV/c in local frame)
    //    [CN] 创建参考质子 (Px=0, Pz=627 MeV/c 在局部坐标系)
    // ============================================================
    double pz_local = 627.0;  // MeV/c
    double py_local = 0.0;
    
    double px_ref_lab = pz_local * sin_theta;  // px_local=0
    double py_ref_lab = py_local;
    double pz_ref_lab = pz_local * cos_theta;
    
    TVector3 momentum_ref(px_ref_lab, py_ref_lab, pz_ref_lab);
    double E_ref = TMath::Sqrt(momentum_ref.Mag2() + kProtonMass * kProtonMass);
    TLorentzVector p4_ref(momentum_ref, E_ref);
    
    SM_INFO("  Reference proton (Px=0): ({:.1f}, {:.1f}, {:.1f}) MeV/c",
            px_ref_lab, py_ref_lab, pz_ref_lab);
    
    // [EN] Track reference proton trajectory / [CN] 追踪参考质子轨迹
    auto trajectory_ref = fTrajectory->CalculateTrajectory(targetPos, p4_ref,
                                                            kProtonCharge, kProtonMass);
    
    if (trajectory_ref.size() < 10) {
        SM_ERROR("Failed to calculate reference proton trajectory!");
        return optimalConfig;
    }
    
    // ============================================================
    // 3. [EN] Create edge proton (Px=+pxRange, Pz=600 MeV/c in local frame)
    //    [CN] 创建边缘质子 (Px=+pxRange, Pz=600 MeV/c 在局部坐标系)
    // ============================================================
    double px_edge_local = pxRange;  // [EN] Use pxRange parameter / [CN] 使用pxRange参数
    
    double px_edge_lab = px_edge_local * cos_theta + pz_local * sin_theta;
    double pz_edge_lab = -px_edge_local * sin_theta + pz_local * cos_theta;
    
    TVector3 momentum_edge(px_edge_lab, py_local, pz_edge_lab);
    double E_edge = TMath::Sqrt(momentum_edge.Mag2() + kProtonMass * kProtonMass);
    TLorentzVector p4_edge(momentum_edge, E_edge);
    
    SM_INFO("  Edge proton (Px=+{:.0f}): ({:.1f}, {:.1f}, {:.1f}) MeV/c",
            pxRange, px_edge_lab, py_local, pz_edge_lab);
    
    // [EN] Track edge proton trajectory / [CN] 追踪边缘质子轨迹
    auto trajectory_edge = fTrajectory->CalculateTrajectory(targetPos, p4_edge,
                                                             kProtonCharge, kProtonMass);
    
    if (trajectory_edge.size() < 10) {
        SM_ERROR("Failed to calculate edge proton trajectory!");
        return optimalConfig;
    }
    
    // ============================================================
    // 4. [EN] Scan along trajectories, find position where edge-center distance = PDC half-width
    //    [CN] 沿两条轨迹扫描，找到边缘质子与参考质子的垂直距离 = PDC半宽度的位置
    // ============================================================
    SM_INFO("  Searching for optimal PDC position where edge-center distance = {:.0f} mm", 
            pdcHalfWidth);
    
    size_t optimalIndex = 0;
    double minDiffFromTarget = 1e9;
    double optimalSeparation = 0;
    
    // [EN] Only search outside the magnetic field region (Z > 500 mm)
    // [CN] 只在磁场外区域搜索 (Z > 500 mm)
    size_t startIdx = 0;
    for (size_t i = 0; i < trajectory_ref.size(); i++) {
        if (trajectory_ref[i].position.Z() > 500) {
            startIdx = i;
            break;
        }
    }
    
    for (size_t i = startIdx; i < trajectory_ref.size(); i++) {
        TVector3 refPos = trajectory_ref[i].position;
        TVector3 refMom = trajectory_ref[i].momentum;
        
        // 当前位置的PDC法向量（沿动量方向）
        TVector3 pdcNormal = refMom.Unit();
        
        // PDC局部X轴：垂直于法向量且在水平面内
        TVector3 pdcLocalX(-pdcNormal.Z(), 0, pdcNormal.X());
        pdcLocalX = pdcLocalX.Unit();
        
        // 在边缘轨迹上找与当前PDC平面相交的点
        // 寻找边缘轨迹上到该平面距离最小的点
        double minDistToPlane = 1e9;
        TVector3 edgeHitPos;
        
        for (const auto& pt : trajectory_edge) {
            TVector3 toPoint = pt.position - refPos;
            double distToPlane = TMath::Abs(toPoint.Dot(pdcNormal));
            
            if (distToPlane < minDistToPlane) {
                minDistToPlane = distToPlane;
                edgeHitPos = pt.position;
            }
        }
        
        // 计算边缘质子相对于参考质子在PDC局部X方向的距离
        TVector3 separation = edgeHitPos - refPos;
        double xSeparation = TMath::Abs(separation.Dot(pdcLocalX));
        
        // 找到距离最接近PDC半宽度的位置
        double diffFromTarget = TMath::Abs(xSeparation - pdcHalfWidth);
        
        if (diffFromTarget < minDiffFromTarget) {
            minDiffFromTarget = diffFromTarget;
            optimalIndex = i;
            optimalSeparation = xSeparation;
        }
    }
    
    // ============================================================
    // 5. 提取最佳PDC位置和配置
    // ============================================================
    TVector3 pdcPos = trajectory_ref[optimalIndex].position;
    TVector3 pdcMomentum = trajectory_ref[optimalIndex].momentum;
    TVector3 pdcNormal = pdcMomentum.Unit();
    
    // PDC旋转角度
    double pdcRotationAngle = TMath::ATan2(pdcNormal.X(), pdcNormal.Z()) * TMath::RadToDeg();
    
    SM_INFO("  Optimal PDC position found:");
    SM_INFO("    Position: ({:.1f}, {:.1f}, {:.1f}) mm",
            pdcPos.X(), pdcPos.Y(), pdcPos.Z());
    SM_INFO("    Edge-Center separation: {:.1f} mm (target: {:.0f} mm)",
            optimalSeparation, pdcHalfWidth);
    SM_INFO("    PDC rotation angle: {:.2f}°", pdcRotationAngle);
    SM_INFO("    Momentum at PDC: ({:.1f}, {:.1f}, {:.1f}) MeV/c",
            pdcMomentum.X(), pdcMomentum.Y(), pdcMomentum.Z());
    
    // ============================================================
    // 6. [EN] Fill PDC configuration / [CN] 填充PDC配置
    // ============================================================
    optimalConfig.position = pdcPos;
    optimalConfig.normal = pdcNormal;
    optimalConfig.rotationAngle = pdcRotationAngle;
    optimalConfig.width = 2 * pdcHalfWidth;  // 1680 mm
    optimalConfig.pxMin = -pxRange;  // [EN] Use pxRange parameter / [CN] 使用pxRange参数
    optimalConfig.pxMax = pxRange;   // [EN] Use pxRange parameter / [CN] 使用pxRange参数
    optimalConfig.isOptimal = true;
    optimalConfig.isFixed = false;   // [EN] This is an optimized config, not fixed / [CN] 这是优化后的配置，非固定
    
    SM_INFO("Optimal PDC configuration:");
    SM_INFO("  Position: ({:.1f}, {:.1f}, {:.1f}) mm",
            optimalConfig.position.X(), optimalConfig.position.Y(), optimalConfig.position.Z());
    SM_INFO("  Rotation angle: {:.2f}°", optimalConfig.rotationAngle);
    SM_INFO("  Size: {:.0f} × {:.0f} × {:.0f} mm³",
            optimalConfig.width, optimalConfig.height, optimalConfig.depth);
    SM_INFO("  Px range: [{:.0f}, {:.0f}] MeV/c", optimalConfig.pxMin, optimalConfig.pxMax);

    // 记录 PDC 法向量和四个顶点的位置（便于在日志中追踪 PDC 几何）
    TVector3 localX(-pdcNormal.Z(), 0, pdcNormal.X());
    if (localX.Mag() < 1e-6) localX.SetXYZ(1,0,0);
    localX = localX.Unit();
    TVector3 localY = pdcNormal.Cross(localX).Unit();

    double halfW = optimalConfig.width / 2.0;
    double halfH = optimalConfig.height / 2.0;

    TVector3 corner1 = optimalConfig.position + localX * halfW + localY * halfH; // +X +Y
    TVector3 corner2 = optimalConfig.position - localX * halfW + localY * halfH; // -X +Y
    TVector3 corner3 = optimalConfig.position - localX * halfW - localY * halfH; // -X -Y
    TVector3 corner4 = optimalConfig.position + localX * halfW - localY * halfH; // +X -Y

    SM_INFO("  PDC normal: ({:.4f}, {:.4f}, {:.4f})", pdcNormal.X(), pdcNormal.Y(), pdcNormal.Z());
    SM_INFO("  PDC corners (mm):");
    SM_INFO("    corner1 (+X,+Y): ({:.1f}, {:.1f}, {:.1f})", corner1.X(), corner1.Y(), corner1.Z());
    SM_INFO("    corner2 (-X,+Y): ({:.1f}, {:.1f}, {:.1f})", corner2.X(), corner2.Y(), corner2.Z());
    SM_INFO("    corner3 (-X,-Y): ({:.1f}, {:.1f}, {:.1f})", corner3.X(), corner3.Y(), corner3.Z());
    SM_INFO("    corner4 (+X,-Y): ({:.1f}, {:.1f}, {:.1f})", corner4.X(), corner4.Y(), corner4.Z());
    
    return optimalConfig;
}

// 检查粒子是否击中PDC
//@brief 检查粒子是否击中PDC探测器
//@param particle 待检查的粒子信息, 包括初始位置、动量、质量、电荷等. 这个粒子信息应在实验室坐标系中. 而非靶点坐标系. 
//@param hitPosition 如果击中，返回击中位置
//@return 如果粒子击中PDC，返回true；否则返回false。
bool DetectorAcceptanceCalculator::CheckPDCHit(const ParticleInfo& particle, 
                                                TVector3& hitPosition)
{
    // 中性粒子不受磁场影响，直线传播
    if (TMath::Abs(particle.charge) < 0.5) {
        // 直线与PDC平面相交
        TVector3 direction = particle.momentum.Vect().Unit();
        TVector3 toPlane = fPDCConfig.position - particle.vertex;
        
        double denom = direction.Dot(fPDCConfig.normal);
        if (TMath::Abs(denom) < 1e-6) return false; // 平行于平面
        
        double t = toPlane.Dot(fPDCConfig.normal) / denom;
        if (t < 0) return false; // 反方向
        
        hitPosition = particle.vertex + direction * t;
        
        // 转换到PDC局部坐标系检查是否在范围内
        TVector3 localPos = hitPosition - fPDCConfig.position;
        
        // PDC局部坐标系：
        // X轴：垂直于法向量且在水平面内
        // Y轴：垂直于X和法向量（接近竖直）
        // Z轴：法向量方向
        TVector3 pdcLocalX(-fPDCConfig.normal.Z(), 0, fPDCConfig.normal.X());
        if (pdcLocalX.Mag() < 1e-6) {
            pdcLocalX.SetXYZ(1, 0, 0);  // 法向量接近垂直时的备用
        }
        pdcLocalX = pdcLocalX.Unit();
        TVector3 pdcLocalY = fPDCConfig.normal.Cross(pdcLocalX).Unit();
        
        double localX = localPos.Dot(pdcLocalX);
        double localY = localPos.Dot(pdcLocalY);
        
        if (TMath::Abs(localX) < fPDCConfig.width / 2.0 &&
            TMath::Abs(localY) < fPDCConfig.height / 2.0) {
            return true;
        }
        return false;
    }
    
    // 带电粒子：需要追踪轨迹
    if (!fTrajectory) return false;
    
    auto trajectory = fTrajectory->CalculateTrajectory(particle.vertex, particle.momentum,
                                                        particle.charge, particle.mass);
    
    // PDC局部坐标系
    TVector3 pdcLocalX(-fPDCConfig.normal.Z(), 0, fPDCConfig.normal.X());
    if (pdcLocalX.Mag() < 1e-6) {
        pdcLocalX.SetXYZ(1, 0, 0);
    }
    pdcLocalX = pdcLocalX.Unit();
    TVector3 pdcLocalY = fPDCConfig.normal.Cross(pdcLocalX).Unit();
    
    // 检查轨迹是否穿过PDC平面
    for (size_t i = 0; i < trajectory.size(); i++) {
        const auto& point = trajectory[i];
        TVector3 toPoint = point.position - fPDCConfig.position;
        double signedDist = toPoint.Dot(fPDCConfig.normal);
        
        // 检测穿越平面（符号变化）
        if (i > 0) {
            TVector3 toPrev = trajectory[i-1].position - fPDCConfig.position;
            double prevDist = toPrev.Dot(fPDCConfig.normal);
            
            if (signedDist * prevDist <= 0) {  // 穿过平面
                // 线性插值找到精确交点
                double t = TMath::Abs(prevDist) / (TMath::Abs(prevDist) + TMath::Abs(signedDist));
                hitPosition = trajectory[i-1].position * (1-t) + point.position * t;
                
                // 检查横向范围
                TVector3 localPos = hitPosition - fPDCConfig.position;
                double localX = localPos.Dot(pdcLocalX);
                double localY = localPos.Dot(pdcLocalY);
                
                if (TMath::Abs(localX) < fPDCConfig.width / 2.0 &&
                    TMath::Abs(localY) < fPDCConfig.height / 2.0) {
                    
                    // 检查Px是否在接受范围内（在局部坐标系中）
                    // 插值动量
                    TVector3 interpMom = trajectory[i-1].momentum * (1-t) + point.momentum * t;
                    double px_local = interpMom.Dot(pdcLocalX);
                    
                    if (px_local >= fPDCConfig.pxMin && px_local <= fPDCConfig.pxMax) {
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

bool DetectorAcceptanceCalculator::CheckNEBULAHit(const ParticleInfo& particle,
                                                   TVector3& hitPosition)
{
    // NEBULA检测中子和质子
    // 简化模型：矩形体探测器
    
    TVector3 direction = particle.momentum.Vect().Unit();
    
    // 与NEBULA前表面(Z平面)相交
    double z_front = fNEBULAConfig.position.Z() - fNEBULAConfig.depth / 2.0;
    double t = (z_front - particle.vertex.Z()) / direction.Z();
    
    if (t < 0) return false; // 反方向
    
    hitPosition = particle.vertex + direction * t;
    
    // 检查X, Y范围
    TVector3 localPos = hitPosition - fNEBULAConfig.position;
    if (TMath::Abs(localPos.X()) < fNEBULAConfig.width / 2.0 &&
        TMath::Abs(localPos.Y()) < fNEBULAConfig.height / 2.0) {
        return true;
    }
    
    return false;
}

DetectorAcceptanceCalculator::AcceptanceResult 
DetectorAcceptanceCalculator::CalculateAcceptance()
{
    AcceptanceResult result;
    
    SM_INFO("=== Calculating Detector Acceptance ===");
    
    // 合并质子和中子数据
    std::vector<ParticleInfo> allParticles;
    allParticles.insert(allParticles.end(), fProtons.begin(), fProtons.end());
    allParticles.insert(allParticles.end(), fNeutrons.begin(), fNeutrons.end());
    
    result.totalEvents = allParticles.size();
    
    if (result.totalEvents == 0) {
        SM_WARN("No particles to analyze!");
        return result;
    }
    
    // 逐个粒子检查
    for (const auto& particle : allParticles) {
        TVector3 pdcHitPos, nebulaHitPos;
        bool hitPDC = CheckPDCHit(particle, pdcHitPos);
        bool hitNEBULA = CheckNEBULAHit(particle, nebulaHitPos);
        
        if (hitPDC) {
            result.pdcHits++;
            double theta = particle.momentum.Vect().Theta() * TMath::RadToDeg();
            result.pdcThetas.push_back(theta);
        }
        
        if (hitNEBULA) {
            result.nebulaHits++;
            double theta = particle.momentum.Vect().Theta() * TMath::RadToDeg();
            result.nebulaThetas.push_back(theta);
        }
        
        if (hitPDC && hitNEBULA) {
            result.bothHits++;
        }
    }
    
    // 计算接受度
    result.pdcAcceptance = 100.0 * result.pdcHits / result.totalEvents;
    result.nebulaAcceptance = 100.0 * result.nebulaHits / result.totalEvents;
    result.coincidenceAcceptance = 100.0 * result.bothHits / result.totalEvents;
    
    SM_INFO("Analysis complete:");
    SM_INFO("  Total particles: {}", result.totalEvents);
    SM_INFO("  PDC hits: {} ({:.2f}%)", result.pdcHits, result.pdcAcceptance);
    SM_INFO("  NEBULA hits: {} ({:.2f}%)", result.nebulaHits, result.nebulaAcceptance);
    SM_INFO("  Coincidence: {} ({:.2f}%)", result.bothHits, result.coincidenceAcceptance);
    
    return result;
}

DetectorAcceptanceCalculator::AcceptanceResult 
DetectorAcceptanceCalculator::CalculateAcceptanceForTarget(const TVector3& targetPos,
                                                           double targetRotationAngle,
                                                           double pxRange)
{
    // [EN] Note: PDC configuration should be set before calling this function.
    // [CN] 注意：在调用此函数之前应设置PDC配置。
    // [EN] If PDC config not set, calculate optimal position as fallback.
    // [CN] 如果未设置PDC配置，则作为后备计算最佳位置。
    if (fPDCConfig.width <= 0) {
        SM_INFO("  PDC not configured, calculating optimal position...");
        fPDCConfig = CalculateOptimalPDCPosition(targetPos, targetRotationAngle, pxRange);
    }
    
    // [EN] Update all particle vertices and transform momenta to lab frame
    // [CN] 更新所有粒子的顶点位置为新的target位置，同时将动量从target局部坐标系转换到实验室坐标系
    double theta_rad = targetRotationAngle * TMath::DegToRad();
    double cos_theta = TMath::Cos(theta_rad);
    double sin_theta = TMath::Sin(theta_rad);
    
    for (auto& p : fProtons) {
        p.vertex = targetPos;
        
        // 旋转动量：从target局部坐标系到实验室坐标系
        // QMD数据中的动量是在target局部坐标系中定义的
        TVector3 pLocal = p.momentum.Vect();
        double px_lab = pLocal.X() * cos_theta + pLocal.Z() * sin_theta;
        double py_lab = pLocal.Y();
        double pz_lab = -pLocal.X() * sin_theta + pLocal.Z() * cos_theta;
        
        double E = TMath::Sqrt(px_lab*px_lab + py_lab*py_lab + pz_lab*pz_lab + 
                               p.mass * p.mass);
        p.momentum.SetPxPyPzE(px_lab, py_lab, pz_lab, E);
    }
    
    for (auto& n : fNeutrons) {
        n.vertex = targetPos;
        
        // 同样旋转中子动量
        TVector3 pLocal = n.momentum.Vect();
        double px_lab = pLocal.X() * cos_theta + pLocal.Z() * sin_theta;
        double py_lab = pLocal.Y();
        double pz_lab = -pLocal.X() * sin_theta + pLocal.Z() * cos_theta;
        
        double E = TMath::Sqrt(px_lab*px_lab + py_lab*py_lab + pz_lab*pz_lab + 
                               n.mass * n.mass);
        n.momentum.SetPxPyPzE(px_lab, py_lab, pz_lab, E);
    }
    
    // 计算接受度
    return CalculateAcceptance();
}

void DetectorAcceptanceCalculator::PrintAcceptanceReport(
    const AcceptanceResult& result,
    double deflectionAngle,
    double fieldStrength) const
{
    SM_INFO("");
    SM_INFO("╔════════════════════════════════════════════════════════╗");
    SM_INFO("║        Detector Acceptance Analysis Report            ║");
    SM_INFO("╠════════════════════════════════════════════════════════╣");
    SM_INFO("║ Magnetic Field:        {:8.2f} T                     ║", fieldStrength);
    SM_INFO("║ Beam Deflection Angle: {:8.2f} deg                  ║", deflectionAngle);
    SM_INFO("╠════════════════════════════════════════════════════════╣");
    SM_INFO("║ Total Events:          {:8d}                        ║", result.totalEvents);
    SM_INFO("║                                                        ║");
    SM_INFO("║ PDC Acceptance:        {:6.2f} %  ({:6d} hits)    ║", 
            result.pdcAcceptance, result.pdcHits);
    SM_INFO("║ NEBULA Acceptance:     {:6.2f} %  ({:6d} hits)    ║", 
            result.nebulaAcceptance, result.nebulaHits);
    SM_INFO("║ Coincidence:           {:6.2f} %  ({:6d} hits)    ║", 
            result.coincidenceAcceptance, result.bothHits);
    SM_INFO("╚════════════════════════════════════════════════════════╝");
    SM_INFO("");
}

void DetectorAcceptanceCalculator::ClearData()
{
    fProtons.clear();
    fNeutrons.clear();
}

void DetectorAcceptanceCalculator::SaveResults(
    const std::string& filename,
    const std::map<double, AcceptanceResult>& results) const
{
    TFile* outFile = TFile::Open(filename.c_str(), "RECREATE");
    if (!outFile || outFile->IsZombie()) {
        SM_ERROR("Cannot create output file {}", filename);
        return;
    }
    
    // TODO: 保存结果到ROOT文件
    // 创建TTree, TH1D等
    
    outFile->Close();
    delete outFile;
    
    SM_INFO("Results saved to {}", filename);
}

DetectorAcceptanceCalculator::PDCConfiguration 
DetectorAcceptanceCalculator::CreateFixedPDCConfiguration(const TVector3& pdcPosition,
                                                           double pdcRotationAngle,
                                                           double pxMin,
                                                           double pxMax)
{
    // [EN] Create a fixed PDC configuration at given position
    // [CN] 创建固定位置的PDC配置
    
    SM_INFO("=== Creating Fixed PDC Configuration ===");
    SM_INFO("  Fixed PDC position: ({:.1f}, {:.1f}, {:.1f}) mm", 
            pdcPosition.X(), pdcPosition.Y(), pdcPosition.Z());
    SM_INFO("  Fixed PDC rotation angle: {:.2f}°", pdcRotationAngle);
    SM_INFO("  Px range: [{:.1f}, {:.1f}] MeV/c", pxMin, pxMax);
    
    PDCConfiguration config;
    config.position = pdcPosition;
    config.rotationAngle = pdcRotationAngle;
    
    // [EN] Calculate normal vector from rotation angle / [CN] 从旋转角度计算法向量
    double rotRad = pdcRotationAngle * TMath::DegToRad();
    config.normal.SetXYZ(TMath::Sin(rotRad), 0, TMath::Cos(rotRad));
    
    // [EN] Use default PDC dimensions / [CN] 使用默认PDC尺寸
    config.width = 2 * 840.0;   // 1680 mm
    config.height = 2 * 390.0;  // 780 mm
    config.depth = 2 * 190.0;   // 380 mm
    
    config.pxMin = pxMin;
    config.pxMax = pxMax;
    config.isOptimal = false;
    config.isFixed = true;      // [EN] Mark as fixed configuration / [CN] 标记为固定配置
    
    SM_INFO("  PDC normal: ({:.4f}, {:.4f}, {:.4f})", 
            config.normal.X(), config.normal.Y(), config.normal.Z());
    
    return config;
}
