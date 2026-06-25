// Make manual test runs chdir() into the target's build directory at startup so output
// files land there (matching how ctest already runs these binaries) instead of littering
// the source tree. Compiled into each test executable with a per-target TEST_BINARY_DIR.
#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>

namespace {
class ChdirToBinaryDir : public ::testing::Environment {
public:
    void SetUp() override
    {
        // TEST_BINARY_DIR is a quoted string-literal macro, so the three adjacent
        // literals concatenate at compile time and the actual path appears in the
        // message, e.g.:  chdir(/Users/.../build/semantic): No such file or directory
        if (chdir(TEST_BINARY_DIR) != 0)
            std::perror("chdir(" TEST_BINARY_DIR ")");
    }
};
// Registered before gtest_main's RUN_ALL_TESTS(); SetUp() runs once before any test.
[[maybe_unused]] const ::testing::Environment *const kChdirEnv =
    ::testing::AddGlobalTestEnvironment(new ChdirToBinaryDir);
} // namespace
