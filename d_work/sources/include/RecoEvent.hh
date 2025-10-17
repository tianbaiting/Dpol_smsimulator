#ifndef RECO_EVENT_H
#define RECO_EVENT_H

#include "TObject.h"
#include "TVector3.h"
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

// 整个重建事件的数据结构
class RecoEvent : public TObject {
public:
    RecoEvent() : eventID(-1) {}
    virtual ~RecoEvent() {}

    void Clear() {
        rawHits.clear();
        smearedHits.clear();
        tracks.clear();
        eventID = -1;
    }

    Long64_t eventID;                // 事件编号
    std::vector<RecoHit> rawHits;    // 原始击中点
    std::vector<RecoHit> smearedHits;// 添加位置分辨率后的点
    std::vector<RecoTrack> tracks;   // 重建的轨迹

    ClassDef(RecoEvent, 1);
};

#endif // RECO_EVENT_H