#include "NEBULABaseReco.hh"
#include "SMLogger.hh"
#include "TRandom3.h"
#include <algorithm>
#include <cmath>

// 物理常数
const double NEBULABaseReco::kLightSpeed  = 29.9792458; // cm/ns
const double NEBULABaseReco::kNeutronMass = 939.565;    // MeV/c²

ClassImp(NEBULABaseReco)

NEBULABaseReco::NEBULABaseReco(const GeometryManager& geo)
    : fGeoManager(geo) {
    SM_INFO("NEBULABaseReco initialized with default parameters:");
    SM_INFO("  Time window: {} ns", fTimeWindow);
    SM_INFO("  Energy threshold: {} MeV", fEnergyThreshold);
    SM_INFO("  Position smearing: {} mm", fPositionSmearing);
    SM_INFO("  Time smearing: {} ns", fTimeSmearing);
}

// ---------------------------------------------------------------------------
// Template-method driver
// ---------------------------------------------------------------------------

std::vector<RecoNeutron> NEBULABaseReco::ReconstructNeutrons() {
    auto hits = ExtractHits();
    std::vector<RecoNeutron> neutrons;
    if (hits.empty()) return neutrons;

    auto clusters = ClusterHits(hits);
    for (const auto& cl : clusters) {
        RecoNeutron n = ReconstructFromCluster(cl);
        if (n.hitMultiplicity > 0) {
            neutrons.push_back(n);
        }
    }
    return neutrons;
}

void NEBULABaseReco::ProcessEvent(RecoEvent& event) {
    SM_DEBUG("NEBULABaseReco::ProcessEvent 开始");

    std::vector<RecoNeutron> neutrons = ReconstructNeutrons();

    SM_INFO("重建总结: 发现 {} 个中子", neutrons.size());

    // 将中子信息添加到 RecoEvent (matches legacy ProcessEvent semantics exactly)
    event.neutrons = neutrons;

    for (const auto& neutron : neutrons) {
        RecoTrack neutronTrack;
        neutronTrack.start   = fTargetPosition;
        neutronTrack.end     = neutron.position;
        neutronTrack.pdgCode = 2112;             // 中子的PDG码
        neutronTrack.chi2    = neutron.energy;   // 临时使用chi2存储能量
        event.tracks.push_back(neutronTrack);

        RecoHit hit(neutron.position, neutron.energy);
        event.rawHits.push_back(hit);
    }
}

// ---------------------------------------------------------------------------
// Shared algorithm steps (ported verbatim from NEBULAReco)
// ---------------------------------------------------------------------------

std::vector<std::vector<NEBULAHit>> NEBULABaseReco::ClusterHits(
        const std::vector<NEBULAHit>& hits) {
    std::vector<std::vector<NEBULAHit>> clusters;

    // 简单的时间聚类算法 — 按时间排序后贪心分组
    std::vector<NEBULAHit> sortedHits = hits;
    std::sort(sortedHits.begin(), sortedHits.end(),
              [](const NEBULAHit& a, const NEBULAHit& b) {
                  return a.time < b.time;
              });

    std::vector<bool> used(sortedHits.size(), false);

    for (size_t i = 0; i < sortedHits.size(); ++i) {
        if (used[i]) continue;

        std::vector<NEBULAHit> cluster;
        cluster.push_back(sortedHits[i]);
        used[i] = true;

        for (size_t j = i + 1; j < sortedHits.size(); ++j) {
            if (used[j]) continue;
            if (std::abs(sortedHits[j].time - sortedHits[i].time) <= fTimeWindow) {
                cluster.push_back(sortedHits[j]);
                used[j] = true;
            } else {
                break; // 时间已排序，后面的都超出窗口
            }
        }

        clusters.push_back(cluster);
    }

    return clusters;
}

