#include <gtest/gtest.h>
#include "QMDInputMetadata.hh"

TEST(QMDInputMetadataIndices, FUserDoubleLayoutIsStable) {
    using namespace qmd_input_metadata;
    EXPECT_EQ(kBimpIndex,        0);
    EXPECT_EQ(kBPhiIndex,        1);
    EXPECT_EQ(kPhiNpTruthIndex,  2);
}

TEST(QMDInputMetadataIndices, FUserIntLayoutIsStable) {
    using namespace qmd_input_metadata;
    EXPECT_EQ(kSourceKindIndex,        0);
    EXPECT_EQ(kOriginalEventIdIndex,   1);
    EXPECT_EQ(kSourceFileIndex,        2);
    EXPECT_EQ(kPolarizationKindIndex,  3);
}
