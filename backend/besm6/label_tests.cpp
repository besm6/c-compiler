#include <gtest/gtest.h>

#include <string>

#include "besm.h"

TEST(FreshLabelTest, UniqueSequentialLabels)
{
    char a[32], b[32], c[32];
    mad_fresh_label(a, sizeof(a), "L");
    mad_fresh_label(b, sizeof(b), "L");
    mad_fresh_label(c, sizeof(c), "L");

    // Each label must be unique.
    EXPECT_STRNE(a, b);
    EXPECT_STRNE(b, c);
    EXPECT_STRNE(a, c);

    // Labels for different prefixes must differ.
    char d[32], e[32];
    mad_fresh_label(d, sizeof(d), "cmp");
    mad_fresh_label(e, sizeof(e), "loop");
    EXPECT_STRNE(d, e);
    // Prefix must appear in the generated name.
    EXPECT_NE(std::string(d).find("cmp"), std::string::npos);
    EXPECT_NE(std::string(e).find("loop"), std::string::npos);
}
