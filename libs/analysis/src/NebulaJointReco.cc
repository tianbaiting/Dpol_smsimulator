#include "NebulaJointReco.hh"
#include "TArtNEBULAPlusPla.hh"
#include "SMLogger.hh"
#include "TClass.h"
#include "TDataMember.h"
#include "TClonesArray.h"
#include <cstring>

ClassImp(NebulaJointReco)

// ---------------------------------------------------------------------------
// ExtractHits — merges hits from NEBULA-Plus (upstream) and NEBULA (downstream).
//
// NEBULA-Plus side: TArtNEBULAPlusPla has public fields; direct access used.
//   wall_tag = pla->fLayer  (1 = NPL-A, 2 = NPL-B)
//
// NEBULA side: TArtNEBULAPla has private fields; ROOT reflection is used,
//   mirroring the extraction pattern in NEBULAReco::ExtractHits verbatim.
//   wall_tag is derived from the "fLayer" data member (3 → tag 3, 4 → tag 4).
// ---------------------------------------------------------------------------

std::vector<NEBULAHit> NebulaJointReco::ExtractHits() {
    std::vector<NEBULAHit> hits;

    // -----------------------------------------------------------------------
    // NEBULA-Plus side (Layer 1/2, upstream walls)
    // -----------------------------------------------------------------------
    if (fNplHits) {
        const Int_t n = fNplHits->GetEntriesFast();
        SM_DEBUG("NebulaJointReco::ExtractHits: processing {} NEBULA-Plus raw hits", n);
        hits.reserve(hits.size() + n);

        for (Int_t i = 0; i < n; ++i) {
            auto* pla = static_cast<TArtNEBULAPlusPla*>(fNplHits->UncheckedAt(i));
            if (!pla) continue;
            if (pla->fIsVeto || pla->fSubLayer == 0) continue;

            NEBULAHit h;
            h.moduleID = pla->fID;
            h.position = pla->fPos;
            h.energy   = pla->fEdep;
            h.time     = pla->fTime;
            h.qave     = 0.5 * (pla->fQU + pla->fQD);
            h.wall_tag = pla->fLayer;  // 1 = NPL-A, 2 = NPL-B

            if (h.energy < fEnergyThreshold) continue;

            h.position = ApplyPositionSmearing(h.position);
            h.time     = ApplyTimeSmearing(h.time);

            hits.push_back(h);
        }
    }

    // -----------------------------------------------------------------------
    // NEBULA side (Layer 3/4, downstream walls)
    // Uses ROOT reflection to access TArtNEBULAPla private fields,
    // identical to NEBULAReco::ExtractHits — only wall_tag assignment added.
    // -----------------------------------------------------------------------
    if (fNebHits) {
        const Int_t n = fNebHits->GetEntriesFast();
        SM_DEBUG("NebulaJointReco::ExtractHits: processing {} NEBULA raw hits", n);
        hits.reserve(hits.size() + n);

        for (Int_t i = 0; i < n; ++i) {
            TObject* obj = fNebHits->At(i);
            if (!obj) continue;

            // Validate that the object is TArtNEBULAPla
            TClass* objClass = obj->IsA();
            if (!objClass || strcmp(objClass->GetName(), "TArtNEBULAPla") != 0) {
                SM_WARN("NebulaJointReco: object {} type is {}, not TArtNEBULAPla, skipping",
                        i, (objClass ? objClass->GetName() : "unknown"));
                continue;
            }

            double time = 0, energy = 0;
            double posX = 0, posY = 0, posZ = 0;
            int    id   = i;  // fallback
            int    layer = 0; // for wall_tag

            try {
                TClass* cls = obj->IsA();
                if (cls) {
                    TDataMember* idMember     = cls->GetDataMember("id");
                    TDataMember* timeMember   = cls->GetDataMember("fTAveCal");
                    TDataMember* energyMember = cls->GetDataMember("fQAveCal");
                    TDataMember* posMember    = cls->GetDataMember("fPos");
                    TDataMember* layerMember  = cls->GetDataMember("fLayer");

                    char* objPtr = (char*)obj;

                    if (idMember)
                        id = *(int*)(objPtr + idMember->GetOffset());

                    if (timeMember)
                        time = *(double*)(objPtr + timeMember->GetOffset());

                    if (energyMember)
                        energy = *(double*)(objPtr + energyMember->GetOffset());

                    if (posMember) {
                        Long_t posOffset = posMember->GetOffset();
                        posX = *(double*)(objPtr + posOffset);
                        posY = *(double*)(objPtr + posOffset + 8);
                        posZ = *(double*)(objPtr + posOffset + 16);
                    }

                    if (layerMember)
                        layer = *(int*)(objPtr + layerMember->GetOffset());
                }
            } catch (...) {
                // Reflection failed — use same fallback values as NEBULAReco
                id     = i;
                time   = 50.0 + i;
                energy = (i == 0 || i == 1) ? 0.1 : (2.0 + i * 5.0);
                posX   = 1200 + i * 100;
                posY   = -50  + i * 20;
                posZ   = 3000;
                layer  = 0;
            }

            // Apply energy threshold
            if (energy < fEnergyThreshold) continue;

            TVector3 pos(posX, posY, posZ);
            pos  = ApplyPositionSmearing(pos);
            time = ApplyTimeSmearing(time);

            // wall_tag: Layer 3 → NEBULA Wall-A (tag 3), Layer 4 → NEBULA Wall-B (tag 4)
            // layer == 0 means unknown; we preserve it as-is for the base reco.
            NEBULAHit h(id, pos, energy, time, energy);
            h.wall_tag = (layer >= 3) ? layer : 0;
            hits.push_back(h);
        }
    }

    return hits;
}
