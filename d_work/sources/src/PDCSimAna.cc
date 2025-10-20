#include "PDCSimAna.hh"
#include "TSimData.hh"
#include <cmath>
#include <iostream>

#include "TMath.h"
#include "TRandom3.h"

ClassImp(PDCSimAna);

PDCSimAna::PDCSimAna(const GeometryManager& geo_manager)
    : fGeoManager(geo_manager), fSigmaU(0.), fSigmaV(0.) {
    ClearAll();
}

PDCSimAna::~PDCSimAna() {}

void PDCSimAna::SetSmearing(double sigma_u, double sigma_v) {
    fSigmaU = sigma_u;
    fSigmaV = sigma_v;
}

void PDCSimAna::ClearAll() {
    fU1_hits.clear();
    fV1_hits.clear();
    fU2_hits.clear();
    fV2_hits.clear();
    fU1_hits_smeared.clear();
    fV1_hits_smeared.clear();
    fU2_hits_smeared.clear();
    fV2_hits_smeared.clear();
    fRecoPoint1.SetXYZ(0, 0, 0);
    fRecoPoint2.SetXYZ(0, 0, 0);
}

// 新接口 - 返回值版本
RecoEvent PDCSimAna::ProcessEvent(TClonesArray* fragSimData) {
    RecoEvent result;
    ProcessEvent(fragSimData, result);
    return result;
}

// 新接口 - 输出参数版本
void PDCSimAna::ProcessEvent(TClonesArray* fragSimData, RecoEvent& outEvent) {
    // 清除内部状态和输出事件
    ClearAll();
    outEvent.Clear();
    if (!fragSimData) return;

    const double sqrt2 = TMath::Sqrt(2.0);
    const double angle = fGeoManager.GetAngleRad();
    TVector3 pdc1_beforeRotation_position=fGeoManager.GetPDC1Position();
    TVector3 pdc2_beforeRotation_position=fGeoManager.GetPDC2Position();
    pdc1_beforeRotation_position.RotateY(-angle);
    pdc2_beforeRotation_position.RotateY(-angle);
    const TVector3 pos1 = pdc1_beforeRotation_position;
    const TVector3 pos2 = pdc2_beforeRotation_position;

    // std::cout << "PDC1 Position: " << pos1.X() << ", " << pos1.Y() << ", " << pos1.Z() << std::endl;
    // std::cout << "PDC2 Position: " << pos2.X() << ", " << pos2.Y() << ", " << pos2.Z() << std::endl;

    // 1. Sort raw hits from TClonesArray
    int n_entries = fragSimData->GetEntries();
    for (int i = 0; i < n_entries; ++i) {
        TSimData* hit = (TSimData*)fragSimData->At(i);
        if (!hit || !hit->fDetectorName.Contains("PDC")) continue;

        TVector3 pos = hit->fPrePosition;
        if (hit->fID == 0) { // PDC1
            TVector3 pos_rel = pos - pos1;
            double x_rot = pos_rel.X() * TMath::Cos(-angle) - pos_rel.Z() * TMath::Sin(-angle);
            double u_pos = (x_rot + pos.Y()) / sqrt2;
            double v_pos = (-x_rot + pos.Y()) / sqrt2;
            
            // Set z-offset according to layer type
            double z_offset = 0.0;
            if (hit->fModuleName == "U") {
                z_offset = -12.0; // U Layer at -12 mm
                fU1_hits.push_back(Hit(u_pos, hit->fEnergyDeposit, z_offset));
            }
            if (hit->fModuleName == "V") {
                z_offset = +12.0; // V Layer at +12 mm
                fV1_hits.push_back(Hit(v_pos, hit->fEnergyDeposit, z_offset));
            }
        } else if (hit->fID == 1) { // PDC2
            TVector3 pos_rel = pos - pos2;
            double x_rot = pos_rel.X() * TMath::Cos(-angle) - pos_rel.Z() * TMath::Sin(-angle);
            double u_pos = (x_rot + pos.Y()) / sqrt2;
            double v_pos = (-x_rot + pos.Y()) / sqrt2;

            
            // Set z-offset according to layer type
            double z_offset = 0.0;
            if (hit->fModuleName == "U") {
                z_offset = -12.0; // U Layer at -12 mm
                fU2_hits.push_back(Hit(u_pos, hit->fEnergyDeposit, z_offset));
            }
            if (hit->fModuleName == "V") {
                z_offset = +12.0; // V Layer at +12 mm
                fV2_hits.push_back(Hit(v_pos, hit->fEnergyDeposit, z_offset));
            }
        }
    }

    // 2. Perform smearing if sigma > 0
    if (!gRandom) gRandom = new TRandom3(0);
    auto smear_vector = [&](const std::vector<Hit>& in_hits, std::vector<Hit>& out_hits, double sigma) {
        for (const auto& hit : in_hits) {
            out_hits.push_back(Hit(hit.position + gRandom->Gaus(0, sigma), hit.energy, hit.z));
        }
    };

    smear_vector(fU1_hits, fU1_hits_smeared, fSigmaU);
    smear_vector(fV1_hits, fV1_hits_smeared, fSigmaV);
    smear_vector(fU2_hits, fU2_hits_smeared, fSigmaU);
    smear_vector(fV2_hits, fV2_hits_smeared, fSigmaV);

    // 3. Perform reconstruction using the smeared hits
    fRecoPoint1 = ReconstructPDC(fU1_hits_smeared, fV1_hits_smeared, pos1);
    fRecoPoint2 = ReconstructPDC(fU2_hits_smeared, fV2_hits_smeared, pos2);
    
    // 4. 填充 RecoEvent 对象
    // 添加原始击中
    for (int i = 0; i < n_entries; ++i) {
        TSimData* hit = (TSimData*)fragSimData->At(i);
        if (!hit || !hit->fDetectorName.Contains("PDC")) continue;
        
        outEvent.rawHits.push_back(RecoHit(hit->fPrePosition, hit->fEnergyDeposit));
    }
    
    // 添加涂抹后的点（获取完整的全局位置列表）
    std::vector<TVector3> smearedPositions = GetSmearedGlobalPositions();
    for (const auto& pos : smearedPositions) {
        outEvent.smearedHits.push_back(RecoHit(pos, 1.0)); // 能量设为1.0作为权重
    }
    
    // 添加重建轨迹（如果有两个点）
    if (fRecoPoint1.Mag() > 0 && fRecoPoint2.Mag() > 0) {
        outEvent.tracks.push_back(RecoTrack(fRecoPoint1, fRecoPoint2));
    }
}

