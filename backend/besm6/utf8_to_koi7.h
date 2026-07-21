#pragma once

#include <stddef.h>

//
// Convert a source string in UTF-8 encoding (NUL terminated)
// to a destination string in KOI-7 encoding (NUL terminated).
// The destination buffer must be at least strlen(src)+1 bytes.
//
void utf8_to_koi7(const char *src, char *dst);

//
// Convert srclen bytes of UTF-8 text to KOI-7 and return the number of bytes written
// (never more than srclen).  Nothing is terminated: a NUL byte in the source is data
// and converts to a NUL byte in the destination.  The destination buffer must be at
// least srclen bytes.
//
size_t utf8_to_koi7_n(const char *src, size_t srclen, char *dst);
