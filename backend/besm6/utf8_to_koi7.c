#include "utf8_to_koi7.h"

#include <string.h>

//
// Convert a Unicode character (16-bit) to KOI7 encoding (8-bit).
// For details, see: https://ru.wikipedia.org/wiki/%D0%9A%D0%9E%D0%98-7#%D0%9A%D0%9E%D0%98-7_%D0%9D2
//
static unsigned char unicode_to_koi7(unsigned short val)
{
    static const unsigned char tab0[256] = {
        // clang-format off
        /* 00 - 07 */  0,    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        /* 08 - 0f */  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        /* 10 - 17 */  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        /* 18 - 1f */  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        /*  !"#$%&' */ 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        /* ()*+,-./ */ 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        /* 01234567 */ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        /* 89:;<=>? */ 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
        /* @ABCDEFG */ 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        /* HIJKLMNO */ 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
        /* PQRSTUVW */ 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        /* XYZ[\]^_ */ 0x58, 0x59, 0x5a, 0x5b, 0x1d, 0x5d, 0x5c, 0x5f,
        /* `abcdefg */ 0,    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, //  ABCDEFG
        /* hijklmno */ 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, // HIJKLMNO
        /* pqrstuvw */ 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, // PQRSTUVW
        /* xyz{|}~  */ 0x58, 0x59, 0x5a, 0x0e, 0x5e, 0x0f, 0x1f, 0,    // XYZ≤|≥¬
        /* 80 - 87 */  0,    0,    0,    0,    0,    0,    0,    0,
        /* 88 - 8f */  0,    0,    0,    0,    0,    0,    0,    0,
        /* 90 - 97 */  0,    0,    0,    0,    0,    0,    0,    0,
        /* 98 - 9f */  0,    0,    0,    0,    0,    0,    0,    0,
        /* a0 - a7 */  0,    0,    0,    0,    0,    0,    0,    0,
        /* a8 - af */  0,    0,    0,    0,    0x1f, 0,    0,    0, // ¬
        /* b0 - b7 */  0x19, 0,    0,    0,    0,    0,    0,    0, // °
        /* b8 - bf */  0,    0,    0,    0,    0,    0,    0,    0,
        /* c0 - c7 */  0,    0,    0,    0,    0,    0,    0,    0,
        /* c8 - cf */  0,    0,    0,    0,    0,    0,    0,    0,
        /* d0 - d7 */  0,    0,    0,    0,    0,    0,    0,    0x06, // ×
        /* d8 - df */  0,    0,    0,    0,    0,    0,    0,    0,
        /* e0 - e7 */  0,    0,    0,    0,    0,    0,    0,    0,
        /* e8 - ef */  0,    0,    0,    0,    0,    0,    0,    0,
        /* f0 - f7 */  0,    0,    0,    0,    0,    0,    0,    0x1a, // ÷
        /* f8 - ff */  0,    0,    0,    0,    0,    0,    0,    0,
        // clang-format on
    };
    switch (val >> 8) {
    case 0x00:
        return tab0[val];
    case 0x04:
        switch ((unsigned char)val) {
        case 0x01:
            return 'E'; // Ë
        case 0x04:
            return 'E'; // Ukrainian Є
        case 0x06:
            return 'I'; // Ukrainian І
        case 0x07:
            return 'I'; // Ukrainian Ї
        case 0x10:
            return 'A'; // А
        case 0x11:
            return 0x62; // Б
        case 0x12:
            return 'B'; // В
        case 0x13:
            return 0x67; // Г
        case 0x14:
            return 0x64; // Д
        case 0x15:
            return 'E'; // Е
        case 0x16:
            return 0x76; // Ж
        case 0x17:
            return 0x7a; // З
        case 0x18:
            return 0x69; // И
        case 0x19:
            return 0x6a; // Й
        case 0x1a:
            return 'K'; // К
        case 0x1b:
            return 0x6c; // Л
        case 0x1c:
            return 'M'; // М
        case 0x1d:
            return 'H'; // Н
        case 0x1e:
            return 'O'; // О
        case 0x1f:
            return 0x70; // П
        case 0x20:
            return 'P'; // Р
        case 0x21:
            return 'C'; // С
        case 0x22:
            return 'T'; // Т
        case 0x23:
            return 'Y'; // У
        case 0x24:
            return 0x66; // Ф
        case 0x25:
            return 'X'; // Х
        case 0x26:
            return 0x63; // Ц
        case 0x27:
            return 0x7e; // Ч
        case 0x28:
            return 0x7b; // Ш
        case 0x29:
            return 0x7d; // Щ
        case 0x2a:
            return 0x05; // Ъ
        case 0x2b:
            return 0x79; // Ы
        case 0x2c:
            return 0x78; // Ь
        case 0x2d:
            return 0x7c; // Э
        case 0x2e:
            return 0x60; // Ю
        case 0x2f:
            return 0x71; // Я
        case 0x30:
            return 'A'; // а
        case 0x31:
            return 0x62; // б
        case 0x32:
            return 'B'; // в
        case 0x33:
            return 0x67; // г
        case 0x34:
            return 0x64; // д
        case 0x35:
            return 'E'; // е
        case 0x36:
            return 0x76; // ж
        case 0x37:
            return 0x7a; // з
        case 0x38:
            return 0x69; // и
        case 0x39:
            return 0x6a; // й
        case 0x3a:
            return 'K'; // к
        case 0x3b:
            return 0x6c; // л
        case 0x3c:
            return 'M'; // м
        case 0x3d:
            return 'H'; // н
        case 0x3e:
            return 'O'; // о
        case 0x3f:
            return 0x70; // п
        case 0x40:
            return 'P'; // р
        case 0x41:
            return 'C'; // с
        case 0x42:
            return 'T'; // т
        case 0x43:
            return 'Y'; // у
        case 0x44:
            return 0x66; // ф
        case 0x45:
            return 'X'; // х
        case 0x46:
            return 0x63; // ц
        case 0x47:
            return 0x7e; // ч
        case 0x48:
            return 0x7b; // ш
        case 0x49:
            return 0x7d; // щ
        case 0x4a:
            return 0x05; // ъ
        case 0x4b:
            return 0x79; // ы
        case 0x4c:
            return 0x78; // ь
        case 0x4d:
            return 0x7c; // э
        case 0x4e:
            return 0x60; // ю
        case 0x4f:
            return 0x71; // я
        case 0x51:
            return 'E'; // ё
        case 0x54:
            return 'E'; // Ukrainian є
        case 0x56:
            return 'I'; // Ukrainian і
        case 0x57:
            return 'I'; // Ukrainian ї
        case 0x90:
            return 0x67; // Ukrainian Ґ
        case 0x91:
            return 0x67; // Ukrainian ґ
        }
        break;
    case 0x20:
        switch ((unsigned char)val) {
        case 0x15:
            return '\25'; // ―
        case 0x18:
            return '\20'; // '
        case 0x19:
            return '\33'; // '
        case 0x28:
            return 0x0a;
        case 0x32:
            return '\'';
        case 0x3e:
            return '\\';
        }
        break;
    case 0x21:
        switch ((unsigned char)val) {
        case 0x2f:
            return 'E';
        case 0x91:
            return '\26'; // ↑
        }
        break;
    case 0x22:
        switch ((unsigned char)val) {
        case 0x27:
            return '&'; // ∧
        case 0x28:
            return '\36'; // ∨
        case 0x60:
            return '\30'; // ≠
        case 0x61:
            return '\35'; // ≡
        case 0x64:
            return '\16'; // ≤
        case 0x65:
            return '\17'; // ≥
        case 0x83:
            return '\34'; // ⊃
        }
        break;
    case 0x23:
        switch ((unsigned char)val) {
        case 0xe8:
            return '\27'; // ⏨
        }
        break;
    case 0x25:
        switch ((unsigned char)val) {
        case 0xc7:
            return '$';
        }
        break;
    }
    return 0;
}

