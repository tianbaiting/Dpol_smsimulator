// RED-phase unit test for NEBULAPlusSimParameterReader.
// The reader class does not exist yet; this test MUST fail to compile
// until P1.5 implements the reader (GREEN phase).
//
// API mirrors NEBULASimParameterReader:
//   NEBULAPlusSimParameterReader()   -- default ctor, auto-registers with SimDataManager
//   ReadNEBULAPlusParameters(const char*)          -- overall geometry CSV
//   ReadNEBULAPlusDetectorParameters(const char*)  -- bar table CSV

#include <gtest/gtest.h>
#include <algorithm>

#include "NEBULAPlusSimParameterReader.hh"   // does not exist yet => compile error
#include "TNEBULAPlusSimParameter.hh"
#include "SimDataManager.hh"

namespace {

const char* kParamCsv = SMSIM_NEBULA_PLUS_PARAM_CSV;
const char* kBarsCsv  = SMSIM_NEBULA_PLUS_BARS_CSV;

// Helper: retrieve the auto-registered parameter object.
TNEBULAPlusSimParameter* GetParam() {
    return static_cast<TNEBULAPlusSimParameter*>(
        SimDataManager::GetSimDataManager()->FindParameter("NEBULAPlusParameter"));
}

// ---------------------------------------------------------------
// Test: overall geometry parameters are read correctly
// ---------------------------------------------------------------
TEST(NEBULAPlusSimParameterReaderTest, ReadsOverallParameters) {
    NEBULAPlusSimParameterReader reader;
    reader.ReadNEBULAPlusParameters(kParamCsv);

    TNEBULAPlusSimParameter* p = GetParam();
    ASSERT_NE(nullptr, p) << "NEBULAPlusParameter not registered in SimDataManager";

    EXPECT_DOUBLE_EQ(8089.0, p->fPosition.z());
    EXPECT_DOUBLE_EQ(120.0,  p->fNeutSize.x());
    EXPECT_DOUBLE_EQ(1800.0, p->fNeutSize.y());
    EXPECT_DOUBLE_EQ(120.0,  p->fNeutSize.z());
    EXPECT_DOUBLE_EQ(320.0,  p->fVetoSize.x());
    EXPECT_DOUBLE_EQ(10.0,   p->fVetoSize.z());
    EXPECT_DOUBLE_EQ(0.240,  p->fTimeReso);
}

// ---------------------------------------------------------------
// Test: bar table gives the right Neut / Veto counts
// ---------------------------------------------------------------
TEST(NEBULAPlusSimParameterReaderTest, ReadsBarTable) {
    NEBULAPlusSimParameterReader reader;
    reader.ReadNEBULAPlusParameters(kParamCsv);
    reader.ReadNEBULAPlusDetectorParameters(kBarsCsv);

    TNEBULAPlusSimParameter* p = GetParam();
    ASSERT_NE(nullptr, p);

    int n_neut = 0, n_veto = 0;
    for (const auto& kv : p->fNEBULAPlusDetectorParameterMap) {
        if (kv.second.fDetectorType == "Neut") ++n_neut;
        if (kv.second.fDetectorType == "Veto") ++n_veto;
    }
    EXPECT_EQ(90, n_neut);
    EXPECT_EQ(24, n_veto);
}

// ---------------------------------------------------------------
// Test: Wall-A sub-layer 1 has 22 Neut bars, spanning -1320 to +1200
// ---------------------------------------------------------------
TEST(NEBULAPlusSimParameterReaderTest, WallASubXLayoutIsAsymmetric) {
    NEBULAPlusSimParameterReader reader;
    reader.ReadNEBULAPlusParameters(kParamCsv);
    reader.ReadNEBULAPlusDetectorParameters(kBarsCsv);

    TNEBULAPlusSimParameter* p = GetParam();
    ASSERT_NE(nullptr, p);

    double xmin = 1e9, xmax = -1e9;
    int count = 0;
    for (const auto& kv : p->fNEBULAPlusDetectorParameterMap) {
        const auto& d = kv.second;
        if (d.fLayer == 1 && d.fSubLayer == 1 && d.fDetectorType == "Neut") {
            xmin = std::min(xmin, d.fPosition.x());
            xmax = std::max(xmax, d.fPosition.x());
            ++count;
        }
    }
    EXPECT_EQ(22, count);
    EXPECT_DOUBLE_EQ(-1320.0, xmin);
    EXPECT_DOUBLE_EQ(+1200.0, xmax);
}

}  // namespace
