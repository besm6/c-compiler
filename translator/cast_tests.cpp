#include "translate_test.h"

// ---------------------------------------------------------------------------
// Type casts and numeric conversions — task #6
// ---------------------------------------------------------------------------

// (long)x where x is int: narrower signed → wider signed → sign_extend
TEST_F(TranslateTest, CastIntToLong)
{
    std::string yaml = CompileToYaml("long f(int x) { return (long)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: sign_extend
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (unsigned long)x where x is unsigned int: narrower unsigned → wider → zero_extend
TEST_F(TranslateTest, CastUintToUlong)
{
    std::string yaml =
        CompileToYaml("unsigned long f(unsigned int x) { return (unsigned long)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: zero_extend
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (int)x where x is long: wider → narrower → truncate
TEST_F(TranslateTest, CastLongToInt)
{
    std::string yaml = CompileToYaml("int f(long x) { return (int)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: truncate
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (unsigned int)x where x is int: same size, different sign → copy
TEST_F(TranslateTest, CastIntToUint)
{
    std::string yaml = CompileToYaml("unsigned int f(int x) { return (unsigned int)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: copy
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (double)x where x is int: signed integer → double → int_to_double
TEST_F(TranslateTest, CastIntToDouble)
{
    std::string yaml = CompileToYaml("double f(int x) { return (double)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: int_to_double
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (double)x where x is unsigned int: unsigned integer → double → uint_to_double
TEST_F(TranslateTest, CastUintToDouble)
{
    std::string yaml = CompileToYaml("double f(unsigned int x) { return (double)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: uint_to_double
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (int)x where x is double: double → signed integer → double_to_int
TEST_F(TranslateTest, CastDoubleToInt)
{
    std::string yaml = CompileToYaml("int f(double x) { return (int)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: double_to_int
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (unsigned int)x where x is double: double → unsigned integer → double_to_uint
TEST_F(TranslateTest, CastDoubleToUint)
{
    std::string yaml = CompileToYaml("unsigned int f(double x) { return (unsigned int)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: double_to_uint
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (short)x where x is int: wider → narrower → truncate
TEST_F(TranslateTest, CastIntToShort)
{
    std::string yaml = CompileToYaml("short f(int x) { return (short)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: truncate
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (int)x where x is short: narrower signed → wider signed → sign_extend
TEST_F(TranslateTest, CastShortToInt)
{
    std::string yaml = CompileToYaml("int f(short x) { return (int)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: sign_extend
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (unsigned short)x where x is unsigned int: wider → narrower → truncate
TEST_F(TranslateTest, CastUintToUshort)
{
    std::string yaml =
        CompileToYaml("unsigned short f(unsigned int x) { return (unsigned short)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: truncate
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (unsigned int)x where x is unsigned short: narrower unsigned → wider → zero_extend
TEST_F(TranslateTest, CastUshortToUint)
{
    std::string yaml =
        CompileToYaml("unsigned int f(unsigned short x) { return (unsigned int)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: zero_extend
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (long)x where x is short: narrower signed → wider → sign_extend
TEST_F(TranslateTest, CastShortToLong)
{
    std::string yaml = CompileToYaml("long f(short x) { return (long)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: sign_extend
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (unsigned long)x where x is unsigned short: narrower unsigned → wider → zero_extend
TEST_F(TranslateTest, CastUshortToUlong)
{
    std::string yaml =
        CompileToYaml("unsigned long f(unsigned short x) { return (unsigned long)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: zero_extend
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// Cast embedded in a larger expression: (long)x + y
TEST_F(TranslateTest, CastEmbeddedInExpr)
{
    std::string yaml = CompileToYaml("long f(int x, long y) { return (long)x + y; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
    - param: %y
  body:
    - instruction:
      kind: sign_extend
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: binary
      op: add
      src1:
        kind: var
        name: %0
      src2:
        kind: var
        name: %y
      dst:
        kind: var
        name: %1
    - instruction:
      kind: return
      src:
        kind: var
        name: %1
)");
}

// (float)x where x is int: signed integer → float → int_to_float
TEST_F(TranslateTest, CastIntToFloat)
{
    std::string yaml = CompileToYaml("float f(int x) { return (float)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: int_to_float
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (int)x where x is float: float → signed integer → float_to_int
TEST_F(TranslateTest, CastFloatToInt)
{
    std::string yaml = CompileToYaml("int f(float x) { return (int)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: float_to_int
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (double)x where x is float: float → double → float_to_double
TEST_F(TranslateTest, CastFloatToDouble)
{
    std::string yaml = CompileToYaml("double f(float x) { return (double)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: float_to_double
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// (float)x where x is double: double → float → double_to_float
TEST_F(TranslateTest, CastDoubleToFloat)
{
    std::string yaml = CompileToYaml("float f(double x) { return (float)x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: %x
  body:
    - instruction:
      kind: double_to_float
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// ---------------------------------------------------------------------------
// long long cast tests
// ---------------------------------------------------------------------------

// (long long)x where x is int: narrower signed → wider signed → sign_extend
TEST_F(TranslateTest, CastIntToLongLong)
{
    std::string yaml = CompileToYaml("long long f(int x) { return (long long)x; }");
    EXPECT_NE(yaml.find("kind: sign_extend"), std::string::npos);
}

// (int)x where x is long long: wider → narrower → truncate
TEST_F(TranslateTest, CastLongLongToInt)
{
    std::string yaml = CompileToYaml("int f(long long x) { return (int)x; }");
    EXPECT_NE(yaml.find("kind: truncate"), std::string::npos);
}

// (unsigned long long)x where x is long long: same size → copy
TEST_F(TranslateTest, CastLongLongToULongLong)
{
    std::string yaml =
        CompileToYaml("unsigned long long f(long long x) { return (unsigned long long)x; }");
    EXPECT_NE(yaml.find("kind: copy"), std::string::npos);
}

// (long long)x where x is unsigned int: narrower unsigned → wider signed → zero_extend
TEST_F(TranslateTest, CastUintToLongLong)
{
    std::string yaml = CompileToYaml("long long f(unsigned int x) { return (long long)x; }");
    EXPECT_NE(yaml.find("kind: zero_extend"), std::string::npos);
}

// ---------------------------------------------------------------------------
// long double casts
// ---------------------------------------------------------------------------

// (long double)x where x is int: signed integer → long double
TEST_F(TranslateTest, CastIntToLongDouble)
{
    std::string yaml = CompileToYaml("long double f(int x) { return (long double)x; }");
    EXPECT_NE(yaml.find("kind: int_to_long_double"), std::string::npos);
}

// (long double)x where x is unsigned int: unsigned integer → long double
TEST_F(TranslateTest, CastUintToLongDouble)
{
    std::string yaml = CompileToYaml("long double f(unsigned int x) { return (long double)x; }");
    EXPECT_NE(yaml.find("kind: uint_to_long_double"), std::string::npos);
}

// (int)x where x is long double: long double → signed integer
TEST_F(TranslateTest, CastLongDoubleToInt)
{
    std::string yaml = CompileToYaml("int f(long double x) { return (int)x; }");
    EXPECT_NE(yaml.find("kind: long_double_to_int"), std::string::npos);
}

// (unsigned int)x where x is long double: long double → unsigned integer
TEST_F(TranslateTest, CastLongDoubleToUint)
{
    std::string yaml = CompileToYaml("unsigned int f(long double x) { return (unsigned int)x; }");
    EXPECT_NE(yaml.find("kind: long_double_to_uint"), std::string::npos);
}

// (long double)x where x is double: double → long double
TEST_F(TranslateTest, CastDoubleToLongDouble)
{
    std::string yaml = CompileToYaml("long double f(double x) { return (long double)x; }");
    EXPECT_NE(yaml.find("kind: double_to_long_double"), std::string::npos);
}

// (double)x where x is long double: long double → double
TEST_F(TranslateTest, CastLongDoubleToDouble)
{
    std::string yaml = CompileToYaml("double f(long double x) { return (double)x; }");
    EXPECT_NE(yaml.find("kind: long_double_to_double"), std::string::npos);
}

// (long double)x where x is float: float → long double
TEST_F(TranslateTest, CastFloatToLongDouble)
{
    std::string yaml = CompileToYaml("long double f(float x) { return (long double)x; }");
    EXPECT_NE(yaml.find("kind: float_to_long_double"), std::string::npos);
}

// (float)x where x is long double: long double → float
TEST_F(TranslateTest, CastLongDoubleToFloat)
{
    std::string yaml = CompileToYaml("float f(long double x) { return (float)x; }");
    EXPECT_NE(yaml.find("kind: long_double_to_float"), std::string::npos);
}
