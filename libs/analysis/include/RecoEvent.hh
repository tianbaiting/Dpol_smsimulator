#ifndef RECO_EVENT_H
#define RECO_EVENT_H

#include "TObject.h"
#include "TVector3.h"
#include "TLorentzVector.h"
#include <vector>

// 单个重建点（可以是原始击中或重建后的点）
struct RecoHit : public TObject {
    TVector3 position;   // 3D 全局位置
    double energy;       // 能量沉积

    RecoHit() : position(0,0,0), energy(0) {}
    RecoHit(const TVector3& pos, double e) : position(pos), energy(e) {}

    ClassDef(RecoHit, 1);
};

// 重建轨迹
struct RecoTrack : public TObject {
    TVector3 start;      // 起点
    TVector3 end;        // 终点
    int pdgCode;         // 粒子类型
    double chi2;         // 拟合质量（可选）

    RecoTrack() : start(0,0,0), end(0,0,0), pdgCode(0), chi2(0) {}
    RecoTrack(const TVector3& s, const TVector3& e) : start(s), end(e), pdgCode(0), chi2(0) {}

    ClassDef(RecoTrack, 1);
};

// 前向声明
struct RecoNeutron;

// 整个重建事件的数据结构
class RecoEvent : public TObject {
public:
    RecoEvent() : eventID(-1) {}
    virtual ~RecoEvent() {}

    void Clear() {
        rawHits.clear();
        smearedHits.clear();
        tracks.clear();
        neutrons.clear();
        eventID = -1;
    }

    Long64_t eventID;                    // 事件编号
    std::vector<RecoHit> rawHits;        // 原始击中点
    std::vector<RecoHit> smearedHits;    // 添加位置分辨率后的点
    std::vector<RecoTrack> tracks;       // 重建的轨迹（带电粒子）
    std::vector<RecoNeutron> neutrons;   // 重建的中子

    ClassDef(RecoEvent, 2);
};

// 重建的中子信息
struct RecoNeutron : public TObject {
    TVector3 position;      // 击中位置
    TVector3 direction;     // 飞行方向 (归一化)
    double energy;          // 重建能量
    double timeOfFlight;    // 飞行时间
    double beta;            // 速度/光速
    double flightLength;    // 飞行距离
    int hitMultiplicity;    // 击中模块数
    
    RecoNeutron() : position(0,0,0), direction(0,0,1), energy(0), 
                   timeOfFlight(0), beta(0), flightLength(0), hitMultiplicity(0) {}
    
    // 计算动量向量 (MeV/c)
    TVector3 GetMomentum() const {
        if (beta <= 0 || beta >= 1) return TVector3(0,0,0);
        double gamma = 1.0 / sqrt(1.0 - beta*beta);
        double momentum = 939.565 * gamma * beta; // 中子质量 939.565 MeV/c²
        return momentum * direction;
    }
    
    // 获取四动量
    TLorentzVector GetFourMomentum() const {
        TVector3 p = GetMomentum();
        double E = sqrt(p.Mag2() + 939.565*939.565); // E² = p² + m²
        return TLorentzVector(p, E);
    }
    
    ClassDef(RecoNeutron, 1);
};

#endif // RECO_EVENT_H