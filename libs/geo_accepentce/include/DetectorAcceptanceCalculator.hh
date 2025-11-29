#ifndef DETECTOR_ACCEPTANCE_CALCULATOR_HH
#define DETECTOR_ACCEPTANCE_CALCULATOR_HH

#include "TVector3.h"
#include "TLorentzVector.h"
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include <vector>
#include <string>
#include <map>

/**
 * @class DetectorAcceptanceCalculator
 * @brief 计算PDC和NEBULA探测器的几何接受度
 * 
 * 根据反应数据(n和p的动量)，计算粒子是否会打到PDC或NEBULA探测器
 * PDC位置可调整以优化接受度，NEBULA位置固定
 */
class DetectorAcceptanceCalculator {
public:
    // 探测器配置
    struct PDCConfiguration {
        TVector3 position;       // PDC中心位置 [mm]
        TVector3 normal;         // PDC平面法向量（单位矢量）
        double rotationAngle;    // PDC绕Y轴旋转角度 [度] - 使PDC平面垂直于质子轨迹
        double width;            // PDC有效宽度 [mm] (X方向, 2×840mm)
        double height;           // PDC有效高度 [mm] (Y方向, 2×390mm)
        double depth;            // PDC总厚度 [mm] (Z方向, 2×190mm)
        double pxMin;            // 最小Px [MeV/c]
        double pxMax;            // 最大Px [MeV/c]
        bool isOptimal;          // 是否为最佳配置
        
        // PDC尺寸来自PDCConstruction.cc:
        // Enclosure: 1700×800×190 mm (半尺寸)
        // Active area: 840×390 mm (半尺寸，每层)
        PDCConfiguration() : position(0,0,0), normal(0,0,1), rotationAngle(0),
                            width(2*840), height(2*390), depth(2*190),
                            pxMin(-100), pxMax(100), isOptimal(false) {}
    };
    
    struct NEBULAConfiguration {
        TVector3 position;       // NEBULA中心位置 [mm] (固定)
        double width;            // 宽度 [mm] (X方向)
        double height;           // 高度 [mm] (Y方向)
        double depth;            // 深度 [mm] (Z方向)
        
        // 实际尺寸基于NEBULA配置:
        // 单个Neut探测器: 120×1800×120 mm (从TNEBULASimParameter.cc: 12×180×12 cm)
        // X范围: -1647.8 到 +1901.8 mm ≈ 3550 mm
        // Y范围: 2层 × 1800 mm (竖直方向)
        // Z深度: 多层结构 ≈ 600 mm
        NEBULAConfiguration() : position(0,0,5000), 
                               width(3600), height(1800), depth(600) {}
    };
    
    // 接受度结果
    struct AcceptanceResult {
        int totalEvents;         // 总事例数
        int pdcHits;            // 打到PDC的事例数
        int nebulaHits;         // 打到NEBULA的事例数
        int bothHits;           // 同时打到两者的事例数
        double pdcAcceptance;   // PDC几何接受度
        double nebulaAcceptance; // NEBULA几何接受度
        double coincidenceAcceptance; // 符合接受度
        
        // 角度分布
        std::vector<double> pdcThetas;    // 打到PDC的粒子角度
        std::vector<double> nebulaThetas; // 打到NEBULA的粒子角度
        
        AcceptanceResult() : totalEvents(0), pdcHits(0), nebulaHits(0), bothHits(0),
                            pdcAcceptance(0), nebulaAcceptance(0), coincidenceAcceptance(0) {}
    };
    
    // 粒子信息
    struct ParticleInfo {
        int eventId;
        TLorentzVector momentum;  // 4-momentum [MeV]
        TVector3 vertex;          // 反应顶点 [mm]
        int pdgCode;              // PDG code (2212=p, 2112=n)
        double charge;            // 电荷 [e]
        double mass;              // 质量 [MeV/c²]
        
        ParticleInfo() : eventId(0), momentum(0,0,0,0), vertex(0,0,0),
                        pdgCode(0), charge(0), mass(0) {}
    };

private:
    MagneticField* fMagField;
    ParticleTrajectory* fTrajectory;
    
    PDCConfiguration fPDCConfig;
    NEBULAConfiguration fNEBULAConfig;
    
    // 事例数据
    std::vector<ParticleInfo> fProtons;
    std::vector<ParticleInfo> fNeutrons;
    
    // 粒子常数
    static const double kProtonMass;   // 938.272 MeV/c²
    static const double kNeutronMass;  // 939.565 MeV/c²
    static const double kProtonCharge; // +1 e
    static const double kNeutronCharge; // 0 e

public:
    DetectorAcceptanceCalculator();
    ~DetectorAcceptanceCalculator();
    
    // 设置磁场
    void SetMagneticField(MagneticField* magField);
    
    // 配置探测器
    void SetPDCConfiguration(const PDCConfiguration& config) { fPDCConfig = config; }
    void SetNEBULAConfiguration(const NEBULAConfiguration& config) { fNEBULAConfig = config; }
    
    PDCConfiguration GetPDCConfiguration() const { return fPDCConfig; }
    NEBULAConfiguration GetNEBULAConfiguration() const { return fNEBULAConfig; }
    
    // 从QMD数据加载粒子信息
    bool LoadQMDData(const std::string& dataFile);
    bool LoadQMDDataFromDirectory(const std::string& directory);
    
    // 计算最佳PDC位置和旋转角度
    // 要求:
    // 1. PDC中心被从targetPos以targetRotationAngle发射的Pz=600MeV/c质子穿过
    // 2. PDC平面近乎垂直于该质子轨迹
    // 3. Px=±100MeV/c的质子刚好到达PDC边缘
    PDCConfiguration CalculateOptimalPDCPosition(const TVector3& targetPos,
                                                  double targetRotationAngle);
    
    // 兼容旧接口
    PDCConfiguration CalculateOptimalPDCPosition(const TVector3& targetPos) {
        return CalculateOptimalPDCPosition(targetPos, 0.0);
    }
    
    // 检查粒子是否打到探测器
    bool CheckPDCHit(const ParticleInfo& particle, TVector3& hitPosition);
    bool CheckNEBULAHit(const ParticleInfo& particle, TVector3& hitPosition);
    
    // 计算接受度
    AcceptanceResult CalculateAcceptance();
    AcceptanceResult CalculateAcceptanceForTarget(const TVector3& targetPos,
                                                  double targetRotationAngle);
    
    // 兼容旧接口
    AcceptanceResult CalculateAcceptanceForTarget(const TVector3& targetPos) {
        return CalculateAcceptanceForTarget(targetPos, 0.0);
    }
    
    // 扫描PDC位置找到最佳配置
    std::vector<PDCConfiguration> ScanPDCPositions(
        const TVector3& targetPos,
        double zMin, double zMax, double zStep
    );
    
    // 生成接受度报告
    void PrintAcceptanceReport(const AcceptanceResult& result,
                              double deflectionAngle,
                              double fieldStrength) const;
    
    // 保存结果到ROOT文件
    void SaveResults(const std::string& filename,
                    const std::map<double, AcceptanceResult>& results) const;
    
    // 清空数据
    void ClearData();
    
    // 获取数据统计
    int GetNumberOfProtons() const { return fProtons.size(); }
    int GetNumberOfNeutrons() const { return fNeutrons.size(); }
    int GetTotalParticles() const { return fProtons.size() + fNeutrons.size(); }
};

#endif // DETECTOR_ACCEPTANCE_CALCULATOR_HH
