#include <gtest/gtest.h>

#include "NeutronDetectorSimConfig.hh"
#include "TNEBULASimParameter.hh"
#include "TNEBULAPlusSimParameter.hh"

namespace {

TEST(NeutronDetectorSimConfig, ParameterMustBeLoadedAndHaveDetectors) {
    TNEBULASimParameter nebula;
    nebula.fIsLoaded = true;
    nebula.fNeutNum = 120;
    nebula.fVetoNum = 24;

    TNEBULAPlusSimParameter nebula_plus;
    nebula_plus.fIsLoaded = true;
    nebula_plus.fNeutNum = 0;
    nebula_plus.fVetoNum = 0;

    EXPECT_TRUE(sim_deuteron::IsNEBULAEnabled(&nebula));
    EXPECT_FALSE(sim_deuteron::IsNEBULAPlusEnabled(&nebula_plus));
}

TEST(NeutronDetectorSimConfig, NullParameterIsDisabled) {
    EXPECT_FALSE(sim_deuteron::IsNEBULAEnabled(nullptr));
    EXPECT_FALSE(sim_deuteron::IsNEBULAPlusEnabled(nullptr));
}

}  // namespace
