#ifndef NEBULAPLUSRECO_HH
#define NEBULAPLUSRECO_HH

#include "NEBULABaseReco.hh"

class TClonesArray;

// Single-detector NEBULA-Plus reconstruction.
// Consumes fNEBULAPlusSimData branch (TClonesArray<TArtNEBULAPlusPla>).
class NEBULAPlusReco : public NEBULABaseReco {
public:
    explicit NEBULAPlusReco(const GeometryManager& geo) : NEBULABaseReco(geo) {}
    ~NEBULAPlusReco() override = default;

    void SetInput(TClonesArray* npl_hits) { fNplHits = npl_hits; }

    // Convenience overload matching the NEBULAReco style.
    std::vector<RecoNeutron> ReconstructNeutrons(TClonesArray* npl_hits) {
        SetInput(npl_hits);
        return NEBULABaseReco::ReconstructNeutrons();
    }
    using NEBULABaseReco::ReconstructNeutrons;

protected:
    std::vector<NEBULAHit> ExtractHits() override;

private:
    TClonesArray* fNplHits = nullptr;

    ClassDef(NEBULAPlusReco, 1);
};

#endif // NEBULAPLUSRECO_HH
