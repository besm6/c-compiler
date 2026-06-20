/*
 * <stdlib.h> — general utilities (C11 §7.22), BESM-6 target.
 *
 * Status: exit() is implemented (Madlen helper in libc.bin); the rest are
 * declared for future implementation (TODO).
 */
#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* RAND_MAX is bounded by the signed integer ceiling (2^40-1). */
#define RAND_MAX 1099511627775L

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

/* ---- implemented in libc.bin ---- */
_Noreturn void exit(int status);

/* ---- declared for future implementation (TODO) ---- */
_Noreturn void abort(void);
int   atexit(void (*func)(void));

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

int   atoi(char *nptr);
long  atol(char *nptr);
double atof(char *nptr);
long  strtol(char *nptr, char **endptr, int base);
unsigned long strtoul(char *nptr, char **endptr, int base);
double strtod(char *nptr, char **endptr);

int   abs(int j);
long  labs(long j);
div_t  div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);

int   rand(void);
void  srand(unsigned seed);

void  qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(void *, void *));
void *bsearch(void *key, void *base, size_t nmemb, size_t size,
              int (*compar)(void *, void *));

char *getenv(char *name);
int   system(char *command);

#endif /* _STDLIB_H */
