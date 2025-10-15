#ifndef PDCSIMANA_HH
#define PDCSIMANA_HH

#include "TObject.h"
#include "TVector3.h"
#include "TClonesArray.h"
#include <vector>

#include "GeometryManager.hh" // Include GeometryManager

// A simple structure to hold hit information (position along an axis and energy)
struct Hit {
    double position; // Position along U or V axis
    double energy;   // Energy deposited
};

class PDCSimAna : public TObject {
public:
    // Constructor now takes a reference to the geometry manager
    PDCSimAna(const GeometryManager& geo_manager);
    virtual ~PDCSimAna();

    // --- Configuration ---
    void SetSmearing(double sigma_u, double sigma_v);

    // --- Main Processing Method ---
    // Processes all hits from the FragSimData TClonesArray for one event
    void ProcessEvent(TClonesArray* fragSimData);

    // --- Getters for Results ---
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
