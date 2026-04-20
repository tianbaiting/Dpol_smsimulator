// Default-value regression test for DeutDetectorConstruction.
// Guards against anyone flipping fSetTarget back to true.

#include <gtest/gtest.h>
#include "DeutDetectorConstruction.hh"

TEST(DeutDetectorConstructionDefaults, TargetDisabledByDefault) {
    DeutDetectorConstruction dc;
    EXPECT_FALSE(dc.IsTargetEnabled())
        << "Target must default to OFF. Tree-gun inputs already contain "
           "post-target secondaries; enabling the target by default causes "
           "silent double-scattering. See "
           "docs/superpowers/specs/2026-04-20-target-config-audit-design.md.";
}
