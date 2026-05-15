// tests/unit/test_NEBULAPlusReco_basic.cc
//
// Basic NEBULAPlusReco unit tests using hand-crafted TArtNEBULAPlusPla input.
// Covers: (a) two close-in-time hits cluster into one neutron,
// (b) below-threshold hits are dropped,
// (c) veto bars are skipped.

#include <gtest/gtest.h>
#include <TClonesArray.h>

#include "NEBULAPlusReco.hh"
#include "TArtNEBULAPlusPla.hh"
#include "GeometryManager.hh"

namespace {

TEST(NEBULAPlusRecoBasic, ClustersTwoCoincidentHitsIntoOne) {
    TClonesArray arr("TArtNEBULAPlusPla", 4);
    // First hit in Wall-A Sub1 (Layer 1 SubL 1), Z=8089
    auto* h0 = new (arr[0]) TArtNEBULAPlusPla();
    h0->fID = 101; h0->fLayer = 1; h0->fSubLayer = 1;
    h0->fPos.SetXYZ(0.0, 0.0, 8089.0);
    h0->fTime = 60.0; h0->fEdep = 8.0;
    h0->fQU = 8.0; h0->fQD = 8.0; h0->fTU = 60.0; h0->fTD = 60.0;
    h0->fIsVeto = false;
    // Second hit in Wall-A Sub2 (Layer 1 SubL 2), Z=8219, very close in time
    auto* h1 = new (arr[1]) TArtNEBULAPlusPla();
    h1->fID = 201; h1->fLayer = 1; h1->fSubLayer = 2;
    h1->fPos.SetXYZ(30.0, 0.0, 8219.0);
    h1->fTime = 60.5; h1->fEdep = 6.0;
    h1->fQU = 6.0; h1->fQD = 6.0; h1->fTU = 60.5; h1->fTD = 60.5;
    h1->fIsVeto = false;

    GeometryManager geo;
    NEBULAPlusReco reco(geo);
    reco.SetTargetPosition({0, 0, 0});
    reco.SetInput(&arr);

    auto cands = reco.ReconstructNeutrons();
    // Two close hits in adjacent sub-layers should merge into one neutron
    // (time window default = 10 ns; dt = 0.5 ns << 10 ns).
    EXPECT_EQ(1u, cands.size())
        << "two close hits in adjacent sub-layers should merge into one neutron";
}

TEST(NEBULAPlusRecoBasic, RejectsBelowEnergyThreshold) {
    TClonesArray arr("TArtNEBULAPlusPla", 1);
    auto* h = new (arr[0]) TArtNEBULAPlusPla();
    h->fID = 101; h->fLayer = 1; h->fSubLayer = 1;
    h->fPos.SetXYZ(0, 0, 8089);
    h->fTime = 60.0; h->fEdep = 0.5;   // below default threshold (1.0 MeV)
    h->fQU = 0.5; h->fQD = 0.5; h->fTU = 60.0; h->fTD = 60.0;
    h->fIsVeto = false;

    GeometryManager geo;
    NEBULAPlusReco reco(geo);
    reco.SetTargetPosition({0, 0, 0});
    reco.SetInput(&arr);
    reco.SetEnergyThreshold(1.0);    // explicit, documents the threshold

    auto cands = reco.ReconstructNeutrons();
    EXPECT_EQ(0u, cands.size());
}

TEST(NEBULAPlusRecoBasic, SkipsVetoBars) {
    TClonesArray arr("TArtNEBULAPlusPla", 1);
    auto* h = new (arr[0]) TArtNEBULAPlusPla();
    h->fID = 501; h->fLayer = 1; h->fSubLayer = 0;   // SubLayer 0 = Veto
    h->fPos.SetXYZ(0, 0, 7989);
    h->fTime = 50.0; h->fEdep = 10.0;                // would pass threshold
    h->fIsVeto = true;

    GeometryManager geo;
    NEBULAPlusReco reco(geo);
    reco.SetInput(&arr);

    auto cands = reco.ReconstructNeutrons();
    EXPECT_EQ(0u, cands.size()) << "veto bars should not produce neutrons";
}

}  // namespace
