#include <float.h>
#include <gtest/gtest.h>
#include <limits.h>

#include "const_convert.h"

// Test fixture for common setup
class ConstConvertTest : public ::testing::Test {
protected:
    // Helper to create constants
    Const make_const_char(int8_t v)
    {
        Const c = { CONST_CHAR, .value.char_val = v };
        return c;
    }
    Const make_const_uchar(uint8_t v)
    {
        Const c = { CONST_UCHAR, .value.uchar_val = v };
        return c;
    }
    Const make_const_int(int32_t v)
    {
        Const c = { CONST_INT, .value.int_val = v };
        return c;
    }
    Const make_const_long(int64_t v)
    {
        Const c = { CONST_LONG, .value.long_val = v };
        return c;
    }
    Const make_const_uint(uint32_t v)
    {
        Const c = { CONST_UINT, .value.uint_val = v };
        return c;
    }
    Const make_const_ulong(uint64_t v)
    {
        Const c = { CONST_ULONG, .value.ulong_val = v };
        return c;
    }
    Const make_const_double(double v)
    {
        Const c = { CONST_DOUBLE, .value.double_val = v };
        return c;
    }

    // Common test values
    Const zero_char    = make_const_char(0);
    Const max_char     = make_const_char(INT8_MAX);
    Const min_char     = make_const_char(INT8_MIN);
    Const zero_uchar   = make_const_uchar(0);
    Const max_uchar    = make_const_uchar(UINT8_MAX);
    Const zero_int     = make_const_int(0);
    Const max_int      = make_const_int(INT32_MAX);
    Const min_int      = make_const_int(INT32_MIN);
    Const zero_long    = make_const_long(0);
    Const max_long     = make_const_long(INT64_MAX);
    Const min_long     = make_const_long(INT64_MIN);
    Const zero_uint    = make_const_uint(0);
    Const max_uint     = make_const_uint(UINT32_MAX);
    Const zero_ulong   = make_const_ulong(0);
    Const max_ulong    = make_const_ulong(UINT64_MAX);
    Const zero_double  = make_const_double(0.0);
    Const large_double = make_const_double(1e18);
    Const neg_double   = make_const_double(-1e18);
};

// Macro to test conversion and check result
#define TEST_CONVERSION(src, target_type, expected_tag, expected_value, value_field) \
    do {                                                                             \
        Const result = const_convert(target_type, src);                              \
        EXPECT_EQ(expected_tag, result.tag);                                         \
        EXPECT_EQ(expected_value, result.value.value_field);                         \
    } while (0)

// Macro for double conversions
#define TEST_DOUBLE_CONVERSION(src, target_type, expected_tag, expected_value) \
    do {                                                                       \
        Const result = const_convert(target_type, src);                        \
        EXPECT_EQ(expected_tag, result.tag);                                   \
        EXPECT_DOUBLE_EQ(expected_value, result.value.double_val);             \
    } while (0)

// Test same-type conversions (no-op)
TEST_F(ConstConvertTest, SameTypeConversion)
{
    TEST_CONVERSION(zero_char, TYPE_SCHAR, CONST_CHAR, 0, char_val);
    TEST_CONVERSION(max_uchar, TYPE_UCHAR, CONST_UCHAR, UINT8_MAX, uchar_val);
    TEST_CONVERSION(min_int, TYPE_INT, CONST_INT, INT32_MIN, int_val);
    TEST_CONVERSION(max_long, TYPE_LONG, CONST_LONG, INT64_MAX, long_val);
    TEST_CONVERSION(zero_uint, TYPE_UINT, CONST_UINT, 0, uint_val);
    TEST_CONVERSION(max_ulong, TYPE_ULONG, CONST_ULONG, UINT64_MAX, ulong_val);
    TEST_DOUBLE_CONVERSION(large_double, TYPE_DOUBLE, CONST_DOUBLE, 1e18);
}

