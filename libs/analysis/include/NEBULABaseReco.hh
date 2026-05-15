#ifndef NEBULABASERECO_HH
#define NEBULABASERECO_HH

#include "TObject.h"
#include "TVector3.h"
#include <vector>

#include "GeometryManager.hh"
#include "RecoEvent.hh"

// NEBULAHit struct — unified hit view. Lives here so derived classes
// (NEBULAReco, NEBULAPlusReco, NebulaJointReco) can return it from ExtractHits.
// wall_tag defaults to 0 (single-detector reco); the joint reco sets 1-4.
struct NEBULAHit {
    int      moduleID = -1;
    TVector3 position;
    double   energy   = 0;
    double   time     = 0;
    double   qave     = 0;
    int      wall_tag = 0;  // 0=unknown, 1=NPL-A, 2=NPL-B, 3=NEBULA-A, 4=NEBULA-B

    NEBULAHit() = default;
    NEBULAHit(int id, const TVector3& pos, double e, double t, double q)
        : moduleID(id), position(pos), energy(e), time(t), qave(q) {}
};

class NEBULABaseReco : public TObject {
public:
    explicit NEBULABaseReco(const GeometryManager& geo);
    virtual ~NEBULABaseReco() = default;

    // 参数设置 (legacy-compatible setters)
    void SetTimeWindow(double w)               { fTimeWindow = w; }
    void SetEnergyThreshold(double t)          { fEnergyThreshold = t; }
    void SetPositionSmearing(double s)         { fPositionSmearing = s; }
    void SetTimeSmearing(double s)             { fTimeSmearing = s; }
    void SetTargetPosition(const TVector3& p)  { fTargetPosition = p; }

    double GetTimeWindow()      const { return fTimeWindow; }
    double GetEnergyThreshold() const { return fEnergyThreshold; }

    // Template-method workflow: derived class's ExtractHits() feeds the pipeline.
    std::vector<RecoNeutron> ReconstructNeutrons();

    // Convenience: run pipeline and populate event (matches legacy ProcessEvent semantics).
    void ProcessEvent(RecoEvent& event);

protected:
    // Derived classes supply the detector-specific hit extraction.
    virtual std::vector<NEBULAHit> ExtractHits() = 0;

    // Shared algorithm steps ported from legacy NEBULAReco.
    std::vector<std::vector<NEBULAHit>> ClusterHits(const std::vector<NEBULAHit>& hits);
    RecoNeutron  ReconstructFromCluster(const std::vector<NEBULAHit>& cluster);
    double       CalculateBeta(double flightLength, double tof);
    double       CalculateNeutronEnergy(double beta);
    TVector3     ApplyPositionSmearing(const TVector3& pos);
    double       ApplyTimeSmearing(double time);

    const GeometryManager& fGeoManager;
    TVector3 fTargetPosition{0, 0, 0};

    // Defaults match legacy NEBULAReco constructor values exactly.
    double fTimeWindow       = 10.0;   // ns
    double fEnergyThreshold  = 1.0;    // MeV
    double fPositionSmearing = 5.0;    // mm
    double fTimeSmearing     = 0.5;    // ns

    static const double kLightSpeed;   // cm/ns
    static const double kNeutronMass;  // MeV/c²

    ClassDef(NEBULABaseReco, 1);
};

#endif // NEBULABASERECO_HH
