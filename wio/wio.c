//
// Word-oriented buffered I/O interface.
// This implementation uses a buffer to handle word-sized I/O operations,
// built on top of standard file descriptors for underlying I/O.
// A `WFILE` structure includes a buffer, file descriptor, and state flags,
// with functions mirroring `stdio.h` but operating on `size_t` words in native byte order.
//
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "wio.h"

#define BUFFER_SIZE (4096 / sizeof(size_t)) /* Buffer holds words, aligned to page size */

//
// Open a file with appropriate flags based on mode (`r`, `w`, `a`),
// allocate a `WFILE` structure and buffer.
//
WFILE *wopen(const char *path, const char *mode)
{
    int flags = 0;
    int fd;
    char m = mode[0];

    if (m == 'r') {
        flags = O_RDONLY;
    } else if (m == 'w') {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (m == 'a') {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        errno = EINVAL;
        return NULL;
    }

    fd = open(path, flags, 0666);
    if (fd == -1)
        return NULL;

    WFILE *stream = malloc(sizeof(WFILE));
    if (!stream) {
        close(fd);
        return NULL;
    }

    stream->buffer = malloc(BUFFER_SIZE * sizeof(size_t));
    if (!stream->buffer) {
        close(fd);
        free(stream);
        return NULL;
    }

    stream->fd = fd;
    stream->buffer_pos = 0;
    stream->buffer_count = 0;
    stream->is_eof = false;
    stream->is_error = false;
    stream->mode = m;

    return stream;
}

//
// Close the existing stream (flushing if needed), then reopen with new parameters.
//
WFILE *wreopen(const char *path, const char *mode, WFILE *stream)
{
    if (stream) {
        if (stream->mode == 'w' || stream->mode == 'a') {
            wflush(stream);
        }
        close(stream->fd);
        free(stream->buffer);
        free(stream);
    }

    return wopen(path, mode);
}

//
// Create a `WFILE` from an existing file descriptor.
//
WFILE *wdopen(int fildes, const char *mode)
{
    char m = mode[0];
    if (m != 'r' && m != 'w' && m != 'a') {
        errno = EINVAL;
        return NULL;
    }

    WFILE *stream = malloc(sizeof(WFILE));
    if (!stream)
        return NULL;

    stream->buffer = malloc(BUFFER_SIZE * sizeof(size_t));
    if (!stream->buffer) {
        free(stream);
        return NULL;
    }

    stream->fd = fildes;
    stream->buffer_pos = 0;
    stream->buffer_count = 0;
    stream->is_eof = false;
    stream->is_error = false;
    stream->mode = m;

    return stream;
}

//
// Write buffered data to the file for write/append modes.
//
int wflush(WFILE *stream)
{
    if (!stream || stream->mode == 'r') {
        errno = EINVAL;
        return -1;
    }

    if (stream->buffer_pos > 0) {
        ssize_t bytes_to_write = stream->buffer_pos * sizeof(size_t);
        ssize_t bytes_written = write(stream->fd, stream->buffer, bytes_to_write);
        if (bytes_written != bytes_to_write) {
            stream->is_error = true;
            return -1;
        }
        stream->buffer_pos = 0;
    }
    return 0;
}

//
// Flush (if needed) and free resources.
//
void wclose(WFILE *stream)
{
    if (!stream)
        return;

    if (stream->mode == 'w' || stream->mode == 'a') {
        wflush(stream);
    }

    close(stream->fd);
    free(stream->buffer);
    free(stream);
}

//
// Adjust file position in word units, flush buffer if writing.
//
int wseek(WFILE *stream, long offset, int whence)
{
    if (!stream) {
        errno = EINVAL;
        return -1;
    }

    if (stream->mode == 'w' || stream->mode == 'a') {
        wflush(stream);
    }

    off_t new_offset = lseek(stream->fd, offset * sizeof(size_t), whence);
    if (new_offset == -1) {
        stream->is_error = true;
        return -1;
    }

    stream->buffer_pos = 0;
    stream->buffer_count = 0;
    stream->is_eof = false;
    return 0;
}

//
// Return current position in words, accounting for buffered data.
//
long wtell(WFILE *stream)
{
    if (!stream) {
        errno = EINVAL;
        return -1;
    }

    off_t pos = lseek(stream->fd, 0, SEEK_CUR);
    if (pos == -1) {
        stream->is_error = true;
        return -1;
    }

    if (stream->mode == 'r') {
        pos -= stream->buffer_count * sizeof(size_t);
        pos += stream->buffer_pos * sizeof(size_t);
    } else {
        pos += stream->buffer_pos * sizeof(size_t);
    }

    return pos / sizeof(size_t);
}

//
// Seek to the start of the file.
//
void wrewind(WFILE *stream)
{
    if (stream) {
        wseek(stream, 0, SEEK_SET);
    }
}

//
// Read the next word, refilling the buffer if empty. Return `(size_t)-1` on EOF or error.
//
size_t wgetw(WFILE *stream)
{
    if (!stream || stream->mode != 'r') {
        errno = EINVAL;
        return (size_t)-1;
    }

    if (stream->buffer_pos >= stream->buffer_count) {
        stream->buffer_pos = 0;
        stream->buffer_count = 0;

        ssize_t bytes_read = read(stream->fd, stream->buffer, BUFFER_SIZE * sizeof(size_t));
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                stream->is_eof = true;
            } else {
                stream->is_error = true;
            }
            return (size_t)-1;
        }

        stream->buffer_count = bytes_read / sizeof(size_t);
        if (bytes_read % sizeof(size_t) != 0) {
            stream->is_error = true;
            return (size_t)-1;
        }
    }

    return stream->buffer[stream->buffer_pos++];
}