// Test conversions to SChar
TEST_F(ConstConvertTest, ConvertToSChar)
{
    TEST_CONVERSION(zero_uchar, TYPE_SCHAR, CONST_CHAR, 0, char_val);
    TEST_CONVERSION(max_uchar, TYPE_SCHAR, CONST_CHAR, -1, char_val);   // 255 -> -1
    TEST_CONVERSION(max_int, TYPE_SCHAR, CONST_CHAR, -1, char_val);     // INT32_MAX -> -1
    TEST_CONVERSION(min_long, TYPE_SCHAR, CONST_CHAR, 0, char_val);     // INT64_MIN -> 0
    TEST_CONVERSION(max_uint, TYPE_SCHAR, CONST_CHAR, -1, char_val);    // UINT32_MAX -> -1
    TEST_CONVERSION(max_ulong, TYPE_SCHAR, CONST_CHAR, -1, char_val);   // UINT64_MAX -> -1
    TEST_CONVERSION(large_double, TYPE_SCHAR, CONST_CHAR, 0, char_val); // 1e18 -> 0
}

// Test conversions to UChar
TEST_F(ConstConvertTest, ConvertToUChar)
{
    TEST_CONVERSION(min_char, TYPE_UCHAR, CONST_UCHAR, 128, uchar_val);   // -128 -> 128
    TEST_CONVERSION(max_int, TYPE_UCHAR, CONST_UCHAR, 255, uchar_val);    // INT32_MAX -> 255
    TEST_CONVERSION(min_long, TYPE_UCHAR, CONST_UCHAR, 0, uchar_val);     // INT64_MIN -> 0
    TEST_CONVERSION(max_uint, TYPE_UCHAR, CONST_UCHAR, 255, uchar_val);   // UINT32_MAX -> 255
    TEST_CONVERSION(max_ulong, TYPE_UCHAR, CONST_UCHAR, 255, uchar_val);  // UINT64_MAX -> 255
    TEST_CONVERSION(large_double, TYPE_UCHAR, CONST_UCHAR, 0, uchar_val); // 1e18 -> 0
}

// Test conversions to Int
TEST_F(ConstConvertTest, ConvertToInt)
{
    TEST_CONVERSION(min_char, TYPE_INT, CONST_INT, -128, int_val);
    TEST_CONVERSION(max_uchar, TYPE_INT, CONST_INT, 255, int_val);
    TEST_CONVERSION(max_long, TYPE_INT, CONST_INT, -1, int_val);    // INT64_MAX -> -1
    TEST_CONVERSION(max_uint, TYPE_INT, CONST_INT, -1, int_val);    // UINT32_MAX -> -1
    TEST_CONVERSION(max_ulong, TYPE_INT, CONST_INT, -1, int_val);   // UINT64_MAX -> -1
    TEST_CONVERSION(large_double, TYPE_INT, CONST_INT, 0, int_val); // 1e18 -> 0
}

// Test conversions to Long
TEST_F(ConstConvertTest, ConvertToLong)
{
    TEST_CONVERSION(min_char, TYPE_LONG, CONST_LONG, -128, long_val);
    TEST_CONVERSION(max_uchar, TYPE_LONG, CONST_LONG, 255, long_val);
    TEST_CONVERSION(max_int, TYPE_LONG, CONST_LONG, INT32_MAX, long_val);
    TEST_CONVERSION(max_uint, TYPE_LONG, CONST_LONG, UINT32_MAX, long_val);
    TEST_CONVERSION(max_ulong, TYPE_LONG, CONST_LONG, -1, long_val);           // UINT64_MAX -> -1
    TEST_CONVERSION(large_double, TYPE_LONG, CONST_LONG, INT64_MAX, long_val); // 1e18 -> INT64_MAX
}

// Test conversions to UInt
TEST_F(ConstConvertTest, ConvertToUInt)
{
    TEST_CONVERSION(min_char, TYPE_UINT, CONST_UINT, (uint32_t)-128, uint_val);
    TEST_CONVERSION(max_uchar, TYPE_UINT, CONST_UINT, 255, uint_val);
    TEST_CONVERSION(max_int, TYPE_UINT, CONST_UINT, INT32_MAX, uint_val);
    TEST_CONVERSION(max_long, TYPE_UINT, CONST_UINT, UINT32_MAX, uint_val);  // INT64_MAX -> -1
    TEST_CONVERSION(max_ulong, TYPE_UINT, CONST_UINT, UINT32_MAX, uint_val); // UINT64_MAX -> -1
    TEST_CONVERSION(large_double, TYPE_UINT, CONST_UINT, 0, uint_val);       // 1e18 -> 0
}

