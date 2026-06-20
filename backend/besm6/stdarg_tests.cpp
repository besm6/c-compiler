// Run-test for the shipped <stdarg.h>.
//
// The compiler has no preprocessor, so the CodegenTest fixture expands the header
// with the system cpp (the documented usage) before compiling, then runs the
// result through the full pipeline under Dubna.  It pins down the one
// runtime-visible property of the header: that va_start points at the first
// variadic argument and va_arg walks the parameter block in the right direction.
#include <string>

#include "codegen_test.h"

TEST_F(CodegenTest, StdargVaList)
{
    std::string result = CompileAndRun(R"(
        #include <stdarg.h>
        #include <stdio.h>
        int sum(int n, ...)
        {
            va_list ap;
            va_start(ap, n);
            int total = 0;
            for (int i = 0; i < n; i++)
                total += va_arg(ap, int);
            va_end(ap);
            return total;
        }
        void program()
        {
            printf("%d\n", sum(4, 10, 20, 30, 40));
        }
    )");

    EXPECT_EQ("100\n", result);
}
