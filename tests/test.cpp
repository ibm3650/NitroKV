#include <gtest/gtest.h>

#include "nitrokv/common/version.hpp"

TEST(SmokeTest, VersionIsDefined) {
    EXPECT_GE(nitrokv::common::version_major, 0);
    EXPECT_GE(nitrokv::common::version_minor, 0);
    EXPECT_GE(nitrokv::common::version_patch, 0);
}



