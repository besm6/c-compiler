//
// Tests for the shared C escape / string-literal decoder.
//
#include <gtest/gtest.h>
#include <string.h>

#include <string>

#include "c_escape.h"
#include "xalloc.h"

using Escape = ::testing::Test;

// Decode a lexeme and return its bytes, so a NUL in the middle is preserved.
static std::string decode(const char *raw)
{
    size_t len  = 0;
    char *bytes = c_decode_string_literal(raw, &len);
    std::string result(bytes ? bytes : "", len);
    xfree(bytes);
    return result;
}

TEST_F(Escape, PlainString)
{
    EXPECT_EQ(decode("\"hello\""), std::string("hello"));
    EXPECT_EQ(decode("\"\""), std::string(""));
}

TEST_F(Escape, SimpleEscapes)
{
    EXPECT_EQ(decode("\"a\\nb\\tc\""), std::string("a\nb\tc"));
    EXPECT_EQ(decode("\"\\\\\\\"\\'\\?\""), std::string("\\\"'?"));
    EXPECT_EQ(decode("\"\\a\\b\\f\\r\\v\""), std::string("\a\b\f\r\v"));
}

// The bug this decoder was rewritten for: a decoded NUL is a byte of the string, not
// its end, so "a\0c" is three bytes long — it used to arrive as the single byte 'a'.
TEST_F(Escape, EmbeddedNul)
{
    std::string s = decode("\"a\\0c\"");
    ASSERT_EQ(s.size(), 3u);
    EXPECT_EQ(s, std::string("a\0c", 3));

    s = decode("\"a\\000c\"");
    ASSERT_EQ(s.size(), 3u);
    EXPECT_EQ(s, std::string("a\0c", 3));

    s = decode("\"\\0\\0\\0\"");
    EXPECT_EQ(s, std::string("\0\0\0", 3));
}

// Octal and hex escapes: the old string decoder knew neither, and copied their
// characters through literally ("\x41" became the three bytes 'x', '4', '1').
TEST_F(Escape, OctalAndHexEscapes)
{
    EXPECT_EQ(decode("\"\\x41\\101\""), std::string("AA"));
    EXPECT_EQ(decode("\"\\1\\12\\123\""), std::string("\1\12\123"));
    EXPECT_EQ(decode("\"\\377\""), std::string("\377"));
    // Octal takes at most three digits, so the '4' here is an ordinary character.
    EXPECT_EQ(decode("\"\\1014\""), std::string("A4"));
}

TEST_F(Escape, EscapeValueAdvancesPastTheWholeEscape)
{
    const char *s = "\\101rest";
    EXPECT_EQ(c_escape_value(&s), 'A');
    EXPECT_STREQ(s, "rest");

    s = "\\n!";
    EXPECT_EQ(c_escape_value(&s), '\n');
    EXPECT_STREQ(s, "!");
}
