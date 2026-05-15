#ifndef NEBULAJOINTRECO_HH
#define NEBULAJOINTRECO_HH

#include "NEBULABaseReco.hh"

class TClonesArray;

// Combined NEBULA + NEBULA-Plus reconstruction.
// Merges hits from both detectors into a single NEBULAHit array; the template
// method in NEBULABaseReco then runs clustering and neutron reconstruction on
// the merged set.  wall_tag follows the convention:
//   1 = NPL Wall-A, 2 = NPL Wall-B  (upstream, from TArtNEBULAPlusPla)
//   3 = NEBULA Wall-A, 4 = NEBULA Wall-B  (downstream, from TArtNEBULAPla)
class NebulaJointReco : public NEBULABaseReco {
public:
    explicit NebulaJointReco(const GeometryManager& geo) : NEBULABaseReco(geo) {}
    ~NebulaJointReco() override = default;

    void SetInputs(TClonesArray* neb_hits, TClonesArray* npl_hits) {
        fNebHits = neb_hits;
        fNplHits = npl_hits;
    }

protected:
    std::vector<NEBULAHit> ExtractHits() override;

private:
    TClonesArray* fNebHits = nullptr;
    TClonesArray* fNplHits = nullptr;

    ClassDef(NebulaJointReco, 1);
};

#endif // NEBULAJOINTRECO_HH
