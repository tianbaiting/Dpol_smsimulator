#ifndef NEBULARECO_HH
#define NEBULARECO_HH

#include "NEBULABaseReco.hh"

class TClonesArray;

// Single-detector NEBULA reconstruction.
// Consumes the `NEBULAPla` branch (same reflection-based extraction as the
// legacy NEBULAReconstructor). Drop-in replacement for the legacy class.
class NEBULAReco : public NEBULABaseReco {
public:
    explicit NEBULAReco(const GeometryManager& geo) : NEBULABaseReco(geo) {}
    ~NEBULAReco() override = default;

    // Set input for the next ReconstructNeutrons() call.
    void SetInput(TClonesArray* neb_hits) { fNebHits = neb_hits; }

    // Legacy-compatible overload: sets the input then runs the pipeline.
    // This is the API used by callers and the test.
    std::vector<RecoNeutron> ReconstructNeutrons(TClonesArray* neb_hits) {
        SetInput(neb_hits);
        return NEBULABaseReco::ReconstructNeutrons();
    }

    // Also expose the zero-argument base-class version.
    using NEBULABaseReco::ReconstructNeutrons;

    // Legacy-compatible ProcessEvent overload.
    void ProcessEvent(TClonesArray* neb_hits, RecoEvent& event) {
        SetInput(neb_hits);
        NEBULABaseReco::ProcessEvent(event);
    }

protected:
    std::vector<NEBULAHit> ExtractHits() override;

private:
    TClonesArray* fNebHits = nullptr;

    ClassDef(NEBULAReco, 1);
};

#endif // NEBULARECO_HH