//
// Buffer a word, flushing when the buffer is full.
//
int wputw(size_t w, WFILE *stream)
{
    if (!stream || (stream->mode != 'w' && stream->mode != 'a')) {
        errno = EINVAL;
        return -1;
    }

    if (stream->buffer_pos >= BUFFER_SIZE) {
        if (wflush(stream) != 0) {
            return -1;
        }
    }

    stream->buffer[stream->buffer_pos++] = w;
    return 0;
}

//
// Check EOF state.
//
bool weof(WFILE *stream)
{
    if (!stream)
        return true;
    return stream->is_eof;
}

//
// Check error state.
//
bool werror(WFILE *stream)
{
    if (!stream)
        return true;
    return stream->is_error;
}

//
// Return the underlying file descriptor.
//
int wfileno(WFILE *stream)
{
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    return stream->fd;
}

//
// Clear EOF and error flags.
//
void wclearerr(WFILE *stream)
{
    if (stream) {
        stream->is_eof = false;
        stream->is_error = false;
    }
}

//
// Read a zero terminated string, aligned to word boundary.
// Return a dynamically allocated buffer.
// Max size is limited by 128 words.
//
char *wgetstr(WFILE *stream)
{
    size_t buf[128];
    int n;

    for (;;) {
        buf[n] = wgetw(stream);
        if (stream->is_eof) {
            return NULL;
        }
        // Does this word contain '\0' byte?
        bool is_last_word = memchr(&buf[n], '\0', sizeof(size_t)) != NULL;
        if (is_last_word) {
            return strdup((char*) buf);
        }
        n++;
        if (n * sizeof(size_t) >= sizeof(buf)) {
            // Too long string
            return NULL;
        }
    }
}

//
// Write a zero terminated string, aligned to word boundary.
//
int wputstr(const char *str, WFILE *stream)
{
    for (;;) {
        size_t w = 0;
        bool is_last_word = memccpy(&w, str, '\0', sizeof(w)) != NULL;

        if (wputw(w, stream) < 0) {
            return -1;
        }
        if (is_last_word) {
            return 0;
        }
        str += sizeof(size_t);
    }
}

//
// Read FP value. Return NaN on EOF or error.
//
double wgetd(WFILE *stream)
{
    if (sizeof(double) == sizeof(size_t)) {
        // One word
        union {
            size_t w;
            double f;
        } u;
        u.w = wgetw(stream);
        if (u.w == (size_t)-1)
            return nan("");
        return u.f;
    }
    if (sizeof(double) == 2*sizeof(size_t)) {
        // Two words
        union {
            size_t w[2];
            double f;
        } u;
        u.w[0] = wgetw(stream);
        if (u.w[0] == (size_t)-1)
            return nan("");
        u.w[1] = wgetw(stream);
        if (u.w[1] == (size_t)-1)
            return nan("");
        return u.f;
    }
    // Cannot deserialize
    errno = EINVAL;
    return -1;
}

//
// Buffer an FP value.
//
int wputd(double f, WFILE *stream)
{
    if (sizeof(double) == sizeof(size_t)) {
        // One word
        union {
            double f;
            size_t w;
        } u;
        u.f = f;
        return wputw(u.w, stream);
    }
    if (sizeof(double) == 2*sizeof(size_t)) {
        // Two words
        union {
            double f;
            size_t w[2];
        } u;
        u.f = f;
        if (wputw(u.w[0], stream) < 0)
            return -1;
        return wputw(u.w[1], stream);
    }
    // Cannot serialize
    errno = EINVAL;
    return -1;
}