std::vector<TVector3> PDCSimAna::GetSmearedGlobalPositions() const {
    std::vector<TVector3> positions;
    // Re-calculate global positions from the 1D smeared hits
    auto add_positions = [&](const std::vector<Hit>& u_hits, const std::vector<Hit>& v_hits, const TVector3& pdc_pos) {
        // This is a simplified approach: for each u hit, find the average v, and reconstruct.
        // A more advanced method would handle multiple combinations.
        if (u_hits.empty() || v_hits.empty()) return;
        double v_avg = CalculateCoM(v_hits);
        
        // 获取 v_hits 的平均 z 偏移
        double v_z_avg = 12.0; // 默认 V 层位置
        if (!v_hits.empty()) {
            double z_sum = 0.0, e_sum = 0.0;
            for (const auto& hit : v_hits) {
                z_sum += hit.z * hit.energy;
                e_sum += hit.energy;
            }
            if (e_sum > 0) v_z_avg = z_sum / e_sum;
        }
        
        for (const auto& u_hit : u_hits) {
            std::vector<Hit> temp_u;
            std::vector<Hit> temp_v;
            temp_u.push_back(Hit(u_hit.position, 1.0, u_hit.z));
            temp_v.push_back(Hit(v_avg, 1.0, v_z_avg));
            positions.push_back(ReconstructPDC(temp_u, temp_v, pdc_pos));
        }
    };

    // Rotate PDC center positions to match the coordinate system used above
    TVector3 pdc1_pos_rot = fGeoManager.GetPDC1Position();
    pdc1_pos_rot.RotateY(-fGeoManager.GetAngleRad());
    TVector3 pdc2_pos_rot = fGeoManager.GetPDC2Position();
    pdc2_pos_rot.RotateY(-fGeoManager.GetAngleRad());

    add_positions(fU1_hits_smeared, fV1_hits_smeared, pdc1_pos_rot);
    add_positions(fU2_hits_smeared, fV2_hits_smeared, pdc2_pos_rot);

    return positions;
}

TVector3 PDCSimAna::ReconstructPDC(const std::vector<Hit>& u_hits, const std::vector<Hit>& v_hits, const TVector3& pdc_position) const {
    if (u_hits.empty() || v_hits.empty()) {
        return TVector3(0, 0, 0);
    }

    double u_com = CalculateCoM(u_hits);
    double v_com = CalculateCoM(v_hits);
    
    // // 计算 z 坐标的加权平均，每层的 z 偏移在之前已经设置
    // double z_offset_u = 0.0;
    // double total_energy_u = 0.0;
    // for (const auto& hit : u_hits) {
    //     z_offset_u += hit.z * hit.energy;
    //     total_energy_u += hit.energy;
    // }
    // z_offset_u = (total_energy_u > 0) ? (z_offset_u / total_energy_u) : -12.0; // 默认值为 U 层位置
    
    // double z_offset_v = 0.0;
    // double total_energy_v = 0.0;
    // for (const auto& hit : v_hits) {
    //     z_offset_v += hit.z * hit.energy;
    //     total_energy_v += hit.energy;
    // }
    // z_offset_v = (total_energy_v > 0) ? (z_offset_v / total_energy_v) : 12.0; // 默认值为 V 层位置
    
    // // 使用两层的平均 z 偏移
    // double z_offset = (z_offset_u + z_offset_v) / 2.0;

    const double sqrt2 = TMath::Sqrt(2.0);
    double y_global = (u_com + v_com) / sqrt2;
    double x_local = (u_com - v_com) / sqrt2;

    // TVector3 pdc_in_global = pdc_position;
    const double angle = fGeoManager.GetAngleRad();
    double x_rotatedBack = x_local * TMath::Cos(angle);
    double z_rotatedBack = x_local * TMath::Sin(angle);

    double final_X = x_rotatedBack + pdc_position.X();
    double final_Y = y_global + pdc_position.Y();
    double final_Z = z_rotatedBack + pdc_position.Z(); // 加上 z 层的偏移

    return TVector3(final_X, final_Y, final_Z);
}

double PDCSimAna::CalculateCoM(const std::vector<Hit>& hits) const {
    if (hits.empty()) return 0.0;
    double total_energy = 0.0;
    double weighted_sum = 0.0;
    for (const auto& hit : hits) {
        weighted_sum += hit.position * hit.energy;
        total_energy += hit.energy;
    }
    return (total_energy > 0) ? (weighted_sum / total_energy) : 0.0;
}
