#include "wio.h"

#include <errno.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

//
// Test fixture for WIO tests
// Creates a temporary file using `mkstemp` for each test,
// ensuring a clean environment. The file is deleted in `TearDown`.
//
class WIOTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create a temporary file for testing
        strcpy(filename, "testfileXXXXXX");
        ASSERT_GE(close(mkstemp(filename)), 0);
    }

    void TearDown() override
    {
        unlink(filename);
    }

    char filename[32];
};

//
// WOpenRead/Write/Append: Verify `wopen` opens files correctly,
// checking mode and initial state.
//
TEST_F(WIOTest, WOpenRead)
{
    WFILE *stream = wopen(filename, "r");
    ASSERT_NE(stream, nullptr);
    EXPECT_GE(wfileno(stream), 0);
    EXPECT_EQ(stream->mode, 'r');
    EXPECT_FALSE(weof(stream));
    EXPECT_FALSE(werror(stream));
    wclose(stream);
}

TEST_F(WIOTest, WOpenWrite)
{
    WFILE *stream = wopen(filename, "w");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->mode, 'w');
    EXPECT_FALSE(weof(stream));
    EXPECT_FALSE(werror(stream));
    wclose(stream);
}

TEST_F(WIOTest, WOpenAppend)
{
    WFILE *stream = wopen(filename, "a");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->mode, 'a');
    EXPECT_FALSE(weof(stream));
    EXPECT_FALSE(werror(stream));
    wclose(stream);
}

//
// Test invalid mode handling.
//
TEST_F(WIOTest, WOpenInvalidMode)
{
    WFILE *stream = wopen(filename, "x");
    EXPECT_EQ(stream, nullptr);
    EXPECT_EQ(errno, EINVAL);
}

//
// Test reopening a stream with a new mode.
//
TEST_F(WIOTest, WReopen)
{
    WFILE *stream = wopen(filename, "w");
    ASSERT_NE(stream, nullptr);
    WFILE *new_stream = wreopen(filename, "r", stream);
    ASSERT_NE(new_stream, nullptr);
    EXPECT_EQ(new_stream->mode, 'r');
    wclose(new_stream);
}

//
// Verify creating a `WFILE` from a file descriptor.
//
TEST_F(WIOTest, WDOpen)
{
    int fd = creat(filename, 0x600);
    ASSERT_GE(fd, 0) << "Failed to create temporary file";

    WFILE *stream = wdopen(fd, "r");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(wfileno(stream), fd);
    EXPECT_EQ(stream->mode, 'r');
    wclose(stream);
    EXPECT_LT(close(fd), 0);
}

//
// Test invalid mode for `wdopen`.
//
TEST_F(WIOTest, WDOpenInvalidMode)
{
    int fd = creat(filename, 0x600);
    ASSERT_GE(fd, 0) << "Failed to create temporary file";

    WFILE *stream = wdopen(fd, "x");
    EXPECT_EQ(stream, nullptr);
    EXPECT_EQ(errno, EINVAL);
    EXPECT_GE(close(fd), 0);
}

//
// Write words to a file and read them back, verifying data integrity and EOF.
//
TEST_F(WIOTest, WPutWAndWGetW)
{
    WFILE *wstream = wopen(filename, "w");
    ASSERT_NE(wstream, nullptr);
    size_t data[] = { 42, 123, 999 };
    for (size_t w : data) {
        EXPECT_EQ(wputw(w, wstream), 0);
    }
    EXPECT_EQ(wflush(wstream), 0);
    wclose(wstream);

    WFILE *rstream = wopen(filename, "r");
    ASSERT_NE(rstream, nullptr);
    for (size_t w : data) {
        EXPECT_EQ(wgetw(rstream), w);
    }
    EXPECT_TRUE(weof(rstream));
    wclose(rstream);
}

//
// WPutWInvalidStream/WGetWInvalidStream: Test error handling for incorrect mode usage.
//
TEST_F(WIOTest, WPutWInvalidStream)
{
    WFILE *stream = wopen(filename, "r");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(wputw(42, stream), -1);
    EXPECT_EQ(errno, EINVAL);
    wclose(stream);
}

TEST_F(WIOTest, WGetWInvalidStream)
{
    WFILE *stream = wopen(filename, "w");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(wgetw(stream), (size_t)-1);
    EXPECT_EQ(errno, EINVAL);
    wclose(stream);
}

//
// Test seeking to specific word positions and verifying position with `wtell`.
//
TEST_F(WIOTest, WSeekAndWTell)
{
    WFILE *stream = wopen(filename, "w");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(wputw(1, stream), 0);
    EXPECT_EQ(wputw(2, stream), 0);
    EXPECT_EQ(wflush(stream), 0);
    wclose(stream);

    stream = wopen(filename, "r");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(wtell(stream), 0);
    EXPECT_EQ(wseek(stream, 1, SEEK_SET), 0);
    EXPECT_EQ(wtell(stream), 1);
    EXPECT_EQ(wgetw(stream), 2);
    EXPECT_EQ(wseek(stream, 0, SEEK_SET), 0);
    EXPECT_EQ(wgetw(stream), 1);
    wclose(stream);
}

//
// Verify rewinding resets the stream to the start.
//
TEST_F(WIOTest, WRewind)
{
    WFILE *stream = wopen(filename, "w");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(wputw(1, stream), 0);
    EXPECT_EQ(wflush(stream), 0);
    wclose(stream);

    stream = wopen(filename, "r");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(wgetw(stream), 1);
    wrewind(stream);
    EXPECT_EQ(wtell(stream), 0);
    EXPECT_EQ(wgetw(stream), 1);
    wclose(stream);
}

//
// Test EOF detection after reading past file end.
//
TEST_F(WIOTest, WEndOfFile)
{
    WFILE *stream = wopen(filename, "r");
    ASSERT_NE(stream, nullptr);
    EXPECT_FALSE(weof(stream));
    EXPECT_EQ(wgetw(stream), (size_t)-1);
    EXPECT_TRUE(weof(stream));
    wclose(stream);
}

//
// Test error detection with an invalid file descriptor.
//
TEST_F(WIOTest, WError)
{
    // Test error by attempting to read from an invalid fd
    WFILE *stream = wdopen(-1, "r");
    ASSERT_NE(stream, nullptr);
    EXPECT_FALSE(werror(stream));
    EXPECT_EQ(wgetw(stream), (size_t)-1);
    EXPECT_TRUE(werror(stream));
    wclose(stream);
}

//
// Verify clearing EOF and error flags.
//
TEST_F(WIOTest, WClearErr)
{
    WFILE *stream = wopen(filename, "r");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(wgetw(stream), (size_t)-1);
    EXPECT_TRUE(weof(stream));
    wclearerr(stream);
    EXPECT_FALSE(weof(stream));
    EXPECT_FALSE(werror(stream));
    wclose(stream);
}

//
// WFileno/WFilenoInvalid: Test retrieving the file descriptor and handling null streams.
//
TEST_F(WIOTest, WFileno)
{
    WFILE *stream = wopen(filename, "r");
    ASSERT_NE(stream, nullptr);
    EXPECT_GE(wfileno(stream), 0);
    wclose(stream);
}

TEST_F(WIOTest, WFilenoInvalid)
{
    EXPECT_EQ(wfileno(nullptr), -1);
    EXPECT_EQ(errno, EINVAL);
}

//
// Ensure `wclose` handles null pointers gracefully.
//
TEST_F(WIOTest, WCloseNull)
{
    wclose(nullptr); // Should not crash
}
