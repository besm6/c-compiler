//
// Convert a source string in UTF-8 encoding (NUL terminated)
// to a destination string in KOI-7 encoding (NUL terminated).
// The destination buffer must be at least strlen(src)+1 bytes.
//
void utf8_to_koi7(const char *src, char *dst);
