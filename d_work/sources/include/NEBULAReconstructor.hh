#ifndef NEBULARECONSTRUCTOR_HH
#define NEBULARECONSTRUCTOR_HH

#include "TObject.h"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TClonesArray.h"
#include <vector>

#include "GeometryManager.hh"
#include "RecoEvent.hh"

// 前向声明，RecoNeutron已在RecoEvent.hh中定义
struct RecoNeutron;

// NEBULA击中信息结构
struct NEBULAHit {
    int moduleID;        // 模块ID
    TVector3 position;   // 3D位置
    double energy;       // 能量沉积
    double time;         // 时间
    double qave;         // 平均电荷
    
    NEBULAHit() : moduleID(-1), position(0,0,0), energy(0), time(0), qave(0) {}
    NEBULAHit(int id, const TVector3& pos, double e, double t, double q) 
        : moduleID(id), position(pos), energy(e), time(t), qave(q) {}
};

class NEBULAReconstructor : public TObject {
public:
    NEBULAReconstructor(const GeometryManager& geo_manager);
    virtual ~NEBULAReconstructor();
    
    // 设置重建参数
    void SetTimeWindow(double window) { fTimeWindow = window; }           // ns
    void SetEnergyThreshold(double threshold) { fEnergyThreshold = threshold; } // MeV
    void SetPositionSmearing(double sigma) { fPositionSmearing = sigma; } // mm
    void SetTimeSmearing(double sigma) { fTimeSmearing = sigma; }         // ns
    void SetTargetPosition(const TVector3& pos) { fTargetPosition = pos; }
    
    // 主要重建方法
    std::vector<RecoNeutron> ReconstructNeutrons(TClonesArray* nebulaData);
    
    // 添加到RecoEvent的便捷方法
    void ProcessEvent(TClonesArray* nebulaData, RecoEvent& event);
    
    // 获取当前设置
    double GetTimeWindow() const { return fTimeWindow; }
    double GetEnergyThreshold() const { return fEnergyThreshold; }
    
private:
    // 辅助方法
    void ClearAll();
    std::vector<NEBULAHit> ExtractHits(TClonesArray* nebulaData);
    std::vector<std::vector<NEBULAHit>> ClusterHits(const std::vector<NEBULAHit>& hits);
    RecoNeutron ReconstructFromCluster(const std::vector<NEBULAHit>& cluster);
    double CalculateBeta(double flightLength, double tof);
    double CalculateNeutronEnergy(double beta);
    TVector3 ApplyPositionSmearing(const TVector3& pos);
    double ApplyTimeSmearing(double time);
    
    // 几何和配置
    const GeometryManager& fGeoManager;
    TVector3 fTargetPosition;
    
    // 重建参数
    double fTimeWindow;         // 时间窗口 (ns)
    double fEnergyThreshold;    // 能量阈值 (MeV)
    double fPositionSmearing;   // 位置模糊 (mm)
    double fTimeSmearing;       // 时间模糊 (ns)
    
    // 物理常数
    static const double kLightSpeed; // cm/ns
    static const double kNeutronMass; // MeV/c²
    
    ClassDef(NEBULAReconstructor, 1);
};

#endif // NEBULARECONSTRUCTOR_HH