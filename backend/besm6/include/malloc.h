/*
 * <malloc.h> — heap-management extras for the BESM-6 C runtime.
 *
 * The standard allocation API (malloc/calloc/realloc/free) is declared in
 * <stdlib.h>.  This header exposes the non-standard helpers used to give the
 * allocator a backing region and to query it.
 *
 * BESM-6 has no sbrk/brk and no "end of program" symbol, so the program must
 * donate memory to the heap with heap_setup() before the first allocation.
 * All sizes here are counted in 48-bit machine words (one word = 6 bytes).
 */
#ifndef _MALLOC_H
#define _MALLOC_H

#include <stddef.h>

/*
 * Donate a region of NWORDS machine words starting at START to the heap.
 * May be called more than once to add disjoint regions.  Must run before the
 * first malloc(); until a region is donated every allocation returns NULL.
 */
void heap_setup(void *start, size_t nwords);

/* Total free memory currently available in the heap, in bytes. */
size_t heap_available(void);

/* Usable payload size, in bytes, of a block previously returned by malloc(). */
size_t malloc_usable_size(void *ptr);

#endif /* _MALLOC_H */