//
// Convert srclen bytes of UTF-8 source text to KOI-7, and return the number of bytes
// written.  The length is explicit because string-literal data may hold embedded NUL
// bytes, which are ordinary data here (KOI-7 maps 0 to 0) and must not end the string.
// A multi-byte sequence cut short by the end of the input is dropped.
//
size_t utf8_to_koi7_n(const char *src, size_t srclen, char *dst)
{
    const char *out = dst;
    for (size_t i = 0; i < srclen;) {
        unsigned char a = (unsigned char)src[i++];
        if (!(a & 0x80)) {
            // Single-byte ASCII character.
            *dst++ = unicode_to_koi7(a);
            continue;
        }

        if (i >= srclen) {
            // Incomplete sequence at end of string.
            break;
        }
        unsigned char b = (unsigned char)src[i++];
        if (!(a & 0x20)) {
            // Two-byte sequence: 10-bit codepoint.
            *dst++ = unicode_to_koi7((unsigned short)((a & 0x1f) << 6 | (b & 0x3f)));
            continue;
        }

        if (i >= srclen) {
            // Incomplete sequence at end of string.
            break;
        }
        unsigned char c = (unsigned char)src[i++];
        // Three-byte sequence: 16-bit codepoint.
        *dst++ = unicode_to_koi7((unsigned short)((a & 0x0f) << 12 | (b & 0x3f) << 6 | (c & 0x3f)));
    }
    return (size_t)(dst - out);
}

//
// Convert a source string in UTF-8 encoding (NUL terminated)
// to a destination string in KOI-7 encoding (NUL terminated).
//
void utf8_to_koi7(const char *src, char *dst)
{
    dst[utf8_to_koi7_n(src, strlen(src), dst)] = 0;
}