RecoNeutron NEBULABaseReco::ReconstructFromCluster(
        const std::vector<NEBULAHit>& cluster) {
    RecoNeutron neutron;
    if (cluster.empty()) return neutron;

    // 计算加权质心位置
    TVector3 weightedPos(0, 0, 0);
    double totalWeight = 0;
    double avgTime     = 0;
    double totalEnergy = 0;

    for (const auto& hit : cluster) {
        double weight = hit.energy;
        weightedPos += weight * hit.position;
        totalWeight += weight;
        avgTime     += hit.time * weight;
        totalEnergy += hit.energy;
    }

    if (totalWeight > 0) {
        neutron.position = weightedPos * (1.0 / totalWeight);
        avgTime /= totalWeight;
    } else {
        neutron.position = cluster[0].position;
        avgTime = cluster[0].time;
    }

    neutron.energy         = totalEnergy;
    neutron.hitMultiplicity = cluster.size();
    neutron.timeOfFlight   = avgTime;

    // 计算飞行方向和距离
    TVector3 flightVector = neutron.position - fTargetPosition;
    neutron.flightLength  = flightVector.Mag();

    if (neutron.flightLength > 0) {
        neutron.direction = flightVector.Unit();
    } else {
        neutron.direction = TVector3(0, 0, 1);
    }

    // 计算beta值
    neutron.beta = CalculateBeta(neutron.flightLength, neutron.timeOfFlight);

    SM_INFO("=== NEBULA中子重建结果 ===");
    SM_INFO("聚类包含hits数: {}", cluster.size());
    SM_INFO("重建位置: ({:.2f}, {:.2f}, {:.2f}) mm",
            neutron.position.X(), neutron.position.Y(), neutron.position.Z());
    SM_INFO("飞行方向: ({:.4f}, {:.4f}, {:.4f})",
            neutron.direction.X(), neutron.direction.Y(), neutron.direction.Z());
    SM_INFO("飞行距离: {:.2f} mm", neutron.flightLength);
    SM_INFO("飞行时间: {:.2f} ns", neutron.timeOfFlight);
    SM_INFO("Beta值: {:.4f}", neutron.beta);
    SM_INFO("总能量: {:.2f} MeV", neutron.energy);

    double neutronKE = CalculateNeutronEnergy(neutron.beta);
    SM_INFO("中子动能: {:.2f} MeV", neutronKE);

    double theta = neutron.direction.Theta() * 180.0 / TMath::Pi();
    double phi   = neutron.direction.Phi()   * 180.0 / TMath::Pi();
    SM_INFO("极角θ: {:.2f}°", theta);
    SM_INFO("方位角φ: {:.2f}°", phi);
    SM_INFO("=========================");

    return neutron;
}

double NEBULABaseReco::CalculateBeta(double flightLength, double tof) {
    if (tof <= 0 || flightLength <= 0) return 0;

    // flightLength 单位: mm, tof 单位: ns → 转换为 cm/ns
    double velocity = (flightLength / 10.0) / tof;
    double beta     = velocity / kLightSpeed;

    if (beta < 0)    beta = 0;
    if (beta > 0.99) beta = 0.99;

    return beta;
}

double NEBULABaseReco::CalculateNeutronEnergy(double beta) {
    if (beta <= 0 || beta >= 1) return 0;

    double gamma         = 1.0 / sqrt(1.0 - beta * beta);
    double kineticEnergy = (gamma - 1.0) * kNeutronMass;
    return kineticEnergy;
}

TVector3 NEBULABaseReco::ApplyPositionSmearing(const TVector3& pos) {
    if (fPositionSmearing <= 0) return pos;

    TRandom3 rand;
    TVector3 smearedPos = pos;
    smearedPos.SetX(pos.X() + rand.Gaus(0, fPositionSmearing));
    smearedPos.SetY(pos.Y() + rand.Gaus(0, fPositionSmearing));
    smearedPos.SetZ(pos.Z() + rand.Gaus(0, fPositionSmearing));
    return smearedPos;
}

double NEBULABaseReco::ApplyTimeSmearing(double time) {
    if (fTimeSmearing <= 0) return time;

    TRandom3 rand;
    return time + rand.Gaus(0, fTimeSmearing);
}