// Test conversions to ULong
TEST_F(ConstConvertTest, ConvertToULong)
{
    TEST_CONVERSION(min_char, TYPE_ULONG, CONST_ULONG, (uint64_t)-128, ulong_val);
    TEST_CONVERSION(max_uchar, TYPE_ULONG, CONST_ULONG, 255, ulong_val);
    TEST_CONVERSION(max_int, TYPE_ULONG, CONST_ULONG, INT32_MAX, ulong_val);
    TEST_CONVERSION(max_long, TYPE_ULONG, CONST_ULONG, INT64_MAX, ulong_val);
    TEST_CONVERSION(max_uint, TYPE_ULONG, CONST_ULONG, UINT32_MAX, ulong_val);
    TEST_DOUBLE_CONVERSION(large_double, TYPE_ULONG, CONST_ULONG, 1e18); // Special case
}

// Test conversions to Double
TEST_F(ConstConvertTest, ConvertToDouble)
{
    TEST_DOUBLE_CONVERSION(min_char, TYPE_DOUBLE, CONST_DOUBLE, -128.0);
    TEST_DOUBLE_CONVERSION(max_uchar, TYPE_DOUBLE, CONST_DOUBLE, 255.0);
    TEST_DOUBLE_CONVERSION(max_int, TYPE_DOUBLE, CONST_DOUBLE, (double)INT32_MAX);
    TEST_DOUBLE_CONVERSION(max_long, TYPE_DOUBLE, CONST_DOUBLE, (double)INT64_MAX);
    TEST_DOUBLE_CONVERSION(max_uint, TYPE_DOUBLE, CONST_DOUBLE, (double)UINT32_MAX);
    TEST_DOUBLE_CONVERSION(max_ulong, TYPE_DOUBLE, CONST_DOUBLE,
                           (double)UINT64_MAX); // Special case
    TEST_DOUBLE_CONVERSION(neg_double, TYPE_DOUBLE, CONST_DOUBLE, -1e18);
}

// Test conversions to Pointer (treated as ULong)
TEST_F(ConstConvertTest, ConvertToPointer)
{
    TEST_CONVERSION(min_char, TYPE_POINTER, CONST_ULONG, (uint64_t)-128, ulong_val);
    TEST_CONVERSION(max_uchar, TYPE_POINTER, CONST_ULONG, 255, ulong_val);
    TEST_CONVERSION(max_int, TYPE_POINTER, CONST_ULONG, INT32_MAX, ulong_val);
    TEST_CONVERSION(max_long, TYPE_POINTER, CONST_ULONG, INT64_MAX, ulong_val);
    TEST_CONVERSION(max_uint, TYPE_POINTER, CONST_ULONG, UINT32_MAX, ulong_val);
    TEST_CONVERSION(large_double, TYPE_POINTER, CONST_ULONG, (uint64_t)1e18, ulong_val);
}

// Test special cases (ULong <-> Double)
TEST_F(ConstConvertTest, SpecialCases)
{
    // ULong to Double
    Const large_ulong = make_const_ulong(UINT64_MAX);
    TEST_DOUBLE_CONVERSION(large_ulong, TYPE_DOUBLE, CONST_DOUBLE, (double)UINT64_MAX);

    // Double to ULong
    Const double_1e18 = make_const_double(1e18);
    TEST_CONVERSION(double_1e18, TYPE_ULONG, CONST_ULONG, (uint64_t)1e18, ulong_val);
}

// Test error cases (non-scalar types)
TEST_F(ConstConvertTest, NonScalarTypes)
{
    EXPECT_DEATH(const_convert(TYPE_VOID, zero_char), "non-scalar type");
    EXPECT_DEATH(const_convert(TYPE_ARRAY, max_uchar), "non-scalar type");
    EXPECT_DEATH(const_convert(TYPE_FUNTYPE, max_int), "non-scalar type");
    EXPECT_DEATH(const_convert(TYPE_STRUCTURE, zero_double), "non-scalar type");
}

// Main function to run tests
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
