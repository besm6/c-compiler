//
// Memory allocation.
//
// Copyright (c) 2025 Serge Vakulenko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xalloc.h"

int xalloc_debug; // Enable manually for debug

//
// Structure for the header of each allocated block
//
typedef struct BlockHeader {
    struct BlockHeader *next;
    struct BlockHeader *prev;
    size_t requested_size;
    const char *funcname;
    const char *filename;
    unsigned lineno;
} BlockHeader;

//
// Global pointer to the head of the doubly linked list
//
static BlockHeader *head = NULL;

//
// Allocates memory like malloc, but adds a header and maintains a doubly linked list
//
void *xalloc(size_t size, const char *funcname, const char *filename, unsigned lineno)
{
    /* Calculate total size: header + requested size */
    size_t total_size = sizeof(BlockHeader) + size;

    /* Allocate memory using malloc */
    void *ptr = calloc(1, total_size);
    if (ptr == NULL) {
        fprintf(stderr, "Out of memory allocating %zu bytes by %s() at file %s, line %u\n",
                size, funcname, filename, lineno);
        exit(1);
    }

    /* Set up the header */
    BlockHeader *h    = (BlockHeader *)ptr;
    h->requested_size = size;
    h->funcname       = funcname;
    h->filename       = filename;
    h->lineno         = lineno;

    /* Insert into the doubly linked list */
    if (head == NULL) {
        /* First allocation */
        head = h;
    } else {
        /* Insert at the head of the list */
        h->next = head;
        head->prev   = h;
        head         = h;
    }

    /* Return pointer to user data (after header) */
    ptr = (void *)((char *)ptr + sizeof(BlockHeader));
    if (xalloc_debug) {
        printf("--- %s %zu bytes, %p\n", __func__, size, ptr);
    }
    return ptr;
}

//
// Frees memory like free, but removes the block from the doubly linked list
//
void xfree(void *ptr)
{
    if (ptr == NULL) {
        return; /* Nothing to free */
    }

    /* Get the header (before the user pointer) */
    BlockHeader *h = (BlockHeader *)((char *)ptr - sizeof(BlockHeader));
    if (xalloc_debug) {
        printf("--- %s %zu bytes, %p\n", __func__, h->requested_size, ptr);
    }

    /* Remove from the doubly linked list */
    if (h->prev != NULL) {
        if (h->prev->next != h) {
            fprintf(stderr, "Damaged memory list in xfree()\n");
            exit(1);
        }
        h->prev->next = h->next;
    } else {
        /* This is the head */
        if (head != h) {
            fprintf(stderr, "Damaged memory head in xfree()\n");
            exit(1);
        }
        head = h->next;
    }

    if (h->next != NULL) {
        if (h->next->prev != h) {
            fprintf(stderr, "Damaged memory list in xfree().\n");
            exit(1);
        }
        h->next->prev = h->prev;
    }
    // Just in case.
    h->next = NULL;
    h->prev = NULL;

    /* Free the entire block (header + user data) */
    free(h);
}

void xreport_lost_memory()
{
    if (head) {
        printf("Lost memory:\n");
    }
    for (BlockHeader *h = head; h; h = h->next) {
        const char *filename = strrchr(h->filename, '/');
        if (!filename)
            filename = h->filename;
        else
            filename++;
        printf("%zu bytes allocated by %s() at line %u of file %s\n",
               h->requested_size, h->funcname, h->lineno, filename);
    }
}

size_t xtotal_allocated_size()
{
    size_t total = 0;
    for (BlockHeader *h = head; h; h = h->next) {
        total += h->requested_size;
    }
    return total;
}

void xfree_all()
{
    if (xalloc_debug) {
        printf("--- %s\n", __func__);
    }
    while (head) {
        BlockHeader *next = head->next;
        free(head);
        head = next;
    }
}

/* Helper to duplicate a string */
char *xstrdup(const char *str)
{
    if (!str)
        return NULL;
    char *new_str = xalloc(strlen(str) + 1, __func__, __FILE__, __LINE__);
    if (new_str)
        strcpy(new_str, str);
    return new_str;
}
