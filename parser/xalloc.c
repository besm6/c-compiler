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

/* Structure for the header of each allocated block */
typedef struct BlockHeader {
    struct BlockHeader *next;
    struct BlockHeader *prev;
} BlockHeader;

/* Global pointer to the head of the doubly linked list */
static BlockHeader *head = NULL;

/* Allocates memory like malloc, but adds a header and maintains a doubly linked list */
void *xmalloc(size_t size)
{
    /* Calculate total size: header + requested size */
    size_t total_size = sizeof(BlockHeader) + size;

    /* Allocate memory using malloc */
    void *ptr = malloc(total_size);
    if (ptr == NULL) {
        return NULL; /* malloc failed */
    }

    /* Set up the header */
    BlockHeader *header = (BlockHeader *)ptr;
    header->next        = NULL;
    header->prev        = NULL;

    /* Insert into the doubly linked list */
    if (head == NULL) {
        /* First allocation */
        head = header;
    } else {
        /* Insert at the head of the list */
        header->next = head;
        head->prev   = header;
        head         = header;
    }

    /* Return pointer to user data (after header) */
    return (void *)((char *)ptr + sizeof(BlockHeader));
}

/* Frees memory like free, but removes the block from the doubly linked list */
void xfree(void *ptr)
{
    if (ptr == NULL) {
        return; /* Nothing to free */
    }

    /* Get the header (before the user pointer) */
    BlockHeader *header = (BlockHeader *)((char *)ptr - sizeof(BlockHeader));

    /* Remove from the doubly linked list */
    if (header->prev != NULL) {
        header->prev->next = header->next;
    } else {
        /* This is the head */
        head = header->next;
    }

    if (header->next != NULL) {
        header->next->prev = header->prev;
    }

    /* Free the entire block (header + user data) */
    free(header);
}
