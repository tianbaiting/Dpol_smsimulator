#include "NEBULAReco.hh"
#include "SMLogger.hh"
#include "TClass.h"
#include "TDataMember.h"
#include "TClonesArray.h"
#include <cstring>

ClassImp(NEBULAReco)

// ---------------------------------------------------------------------------
// ExtractHits — ported verbatim from NEBULAReco::ExtractHits().
// Reads from fNebHits (set via SetInput / ReconstructNeutrons overload).
// wall_tag is left at 0 (no Layer info available in the reflection path).
// ---------------------------------------------------------------------------

std::vector<NEBULAHit> NEBULAReco::ExtractHits() {
    std::vector<NEBULAHit> hits;

    if (!fNebHits) return hits;

    int nHits = fNebHits->GetEntriesFast();
    SM_DEBUG("ExtractHits: 处理 {} 个NEBULA原始hits", nHits);

    for (int i = 0; i < nHits; ++i) {
        TObject* obj = fNebHits->At(i);
        if (!obj) continue;

        // 检查对象是否是TArtNEBULAPla类型
        TClass* objClass = obj->IsA();
        if (!objClass || strcmp(objClass->GetName(), "TArtNEBULAPla") != 0) {
            SM_WARN("对象 {} 类型是 {}，不是TArtNEBULAPla，跳过...",
                    i, (objClass ? objClass->GetName() : "unknown"));
            continue;
        }

        double time = 0, energy = 0;
        double posX = 0, posY = 0, posZ = 0;
        int id = i; // 默认值

        if (i < 3) {
            SM_TRACE("对象 {} 的数据结构: (Dump output suppressed)", i);
        }

        // 使用 ROOT 反射机制读取字段 (与 legacy NEBULAReco 完全相同)
        try {
            TClass* cls = obj->IsA();
            if (cls) {
                TDataMember* idMember     = cls->GetDataMember("id");
                TDataMember* timeMember   = cls->GetDataMember("fTAveCal");
                TDataMember* energyMember = cls->GetDataMember("fQAveCal");
                TDataMember* posMember    = cls->GetDataMember("fPos");

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
            }
        } catch (...) {
            // 反射失败时使用与 legacy 相同的回退默认值
            id     = i;
            time   = 50.0 + i;
            energy = (i == 0 || i == 1) ? 0.1 : (2.0 + i * 5.0);
            posX   = 1200 + i * 100;
            posY   = -50  + i * 20;
            posZ   = 3000;
        }

        TVector3 pos(posX, posY, posZ);

        SM_DEBUG("Hit {}: ID={}, Pos=({},{},{}), Time={}ns, Energy={}",
                 i, id, posX, posY, posZ, time, energy);

        // 应用能量阈值
        if (energy < fEnergyThreshold) continue;

        // 应用位置和时间模糊
        pos  = ApplyPositionSmearing(pos);
        time = ApplyTimeSmearing(time);

        // wall_tag = 0: no layer information in reflection path
        NEBULAHit hit(i, pos, energy, time, energy);
        hits.push_back(hit);
    }

    return hits;
}
