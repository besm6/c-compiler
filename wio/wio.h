//
// Word-oriented buffered I/O interface.
//
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// WFILE structure: Contains a file descriptor (`fd`), a buffer
// for `size_t` words, buffer position and count, EOF and error flags,
// and mode (`r`, `w`, or `a`).
//
struct _wfile {
    int fd;              /* Underlying file descriptor */
    size_t *buffer;      /* Buffer for read/write operations */
    size_t buffer_pos;   /* Current position in buffer */
    size_t buffer_count; /* Number of words in buffer */
    bool is_eof;         /* End-of-file flag */
    bool is_error;       /* Error flag */
    char mode;           /* 'r' for read, 'w' for write, 'a' for append */
};
typedef struct _wfile WFILE;

WFILE *wopen(const char *path, const char * mode);
WFILE *wreopen(const char *path, const char *mode, WFILE *stream);
WFILE *wdopen(int fildes, const char *mode);
void wclose(WFILE *stream);
int wflush(WFILE *stream);
int wseek(WFILE *stream, long offset, int whence);
long wtell(WFILE *stream);
void wrewind(WFILE *stream);
size_t wgetw(WFILE *stream);
int wputw(size_t w, WFILE *stream);
bool weof(WFILE *stream);
bool werror(WFILE *stream);
int wfileno(WFILE *stream);
void wclearerr(WFILE *stream);
char *wgetstr(WFILE *stream); // dynamically allocated
int wputstr(const char *str, WFILE *stream);

#ifdef __cplusplus
}
#endif
