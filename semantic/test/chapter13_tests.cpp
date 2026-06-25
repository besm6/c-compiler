//
// Chapter 13 — Floating-point: semantic errors.  Imported from "Writing a C
// Compiler" (tests/chapter_13/invalid_types and invalid_types/extra_credit).
// Each program parses cleanly but applies an integer-only operator (~, %, the
// bitwise/shift operators, or a switch/case) to a double; tests assert on a
// substring of the fatal-error text.
//
#include "typecheck_fixture.h"

// --- invalid_types ----------------------------------------------------------

// ~10.0 — bitwise complement is integer-only.
TEST_F(PipelineTest, Chapter13_ComplementDouble_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double d = ~10.0;
    return 0;
}
)"),
                 "Bitwise complement only valid for integer types");
}

// d % 3 — the remainder operator rejects a floating left operand.
TEST_F(PipelineTest, Chapter13_ModDouble_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double d = 10.0;
    d = d % 3;
    return 0;
}
)"),
                 "Can't apply % to floating-point type");
}

// 3.0 % 5 — same, with a floating constant left operand.
TEST_F(PipelineTest, Chapter13_ModDouble2_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double e =  3.0 % 5;
    return 0;
}
)"),
                 "Can't apply % to floating-point type");
}

// --- invalid_types/extra_credit --------------------------------------------

// 10.0 & -1 — bitwise AND is integer-only.
TEST_F(PipelineTest, Chapter13_BitwiseAnd_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double d = 10.0 & -1;
    return 0;
}
)"),
                 "Bitwise operators require integer operands");
}

// 0.0 | -0.0 — bitwise OR is integer-only.
TEST_F(PipelineTest, Chapter13_BitwiseOr_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double d = 0.0 | -0.0;
    return 0;
}
)"),
                 "Bitwise operators require integer operands");
}

// 1e10 ^ -1e10 — bitwise XOR is integer-only.
TEST_F(PipelineTest, Chapter13_BitwiseXor_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    return 1e10 ^ -1e10;
}
)"),
                 "Bitwise operators require integer operands");
}

// 5.0 << 3 — the left operand of a shift must be an integer.
TEST_F(PipelineTest, Chapter13_BitwiseShiftDouble_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double d = 5.0 << 3;
    return 0;
}
)"),
                 "Shift operators require integer operands");
}

// 1 << 2.0 — the right operand of a shift must be an integer.
TEST_F(PipelineTest, Chapter13_BitwiseShiftDouble2_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    return 1 << 2.0;
}
)"),
                 "Shift operators require integer operands");
}

// d &= 0 — compound bitwise AND on a double.
TEST_F(PipelineTest, Chapter13_CompoundBitwiseAnd_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double d = 1.0;
    d &= 0;
    return (int) d;
}
)"),
                 "requires integer operands");
}

// i |= 2.0 — compound bitwise OR with a double right operand.
TEST_F(PipelineTest, Chapter13_CompoundBitwiseXor_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    int i = 0;
    i |= 2.0;
    return (int) i;
}
)"),
                 "requires integer operands");
}

// d <<= 1 — compound left shift on a double.
TEST_F(PipelineTest, Chapter13_CompoundLeftBitshift_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double d = 1.0;
    d <<= 1;
    return d;
}
)"),
                 "requires integer operands");
}

// i >>= 2.0 — compound right shift with a double right operand.
TEST_F(PipelineTest, Chapter13_CompoundRightBitshift_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    int i = 1000;
    i >>= 2.0;
    return i;
}
)"),
                 "requires integer operands");
}

// d %= 2 — compound remainder on a double.
TEST_F(PipelineTest, Chapter13_CompoundMod_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double d = 5.0;
    d %= 2;
    return (int) d;
}
)"),
                 "requires integer operands");
}

// i %= 1.0 — compound remainder with a double right operand.
TEST_F(PipelineTest, Chapter13_CompoundMod2_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    int i = 5;
    i %= 1.0;
    return i;
}
)"),
                 "requires integer operands");
}

// switch (d) where d is a double — the controlling expression must be integer.
TEST_F(PipelineTest, Chapter13_SwitchOnDouble_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    double d = 10;
    switch (d) {
        case 10: return 0;
    }
    return 1;
}
)"),
                 "Switch controlling expression must be of integer type");
}

// case 1.0 — a case label must be an integer constant.
TEST_F(PipelineTest, Chapter13_SwitchDoubleCase_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    int x = 10;
    switch (x) {
        case 1.0: return 0;
        default: return 4;
    }
}
)"),
                 "Case expression must be of integer type");
}
