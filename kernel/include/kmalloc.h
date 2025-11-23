/*
 * AArch64 Bare Metal OS - Kernel Heap Allocator
 *
 * Simple slab-like allocator for kernel heap
 */

#ifndef _KMALLOC_H
#define _KMALLOC_H

#include "kernel.h"

/* Allocation flags */
#define KMALLOC_ZERO        (1 << 0)                    /* Zero memory */

/* Slab sizes (power of 2) */
#define SLAB_SIZE_MIN       16
#define SLAB_SIZE_MAX       2048
#define NUM_SLABS           8                           /* 16, 32, 64, 128, 256, 512, 1024, 2048 */

/* Slab structure */
struct slab {
    size_t size;                                        /* Object size */
    void *free_list;                                    /* Free object list */
    uint64_t total_objects;                             /* Total objects */
    uint64_t free_objects;                              /* Free objects */
};

/* Function prototypes */
void kmalloc_init(void);

void *kmalloc(size_t size);
void *kzalloc(size_t size);                             /* Zeroed allocation */
void *kcalloc(size_t nmemb, size_t size);               /* Calloc-style */
void kfree(void *ptr);

void kmalloc_dump_stats(void);

#endif /* _KMALLOC_H */
