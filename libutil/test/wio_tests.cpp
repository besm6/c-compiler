#include <errno.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "wio.h"
#include "xalloc.h"

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

    void TearDown() override { unlink(filename); }

    char filename[32];
};

//
// WOpenRead/Write/Append: Verify `wopen` opens files correctly,
// checking mode and initial state.
//
TEST_F(WIOTest, WOpenRead)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "r"), 0);
    EXPECT_GE(wfileno(&stream), 0);
    EXPECT_EQ(stream.mode, 'r');
    EXPECT_FALSE(weof(&stream));
    EXPECT_FALSE(werror(&stream));
    wclose(&stream);
}

TEST_F(WIOTest, WOpenWrite)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "w"), 0);
    EXPECT_EQ(stream.mode, 'w');
    EXPECT_FALSE(weof(&stream));
    EXPECT_FALSE(werror(&stream));
    wclose(&stream);
}

TEST_F(WIOTest, WOpenAppend)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "a"), 0);
    EXPECT_EQ(stream.mode, 'a');
    EXPECT_FALSE(weof(&stream));
    EXPECT_FALSE(werror(&stream));
    wclose(&stream);
}

//
// Test invalid mode handling.
//
TEST_F(WIOTest, WOpenInvalidMode)
{
    WFILE stream;
    ASSERT_LT(wopen(&stream, filename, "x"), 0);
    EXPECT_EQ(errno, EINVAL);
}

//
// Test reopening a stream with a new mode.
//
TEST_F(WIOTest, WReopen)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "w"), 0);
    ASSERT_GE(wreopen(&stream, filename, "r"), 0);
    EXPECT_EQ(stream.mode, 'r');
    wclose(&stream);
}

//
// Verify creating a `WFILE` from a file descriptor.
//
TEST_F(WIOTest, WDOpen)
{
    int fd = creat(filename, 0x600);
    ASSERT_GE(fd, 0) << "Failed to create temporary file";

    WFILE stream;
    ASSERT_GE(wdopen(&stream, fd, "r"), 0);
    EXPECT_EQ(wfileno(&stream), fd);
    EXPECT_EQ(stream.mode, 'r');
    wclose(&stream);
    EXPECT_GE(close(fd), 0); // Note: wclose() does not close fd after wdopen()
}

//
// Test invalid mode for `wdopen`.
//
TEST_F(WIOTest, WDOpenInvalidMode)
{
    int fd = creat(filename, 0x600);
    ASSERT_GE(fd, 0) << "Failed to create temporary file";

    WFILE stream;
    ASSERT_LT(wdopen(&stream, fd, "x"), 0);
    EXPECT_EQ(errno, EINVAL);
    EXPECT_GE(close(fd), 0);
}

//
// Write words to a file and read them back, verifying data integrity and EOF.
//
TEST_F(WIOTest, WPutWAndWGetW)
{
    WFILE wstream;
    ASSERT_GE(wopen(&wstream, filename, "w"), 0);
    static const size_t data[] = { 42, 123, 999 };
    for (size_t w : data) {
        EXPECT_EQ(wputw(w, &wstream), 0);
    }
    EXPECT_EQ(wflush(&wstream), 0);
    wclose(&wstream);

    WFILE rstream;
    ASSERT_GE(wopen(&rstream, filename, "r"), 0);
    for (size_t w : data) {
        EXPECT_EQ(wgetw(&rstream), w);
    }
    EXPECT_FALSE(weof(&rstream));           // at file end
    EXPECT_EQ(wgetw(&rstream), (size_t)-1); // failed
    EXPECT_TRUE(weof(&rstream));            // beyond file end
    wclose(&rstream);
}

//
// WPutWInvalidStream/WGetWInvalidStream: Test error handling for incorrect mode usage.
//
TEST_F(WIOTest, WPutWInvalidStream)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "r"), 0);
    ASSERT_NE(&stream, nullptr);
    EXPECT_EQ(wputw(42, &stream), -1);
    EXPECT_EQ(errno, EINVAL);
    wclose(&stream);
}

