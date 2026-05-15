// tests/analysis/test_NEBULAReco_legacy_compat.cc
//
// After the Phase 2 refactor, NEBULAReco (the new class derived from
// NEBULABaseReco) must reproduce the legacy NEBULAReconstructor's
// outputs bit-for-bit on the same input.
//
// This test is initially RED (NEBULAReco doesn't exist yet); it goes
// GREEN in Task P2.5 when NEBULAReco lands.

#include <gtest/gtest.h>
#include <TFile.h>
#include <TTree.h>
#include <TClonesArray.h>
#include <vector>

#include "NEBULAReco.hh"           // <- the new class, doesn't exist yet
#include "GeometryManager.hh"
#include "RecoEvent.hh"            // RecoNeutron lives here

namespace {

const char* kInputPath  = "tests/fixtures/nebula_reco_input.root";
const char* kGoldenPath = "tests/fixtures/nebula_reco_golden.root";

TEST(NEBULARecoLegacyCompat, MatchesGoldenFixture) {
    TFile* fgold = TFile::Open(kGoldenPath);
    ASSERT_NE(nullptr, fgold) << "fixture missing: " << kGoldenPath;
    TTree* tgold = (TTree*) fgold->Get("golden");
    ASSERT_NE(nullptr, tgold);
    std::vector<RecoNeutron>* gcands = nullptr;
    tgold->SetBranchAddress("cands", &gcands);

    TFile* fin = TFile::Open(kInputPath);
    ASSERT_NE(nullptr, fin) << "fixture missing: " << kInputPath;
    TTree* tin = (TTree*) fin->Get("tree");
    if (!tin) tin = (TTree*) fin->Get("g4tree");
    ASSERT_NE(nullptr, tin) << "no tree found in input fixture";

    TClonesArray* neb = nullptr;
    tin->SetBranchAddress("NEBULAPla", &neb);

    GeometryManager geo;
    NEBULAReco reco(geo);
    reco.SetTargetPosition({0, 0, 0});
    reco.SetPositionSmearing(0.0);
    reco.SetTimeSmearing(0.0);

    Long64_t n = tin->GetEntries();
    ASSERT_EQ(n, tgold->GetEntries());
    for (Long64_t i = 0; i < n; ++i) {
        tin->GetEntry(i);
        tgold->GetEntry(i);

        auto cands = reco.ReconstructNeutrons(neb);
        ASSERT_EQ(cands.size(), gcands->size())
            << "event " << i << " candidate count mismatch (got "
            << cands.size() << ", want " << gcands->size() << ")";
        for (size_t k = 0; k < cands.size(); ++k) {
            EXPECT_NEAR(cands[k].energy,        (*gcands)[k].energy,        1e-4);
            EXPECT_NEAR(cands[k].timeOfFlight,  (*gcands)[k].timeOfFlight,  1e-6);
            EXPECT_NEAR(cands[k].beta,          (*gcands)[k].beta,          1e-6);
            EXPECT_NEAR(cands[k].position.X(),  (*gcands)[k].position.X(),  1e-3);
            EXPECT_NEAR(cands[k].position.Y(),  (*gcands)[k].position.Y(),  1e-3);
            EXPECT_NEAR(cands[k].position.Z(),  (*gcands)[k].position.Z(),  1e-3);
        }
    }
    fin->Close();
    fgold->Close();
}

}  // namespace
