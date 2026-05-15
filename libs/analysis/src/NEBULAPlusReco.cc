#include "NEBULAPlusReco.hh"
#include "TArtNEBULAPlusPla.hh"
#include "SMLogger.hh"
#include "TClonesArray.h"

ClassImp(NEBULAPlusReco)

// ---------------------------------------------------------------------------
// ExtractHits — reads TArtNEBULAPlusPla objects from fNplHits.
// TArtNEBULAPlusPla has public fields, so direct member access is used
// (no reflection needed, unlike NEBULAReco's legacy path).
// Veto bars (fIsVeto) and padding entries (fSubLayer == 0) are skipped.
// wall_tag: 1 = NPL Wall-A, 2 = NPL Wall-B (mirrors fLayer).
// ---------------------------------------------------------------------------

std::vector<NEBULAHit> NEBULAPlusReco::ExtractHits() {
    std::vector<NEBULAHit> hits;
    if (!fNplHits) return hits;

    const Int_t n = fNplHits->GetEntriesFast();
    SM_DEBUG("NEBULAPlusReco::ExtractHits: processing {} NEBULA-Plus raw hits", n);
    hits.reserve(n);

    for (Int_t i = 0; i < n; ++i) {
        auto* pla = static_cast<TArtNEBULAPlusPla*>(fNplHits->UncheckedAt(i));
        if (!pla) continue;
        if (pla->fIsVeto || pla->fSubLayer == 0) continue;  // skip veto bars

        NEBULAHit h;
        h.moduleID = pla->fID;
        h.position = pla->fPos;
        h.energy   = pla->fEdep;
        h.time     = pla->fTime;
        h.qave     = 0.5 * (pla->fQU + pla->fQD);
        h.wall_tag = pla->fLayer;  // 1 = NPL-A, 2 = NPL-B

        SM_DEBUG("NEBULAPlusReco hit {}: ID={}, Layer={}, Pos=({},{},{}), Time={}ns, Edep={}MeV",
                 i, h.moduleID, pla->fLayer,
                 h.position.X(), h.position.Y(), h.position.Z(),
                 h.time, h.energy);

        if (h.energy < fEnergyThreshold) continue;

        // Apply position and time smearing
        h.position = ApplyPositionSmearing(h.position);
        h.time     = ApplyTimeSmearing(h.time);

        hits.push_back(h);
    }

    return hits;
}
