//
// Copyright (c) 2025 Serge Vakulenko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

extern "C" {
#include "xalloc.h"
}

// Fixture: each test starts and ends with an empty allocation list.
class XAllocTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        xfree_all();
        ASSERT_EQ(xtotal_allocated_size(), 0u);
    }

    void TearDown() override { xfree_all(); }
};

// ---------------------------------------------------------------------------
// xalloc()
// ---------------------------------------------------------------------------

TEST_F(XAllocTest, AllocReturnsNonNull)
{
    void *p = xalloc(64, __func__, __FILE__, __LINE__);
    EXPECT_NE(p, nullptr);
    xfree(p);
}

TEST_F(XAllocTest, AllocZeroInitialized)
{
    const size_t n    = 64;
    void *p           = xalloc(n, __func__, __FILE__, __LINE__);
    const auto *bytes = static_cast<const uint8_t *>(p);
    bool all_zero     = true;
    for (size_t i = 0; i < n; i++) {
        if (bytes[i] != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_TRUE(all_zero);
    xfree(p);
}

TEST_F(XAllocTest, AllocTracksSize)
{
    void *p = xalloc(100, __func__, __FILE__, __LINE__);
    EXPECT_EQ(xtotal_allocated_size(), 100u);
    xfree(p);
}

TEST_F(XAllocTest, AllocSingleByte)
{
    void *p = xalloc(1, __func__, __FILE__, __LINE__);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(xtotal_allocated_size(), 1u);
    xfree(p);
}

TEST_F(XAllocTest, AllocLargeBlock)
{
    void *p = xalloc(1024, __func__, __FILE__, __LINE__);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(xtotal_allocated_size(), 1024u);
    xfree(p);
}

TEST_F(XAllocTest, AllocMultipleTracked)
{
    void *a = xalloc(10, __func__, __FILE__, __LINE__);
    void *b = xalloc(20, __func__, __FILE__, __LINE__);
    void *c = xalloc(30, __func__, __FILE__, __LINE__);
    EXPECT_EQ(xtotal_allocated_size(), 60u);
    xfree(a);
    xfree(b);
    xfree(c);
}

TEST_F(XAllocTest, AllocIndependentPointers)
{
    void *a = xalloc(32, __func__, __FILE__, __LINE__);
    void *b = xalloc(32, __func__, __FILE__, __LINE__);
    EXPECT_NE(a, b);
    // The blocks must not overlap: the later allocation (b) is inserted at the
    // head, so b < a in address space (or they are separate regardless).
    auto *pa        = static_cast<char *>(a);
    auto *pb        = static_cast<char *>(b);
    bool no_overlap = (pb + 32 <= pa) || (pa + 32 <= pb);
    EXPECT_TRUE(no_overlap);
    xfree(a);
    xfree(b);
}

// ---------------------------------------------------------------------------
// xfree()
// ---------------------------------------------------------------------------

TEST_F(XAllocTest, FreeNullNoOp)
{
    xfree(nullptr); // must not crash
    EXPECT_EQ(xtotal_allocated_size(), 0u);
}

TEST_F(XAllocTest, FreeDecreasesSize)
{
    void *p = xalloc(50, __func__, __FILE__, __LINE__);
    EXPECT_EQ(xtotal_allocated_size(), 50u);
    xfree(p);
    EXPECT_EQ(xtotal_allocated_size(), 0u);
}

TEST_F(XAllocTest, FreeRestoresZero)
{
    void *p = xalloc(77, __func__, __FILE__, __LINE__);
    xfree(p);
    EXPECT_EQ(xtotal_allocated_size(), 0u);
}

TEST_F(XAllocTest, FreeHeadBlock)
{
    // xalloc inserts at the head, so the last allocation becomes the head.
    void *a = xalloc(10, __func__, __FILE__, __LINE__);
    void *b = xalloc(20, __func__, __FILE__, __LINE__);
    void *c = xalloc(30, __func__, __FILE__, __LINE__); // head
    xfree(c);
    EXPECT_EQ(xtotal_allocated_size(), 30u); // a(10) + b(20)
    xfree(a);
    xfree(b);
}

TEST_F(XAllocTest, FreeMiddleBlock)
{
    void *a = xalloc(10, __func__, __FILE__, __LINE__);
    void *b = xalloc(20, __func__, __FILE__, __LINE__); // middle
    void *c = xalloc(30, __func__, __FILE__, __LINE__);
    xfree(b);
    EXPECT_EQ(xtotal_allocated_size(), 40u); // a(10) + c(30)
    xfree(a);
    xfree(c);
}

TEST_F(XAllocTest, FreeTailBlock)
{
    void *a = xalloc(10, __func__, __FILE__, __LINE__); // tail (oldest)
    void *b = xalloc(20, __func__, __FILE__, __LINE__);
    void *c = xalloc(30, __func__, __FILE__, __LINE__);
    xfree(a);
    EXPECT_EQ(xtotal_allocated_size(), 50u); // b(20) + c(30)
    xfree(b);
    xfree(c);
}

// ---------------------------------------------------------------------------
// xfree_all()
// ---------------------------------------------------------------------------

TEST_F(XAllocTest, FreeAllEmpty)
{
    xfree_all(); // must not crash on empty list
    EXPECT_EQ(xtotal_allocated_size(), 0u);
}

TEST_F(XAllocTest, FreeAllResetsSize)
{
    xalloc(10, __func__, __FILE__, __LINE__);
    xalloc(20, __func__, __FILE__, __LINE__);
    xalloc(30, __func__, __FILE__, __LINE__);
    EXPECT_EQ(xtotal_allocated_size(), 60u);
    xfree_all();
    EXPECT_EQ(xtotal_allocated_size(), 0u);
}

TEST_F(XAllocTest, FreeAllThenReallocate)
{
    xalloc(10, __func__, __FILE__, __LINE__);
    xfree_all();
    void *p = xalloc(42, __func__, __FILE__, __LINE__);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(xtotal_allocated_size(), 42u);
    xfree(p);
}

// ---------------------------------------------------------------------------
// xstrdup()
// ---------------------------------------------------------------------------

TEST_F(XAllocTest, StrdupNullReturnsNull)
{
    char *p = xstrdup(nullptr);
    EXPECT_EQ(p, nullptr);
    EXPECT_EQ(xtotal_allocated_size(), 0u);
}

TEST_F(XAllocTest, StrdupEmptyString)
{
    char *p = xstrdup("");
    ASSERT_NE(p, nullptr);
    EXPECT_STREQ(p, "");
    xfree(p);
}

TEST_F(XAllocTest, StrdupCopiesContent)
{
    char *p = xstrdup("hello");
    ASSERT_NE(p, nullptr);
    EXPECT_STREQ(p, "hello");
    xfree(p);
}

TEST_F(XAllocTest, StrdupIndependentCopy)
{
    char original[] = "world";
    char *copy      = xstrdup(original);
    ASSERT_NE(copy, nullptr);
    original[0] = 'X';           // mutate original
    EXPECT_EQ(original[0], 'X'); // confirm original changed
    EXPECT_STREQ(copy, "world"); // copy must be unaffected
    xfree(copy);
}

TEST_F(XAllocTest, StrdupIsTracked)
{
    const char *str = "hello";
    size_t expected = strlen(str) + 1;
    char *p         = xstrdup(str);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(xtotal_allocated_size(), expected);
    xfree(p);
}

TEST_F(XAllocTest, StrdupCanBeFreed)
{
    char *p = xstrdup("test");
    xfree(p);
    EXPECT_EQ(xtotal_allocated_size(), 0u);
}

// ---------------------------------------------------------------------------
// xstruniq()
// ---------------------------------------------------------------------------

TEST_F(XAllocTest, StruniqBasicFormat)
{
    int n   = 0;
    char *s = xstruniq("t.", &n);
    ASSERT_NE(s, nullptr);
    EXPECT_STREQ(s, "t.0");
    EXPECT_EQ(n, 1);
    xfree(s);
}

TEST_F(XAllocTest, StruniqIncrements)
{
    int n    = 0;
    char *s0 = xstruniq("t.", &n);
    char *s1 = xstruniq("t.", &n);
    char *s2 = xstruniq("t.", &n);
    EXPECT_STREQ(s0, "t.0");
    EXPECT_STREQ(s1, "t.1");
    EXPECT_STREQ(s2, "t.2");
    EXPECT_EQ(n, 3);
    xfree(s0);
    xfree(s1);
    xfree(s2);
}

TEST_F(XAllocTest, StruniqIsTracked)
{
    int n   = 0;
    char *s = xstruniq("t.", &n);
    ASSERT_NE(s, nullptr);
    EXPECT_GT(xtotal_allocated_size(), 0u);
    xfree(s);
}

TEST_F(XAllocTest, StruniqCanBeFreed)
{
    int n   = 0;
    char *s = xstruniq("x", &n);
    xfree(s);
    EXPECT_EQ(xtotal_allocated_size(), 0u);
}

TEST_F(XAllocTest, StruniqEmptyPrefix)
{
    int n    = 0;
    char *s0 = xstruniq("", &n);
    char *s1 = xstruniq("", &n);
    char *s2 = xstruniq("", &n);
    EXPECT_STREQ(s0, "0");
    EXPECT_STREQ(s1, "1");
    EXPECT_STREQ(s2, "2");
    xfree(s0);
    xfree(s1);
    xfree(s2);
}

// ---------------------------------------------------------------------------
// xtotal_allocated_size() and xreport_lost_memory()
// ---------------------------------------------------------------------------

TEST_F(XAllocTest, TotalSizeInitiallyZero)
{
    EXPECT_EQ(xtotal_allocated_size(), 0u);
}

TEST_F(XAllocTest, ReportNoLeaks)
{
    testing::internal::CaptureStdout();
    xreport_lost_memory();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.empty());
}

TEST_F(XAllocTest, ReportWithLeaks)
{
    // Intentionally leave allocation outstanding so report fires.
    xalloc(123, "test_func", "test_file.c", 42);
    testing::internal::CaptureStdout();
    xreport_lost_memory();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Lost memory:"), std::string::npos);
    EXPECT_NE(output.find("123"), std::string::npos);
    EXPECT_NE(output.find("test_func"), std::string::npos);
    // TearDown calls xfree_all() to clean up the outstanding block.
}