TEST_F(WIOTest, WGetWInvalidStream)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "w"), 0);
    EXPECT_EQ(wgetw(&stream), (size_t)-1);
    EXPECT_EQ(errno, EINVAL);
    wclose(&stream);
}

//
// Test seeking to specific word positions and verifying position with `wtell`.
//
TEST_F(WIOTest, WSeekAndWTell)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "w"), 0);
    EXPECT_EQ(wputw(1, &stream), 0);
    EXPECT_EQ(wputw(2, &stream), 0);
    EXPECT_EQ(wflush(&stream), 0);
    wclose(&stream);

    ASSERT_GE(wopen(&stream, filename, "r"), 0);
    ASSERT_NE(&stream, nullptr);
    EXPECT_EQ(wtell(&stream), 0);
    EXPECT_EQ(wseek(&stream, 1, SEEK_SET), 0);
    EXPECT_EQ(wtell(&stream), 1);
    EXPECT_EQ(wgetw(&stream), 2);
    EXPECT_EQ(wseek(&stream, 0, SEEK_SET), 0);
    EXPECT_EQ(wgetw(&stream), 1);
    wclose(&stream);
}

//
// Verify rewinding resets the stream to the start.
//
TEST_F(WIOTest, WRewind)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "w"), 0);
    EXPECT_EQ(wputw(1, &stream), 0);
    EXPECT_EQ(wflush(&stream), 0);
    wclose(&stream);

    ASSERT_GE(wopen(&stream, filename, "r"), 0);
    EXPECT_EQ(wgetw(&stream), 1);
    wrewind(&stream);
    EXPECT_EQ(wtell(&stream), 0);
    EXPECT_EQ(wgetw(&stream), 1);
    wclose(&stream);
}

//
// Test EOF detection after reading past file end.
//
TEST_F(WIOTest, WEndOfFile)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "r"), 0);
    EXPECT_FALSE(weof(&stream));
    EXPECT_EQ(wgetw(&stream), (size_t)-1);
    EXPECT_TRUE(weof(&stream));
    wclose(&stream);
}

//
// Test error detection with an invalid file descriptor.
//
TEST_F(WIOTest, WError)
{
    // Test error by attempting to read from an invalid fd
    WFILE stream;
    ASSERT_GE(wdopen(&stream, -1, "r"), 0);
    EXPECT_FALSE(werror(&stream));
    EXPECT_EQ(wgetw(&stream), (size_t)-1);
    EXPECT_TRUE(werror(&stream));
    wclose(&stream);
}

//
// Verify clearing EOF and error flags.
//
TEST_F(WIOTest, WClearErr)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "r"), 0);
    EXPECT_EQ(wgetw(&stream), (size_t)-1);
    EXPECT_TRUE(weof(&stream));
    wclearerr(&stream);
    EXPECT_FALSE(weof(&stream));
    EXPECT_FALSE(werror(&stream));
    wclose(&stream);
}

