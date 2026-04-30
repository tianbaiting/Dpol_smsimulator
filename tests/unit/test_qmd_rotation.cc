#include <gtest/gtest.h>
#include "qmd_rotation.hh"
#include <cmath>

constexpr double kTwoPi = 2.0 * M_PI;
constexpr double kPi    = M_PI;

TEST(QMDRotation, PhiNpFromComponentsIdentity) {
    EXPECT_NEAR(qmd::phi_np(1.0, 0.0), 0.0, 1e-12);
    EXPECT_NEAR(qmd::phi_np(0.0, 1.0), kPi / 2.0, 1e-12);
    EXPECT_NEAR(qmd::phi_np(-1.0, 0.0), kPi, 1e-12);
}

TEST(QMDRotation, ApplyXYRotationZeroIsIdentity) {
    double x = 100.0, y = -50.0;
    qmd::rotate_xy(x, y, 0.0);
    EXPECT_NEAR(x, 100.0, 1e-9);
    EXPECT_NEAR(y, -50.0, 1e-9);
}

TEST(QMDRotation, ApplyXYRotationHalfPi) {
    // [EN] (1,0) rotated by +π/2 => (0,1).
    double x = 1.0, y = 0.0;
    qmd::rotate_xy(x, y, kPi / 2.0);
    EXPECT_NEAR(x, 0.0, 1e-9);
    EXPECT_NEAR(y, 1.0, 1e-9);
}

TEST(QMDRotation, WrapTwoPiInRange) {
    EXPECT_NEAR(qmd::wrap_two_pi(0.0), 0.0, 1e-12);
    EXPECT_NEAR(qmd::wrap_two_pi(kTwoPi), 0.0, 1e-12);
    EXPECT_NEAR(qmd::wrap_two_pi(-0.1), kTwoPi - 0.1, 1e-9);
    EXPECT_NEAR(qmd::wrap_two_pi(3.0 * kPi), kPi, 1e-9);
}
