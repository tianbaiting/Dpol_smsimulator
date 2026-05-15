// tests/unit/test_NebulaJointReco_combined.cc
//
// NebulaJointReco unit tests focusing on NPL-side correctness + boundary
// conditions (null / empty inputs). Cross-detector merge is exercised by
// integration tests; constructing synthetic TArtNEBULAPla here is impractical
// because it uses anaroot's multi-stage calibration setters.

#include <gtest/gtest.h>
#include <TClonesArray.h>

#include "NebulaJointReco.hh"
#include "TArtNEBULAPlusPla.hh"
#include "GeometryManager.hh"
#include "RecoEvent.hh"

namespace {

TEST(NebulaJointRecoCombined, HandlesNullNebHitsGracefully) {
    TClonesArray npl("TArtNEBULAPlusPla", 1);
    auto* hp = new (npl[0]) TArtNEBULAPlusPla();
    hp->fID = 101; hp->fLayer = 1; hp->fSubLayer = 1;
    hp->fPos.SetXYZ(0, 0, 8089);
    hp->fTime = 60.0; hp->fEdep = 8.0;
    hp->fQU = 8.0; hp->fQD = 8.0; hp->fTU = 60.0; hp->fTD = 60.0;
    hp->fIsVeto = false;

    GeometryManager geo;
    NebulaJointReco reco(geo);
    reco.SetTargetPosition({0, 0, 0});
    reco.SetInputs(nullptr, &npl);                    // null NEBULA, valid NPL
    auto cands = reco.ReconstructNeutrons();
    EXPECT_GE(cands.size(), 1u)
        << "Joint reco on null NEBULA + valid NPL should still produce neutrons from NPL";
}

TEST(NebulaJointRecoCombined, HandlesEmptyArraysGracefully) {
    TClonesArray neb("TArtNEBULAPla", 0);
    TClonesArray npl("TArtNEBULAPlusPla", 0);
    GeometryManager geo;
    NebulaJointReco reco(geo);
    reco.SetInputs(&neb, &npl);
    std::vector<RecoNeutron> cands;
    EXPECT_NO_THROW({ cands = reco.ReconstructNeutrons(); });
    EXPECT_EQ(0u, cands.size());
}

TEST(NebulaJointRecoCombined, HandlesBothNullGracefully) {
    GeometryManager geo;
    NebulaJointReco reco(geo);
    reco.SetInputs(nullptr, nullptr);
    std::vector<RecoNeutron> cands;
    EXPECT_NO_THROW({ cands = reco.ReconstructNeutrons(); });
    EXPECT_EQ(0u, cands.size());
}

}  // namespace