//
// WFileno/WFilenoInvalid: Test retrieving the file descriptor and handling null streams.
//
TEST_F(WIOTest, WFileno)
{
    WFILE stream;
    ASSERT_GE(wopen(&stream, filename, "r"), 0);
    EXPECT_GE(wfileno(&stream), 0);
    wclose(&stream);
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

//
// Write strings to a file and read them back, verifying data integrity and EOF.
//
TEST_F(WIOTest, WPutStrAndWGetStr)
{
    WFILE wstream;
    ASSERT_GE(wopen(&wstream, filename, "w"), 0);

    static const char *twas =
        "Twas brillig, and the slithy toves Did gyre and gimble in the wabefoobar";
    EXPECT_EQ(wputw(42, &wstream), 0);
    EXPECT_EQ(wputstr("foobar", &wstream), 0);
    EXPECT_EQ(wputstr(nullptr, &wstream), 0); // null string
    EXPECT_EQ(wputw(123, &wstream), 0);
    EXPECT_EQ(wputstr(twas, &wstream), 0);
    EXPECT_EQ(wputw(999, &wstream), 0);
    EXPECT_EQ(wflush(&wstream), 0);
    wclose(&wstream);

    WFILE rstream;
    ASSERT_GE(wopen(&rstream, filename, "r"), 0);
    EXPECT_EQ(wgetw(&rstream), 42);
    char *str = wgetstr(&rstream);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "foobar");
    xfree(str);
    str = wgetstr(&rstream); // null string
    ASSERT_EQ(str, nullptr);
    EXPECT_EQ(wgetw(&rstream), 123);
    str = wgetstr(&rstream);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, twas);
    xfree(str);
    EXPECT_EQ(wgetw(&rstream), 999);
    EXPECT_FALSE(weof(&rstream));           // at file end
    EXPECT_EQ(wgetw(&rstream), (size_t)-1); // failed
    EXPECT_TRUE(weof(&rstream));            // beyond file end
    wclose(&rstream);
}

//
// Write byte blobs holding embedded NUL bytes and read them back: unlike wputstr(),
// wputdata() carries an explicit length, so nothing is cut short at a zero byte.
// This is what a decoded C string literal such as "a\0c" needs.
//
TEST_F(WIOTest, WPutDataAndWGetData)
{
    static const char embedded[] = "a\0c\0\0z"; // 6 bytes, four of them past a NUL
    WFILE wstream;
    ASSERT_GE(wopen(&wstream, filename, "w"), 0);
    EXPECT_EQ(wputw(42, &wstream), 0);
    EXPECT_EQ(wputdata(embedded, sizeof(embedded) - 1, &wstream), 0);
    EXPECT_EQ(wputdata("", 0, &wstream), 0);                  // empty blob
    EXPECT_EQ(wputdata("exactly-eight!!!", 16, &wstream), 0); // whole number of words
    EXPECT_EQ(wputw(999, &wstream), 0);
    EXPECT_EQ(wflush(&wstream), 0);
    wclose(&wstream);

    WFILE rstream;
    ASSERT_GE(wopen(&rstream, filename, "r"), 0);
    EXPECT_EQ(wgetw(&rstream), 42u);
    size_t len = 0;
    char *data = static_cast<char *>(wgetdata(&len, &rstream));
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(len, sizeof(embedded) - 1);
    EXPECT_EQ(memcmp(data, embedded, len), 0);
    xfree(data);
    data = static_cast<char *>(wgetdata(&len, &rstream)); // empty blob
    EXPECT_EQ(data, nullptr);
    EXPECT_EQ(len, 0u);
    data = static_cast<char *>(wgetdata(&len, &rstream));
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(len, 16u);
    EXPECT_EQ(memcmp(data, "exactly-eight!!!", 16), 0);
    xfree(data);
    EXPECT_EQ(wgetw(&rstream), 999u);
    wclose(&rstream);
}

//
// A string far longer than wgetstr()'s initial buffer must survive the round trip:
// the buffer grows instead of giving up and returning NULL (which silently dropped
// any string of 1016 bytes or more).
//
TEST_F(WIOTest, WGetStrLongString)
{
    std::string huge(4000, 'x');
    huge += "-end";

    WFILE wstream;
    ASSERT_GE(wopen(&wstream, filename, "w"), 0);
    EXPECT_EQ(wputstr(huge.c_str(), &wstream), 0);
    EXPECT_EQ(wputw(7, &wstream), 0);
    EXPECT_EQ(wflush(&wstream), 0);
    wclose(&wstream);

    WFILE rstream;
    ASSERT_GE(wopen(&rstream, filename, "r"), 0);
    char *str = wgetstr(&rstream);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, huge.c_str());
    xfree(str);
    EXPECT_EQ(wgetw(&rstream), 7u);
    wclose(&rstream);
}
