#ifndef PDCSIMANA_HH
#define PDCSIMANA_HH

#include "TObject.h"
#include "TVector3.h"
#include "TClonesArray.h"
#include <vector>

#include "GeometryManager.hh" // Include GeometryManager
#include "RecoEvent.hh"       // 重建事件数据结构

// A simple structure to hold hit information (position along an axis and energy)
struct Hit {
    double position; // Position along U or V axis
    double energy;   // Energy deposited
    double z;        // Z position relative to PDC center (mm)
    
    Hit() : position(0), energy(0), z(0) {}
    Hit(double pos, double e, double z_val) : position(pos), energy(e), z(z_val) {}
};

class PDCSimAna : public TObject {
public:
    // Constructor now takes a reference to the geometry manager
    PDCSimAna(const GeometryManager& geo_manager);
    virtual ~PDCSimAna();

    // --- Configuration ---
    void SetSmearing(double sigma_u, double sigma_v);

    // --- Main Processing Method ---
    // 处理原始 hits 并返回重建结果
    RecoEvent ProcessEvent(TClonesArray* fragSimData);
    // 处理批处理版本（填充已有的 RecoEvent 对象，可用于批处理）
    void ProcessEvent(TClonesArray* fragSimData, RecoEvent& outEvent);

    // --- 为了向后兼容的 Getters ---
    TVector3 GetRecoPoint1() const { return fRecoPoint1; }
    TVector3 GetRecoPoint2() const { return fRecoPoint2; }
    std::vector<TVector3> GetSmearedGlobalPositions() const;

private:
    // --- Helper Methods ---
    void ClearAll();
    TVector3 ReconstructPDC(const std::vector<Hit>& u_hits, const std::vector<Hit>& v_hits, const TVector3& pdc_position) const;
    double CalculateCoM(const std::vector<Hit>& hits) const;

    // --- Geometry ---
    const GeometryManager& fGeoManager;
    double fSigmaU, fSigmaV;

    // --- Internal Hit Storage ---
    std::vector<Hit> fU1_hits, fV1_hits, fU2_hits, fV2_hits;
    std::vector<Hit> fU1_hits_smeared, fV1_hits_smeared, fU2_hits_smeared, fV2_hits_smeared;

    // --- Reconstruction Results ---
    TVector3 fRecoPoint1;
    TVector3 fRecoPoint2;

    ClassDef(PDCSimAna, 4); // Version 4 of the class
};

#endif // PDCSIMANA_HH
