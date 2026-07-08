/*
 * <malloc.h> — heap-management extras for the BESM-6 C runtime.
 *
 * The standard allocation API (malloc/calloc/realloc/free) is declared in
 * <stdlib.h>.  This header exposes the non-standard helpers used to query the
 * allocator.
 *
 * BESM-6 has no sbrk/brk: the heap is provisioned automatically over the fixed
 * span between the linker `_end` symbol and the stack base, claimed on the first
 * allocation — the program need not (and cannot) supply a backing region.  This
 * relies on the Unix (b6ld/b6sim) memory map, so the allocator is unavailable in
 * the Madlen (Dubna) runtime.  All sizes here are counted in bytes.
 */
#ifndef _MALLOC_H
#define _MALLOC_H

#include <stddef.h>

/*
 * Total free memory currently available in the heap, in bytes.
 */
size_t malloc_free_bytes(void);

/*
 * Usable payload size, in bytes, of a block previously returned by malloc().
 */
size_t malloc_usable_size(void *ptr);

#endif /* _MALLOC_H */
