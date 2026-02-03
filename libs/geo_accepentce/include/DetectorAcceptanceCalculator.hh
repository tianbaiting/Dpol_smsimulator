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
    // [EN] Detector configuration / [CN] 探测器配置
    struct PDCConfiguration {
        TVector3 position;       // [EN] PDC center position [mm] / [CN] PDC中心位置 [mm]
        TVector3 normal;         // [EN] PDC plane normal vector (unit) / [CN] PDC平面法向量（单位矢量）
        double rotationAngle;    // [EN] PDC rotation angle around Y-axis [deg] / [CN] PDC绕Y轴旋转角度 [度]
        double width;            // [EN] PDC effective width [mm] (X-direction, 2×840mm) / [CN] PDC有效宽度 [mm]
        double height;           // [EN] PDC effective height [mm] (Y-direction, 2×390mm) / [CN] PDC有效高度 [mm]
        double depth;            // [EN] PDC total depth [mm] (Z-direction, 2×190mm) / [CN] PDC总厚度 [mm]
        double pxMin;            // [EN] Minimum Px [MeV/c] / [CN] 最小Px [MeV/c]
        double pxMax;            // [EN] Maximum Px [MeV/c] / [CN] 最大Px [MeV/c]
        bool isOptimal;          // [EN] Whether this is an optimal config / [CN] 是否为最佳配置
        bool isFixed;            // [EN] Whether PDC position is fixed (not optimized) / [CN] 是否为固定位置（不优化）
        
        // [EN] PDC dimensions from PDCConstruction.cc / [CN] PDC尺寸来自PDCConstruction.cc
        // Enclosure: 1700×800×190 mm (半尺寸)
        // Active area: 840×390 mm (半尺寸，每层)
        PDCConfiguration() : position(0,0,0), normal(0,0,1), rotationAngle(0),
                            width(2*840), height(2*390), depth(2*190),
                            pxMin(-100), pxMax(100), isOptimal(false), isFixed(false) {}
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
    PDCConfiguration fPDCConfig2;
    bool fUsePdcPair = false;           // [EN] Use two PDC planes / [CN] 使用双层PDC
    bool fRequireBothPdcPlanes = true;  // [EN] Require hit on both planes / [CN] 需要同时命中两层
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
    void SetPDCConfiguration(const PDCConfiguration& config) { 
        fPDCConfig = config; 
        fUsePdcPair = false; 
        fRequireBothPdcPlanes = true;
    }
    // [EN] Set two PDC planes and require both hits / [CN] 设置两层PDC并要求同时命中
    void SetPDCConfigurationPair(const PDCConfiguration& config1,
                                 const PDCConfiguration& config2,
                                 bool requireBoth = true) {
        fPDCConfig = config1;
        fPDCConfig2 = config2;
        fUsePdcPair = true;
        fRequireBothPdcPlanes = requireBoth;
    }
    void SetNEBULAConfiguration(const NEBULAConfiguration& config) { fNEBULAConfig = config; }
    
    PDCConfiguration GetPDCConfiguration() const { return fPDCConfig; }
    PDCConfiguration GetPDCConfiguration2() const { return fPDCConfig2; }
    bool GetUsePdcPair() const { return fUsePdcPair; }
    NEBULAConfiguration GetNEBULAConfiguration() const { return fNEBULAConfig; }
    
    // 从QMD数据加载粒子信息
    // polType: optional, "y" for Y-polarization files, "z" for Z-polarization files.
    // If empty, the implementation will try to auto-detect from the file path.
    bool LoadQMDData(const std::string& dataFile, const std::string& polType = "");
    bool LoadQMDDataFromDirectory(const std::string& directory);
    
    // [EN] Calculate optimal PDC position and rotation angle / [CN] 计算最佳PDC位置和旋转角度
    // [EN] Requirements: / [CN] 要求:
    // 1. PDC center is traversed by proton with Pz=600MeV/c emitted from targetPos at targetRotationAngle
    // 2. PDC plane is nearly perpendicular to the proton trajectory
    // 3. Protons with Px=±pxRange reach the PDC edges
    // [EN] pxRange: The Px range for edge protons [MeV/c], default 100 / [CN] pxRange: 边缘质子的Px范围 [MeV/c]，默认100
    PDCConfiguration CalculateOptimalPDCPosition(const TVector3& targetPos,
                                                  double targetRotationAngle,
                                                  double pxRange = 100.0);
    
    // [EN] Legacy interface / [CN] 兼容旧接口
    PDCConfiguration CalculateOptimalPDCPosition(const TVector3& targetPos) {
        return CalculateOptimalPDCPosition(targetPos, 0.0, 100.0);
    }
    
    // [EN] Create a fixed PDC configuration at given position / [CN] 创建固定位置的PDC配置
    // [EN] Use this when you want to use a pre-determined PDC position instead of optimization
    // [CN] 当你想使用预设的PDC位置而不是优化时使用此方法
    PDCConfiguration CreateFixedPDCConfiguration(const TVector3& pdcPosition,
                                                  double pdcRotationAngle,
                                                  double pxMin = -100.0,
                                                  double pxMax = 100.0);
    
    // 检查粒子是否打到探测器
    bool CheckPDCHit(const ParticleInfo& particle, TVector3& hitPosition);
    // [EN] Check hit against an explicit PDC configuration / [CN] 使用显式PDC配置检查命中
    bool CheckPDCHitWithConfig(const ParticleInfo& particle,
                               const PDCConfiguration& config,
                               TVector3& hitPosition) const;
    bool CheckNEBULAHit(const ParticleInfo& particle, TVector3& hitPosition);
    
    // [EN] Calculate acceptance with current PDC/NEBULA configuration / [CN] 使用当前PDC/NEBULA配置计算接受度
    AcceptanceResult CalculateAcceptance();
    
    // [EN] Calculate acceptance for given target position / [CN] 计算给定靶位置的接受度
    // [EN] pxRange: Px range for PDC optimization [MeV/c], default 100 / [CN] pxRange: PDC优化的Px范围 [MeV/c]，默认100
    AcceptanceResult CalculateAcceptanceForTarget(const TVector3& targetPos,
                                                  double targetRotationAngle,
                                                  double pxRange = 100.0);
    
    // [EN] Legacy interface / [CN] 兼容旧接口
    AcceptanceResult CalculateAcceptanceForTarget(const TVector3& targetPos) {
        return CalculateAcceptanceForTarget(targetPos, 0.0, 100.0);
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
