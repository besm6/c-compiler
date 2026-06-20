// Run-test for the shipped <stdarg.h>.
//
// The compiler has no preprocessor, so this test first expands the header with
// the system cpp (the documented usage), then compiles the result through the
// full pipeline and runs it under Dubna.  It pins down the one runtime-visible
// property of the header: that va_start points at the first variadic argument
// and va_arg walks the parameter block in the right direction.
#include <cstdio>
#include <string>

#include "codegen_test.h"

namespace {

// Expand SRC with the system C preprocessor against the BESM-6 include dir.
// Returns the preprocessed text, or an empty string if cpp is unavailable.
std::string Preprocess(const std::string &src)
{
    char path[] = "/tmp/stdarg_src_XXXXXX";
    int fd      = mkstemp(path);
    if (fd < 0)
        return {};
    EXPECT_EQ(static_cast<ssize_t>(src.size()), write(fd, src.data(), src.size()));
    close(fd);

    std::string cmd = "cpp -P -nostdinc -I" BESM6_INCLUDE_DIR " ";
    cmd += path;
    cmd += " 2>/dev/null";

    std::string out;
    FILE *p = popen(cmd.c_str(), "r");
    if (p) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0)
            out.append(buf, n);
        if (pclose(p) != 0)
            out.clear();
    }
    unlink(path);
    return out;
}

} // namespace

TEST_F(CodegenTest, StdargVaList)
{
    std::string expanded = Preprocess(R"(
        #include <stdarg.h>
        int printf(char *fmt, ...);
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

    if (expanded.empty())
        GTEST_SKIP() << "system cpp unavailable; skipping <stdarg.h> run-test";

    EXPECT_EQ("100\n", CompileAndRun(expanded));
}
