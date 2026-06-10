#include <gtest/gtest.h>

extern "C" {
#include "optimize.h"
#include "xalloc.h"
}

TEST(OptimizerTest, NullBodyReturnsNull) {
    EXPECT_EQ(optimize_function(nullptr, opt_flags_default()), nullptr);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}
