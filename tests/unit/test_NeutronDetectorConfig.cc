#include <gtest/gtest.h>

#include "NeutronDetectorConfig.hh"

namespace {

using analysis::neutron::DetectorAvailability;
using analysis::neutron::NeutronDetectorMode;
using analysis::neutron::ParseNeutronDetectorMode;
using analysis::neutron::ResolveNeutronDetectorMode;

TEST(NeutronDetectorConfig, ParsesCanonicalModes) {
    EXPECT_EQ(NeutronDetectorMode::kAuto, ParseNeutronDetectorMode("auto"));
    EXPECT_EQ(NeutronDetectorMode::kNone, ParseNeutronDetectorMode("none"));
    EXPECT_EQ(NeutronDetectorMode::kNebula, ParseNeutronDetectorMode("nebula"));
    EXPECT_EQ(NeutronDetectorMode::kNebulaPlus, ParseNeutronDetectorMode("nebula-plus"));
    EXPECT_EQ(NeutronDetectorMode::kJoint, ParseNeutronDetectorMode("joint"));
}

TEST(NeutronDetectorConfig, AutoPrefersParameterAvailabilityOverBranches) {
    DetectorAvailability availability;
    availability.has_nebula_branch = true;
    availability.has_nebula_plus_branch = true;
    availability.nebula_parameter_loaded = true;
    availability.nebula_detector_count = 120;
    availability.nebula_plus_parameter_loaded = false;
    availability.nebula_plus_detector_count = 0;

    const auto resolved = ResolveNeutronDetectorMode(NeutronDetectorMode::kAuto, availability);

    EXPECT_EQ(NeutronDetectorMode::kNebula, resolved.effective_mode);
    EXPECT_TRUE(resolved.used_metadata);
    EXPECT_TRUE(resolved.ignored_nebula_plus_branch);
}

TEST(NeutronDetectorConfig, AutoFallsBackToBranchesForLegacyFiles) {
    DetectorAvailability availability;
    availability.has_nebula_branch = true;
    availability.has_nebula_plus_branch = false;
    availability.metadata_available = false;

    const auto resolved = ResolveNeutronDetectorMode(NeutronDetectorMode::kAuto, availability);

    EXPECT_EQ(NeutronDetectorMode::kNebula, resolved.effective_mode);
    EXPECT_FALSE(resolved.used_metadata);
}

TEST(NeutronDetectorConfig, ExplicitJointRequiresBothDetectors) {
    DetectorAvailability availability;
    availability.metadata_available = true;
    availability.nebula_parameter_loaded = true;
    availability.nebula_detector_count = 120;
    availability.nebula_plus_parameter_loaded = false;
    availability.nebula_plus_detector_count = 0;

    EXPECT_THROW(
        ResolveNeutronDetectorMode(NeutronDetectorMode::kJoint, availability),
        std::runtime_error
    );
}

}  // namespace
