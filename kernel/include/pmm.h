/*
 * AArch64 Bare Metal OS - Physical Memory Manager
 *
 * Buddy allocator for physical page frames
 */

#ifndef _PMM_H
#define _PMM_H

#include "kernel.h"
#include "mmu.h"

/* Maximum order for buddy allocator (2^MAX_ORDER pages) */
#define MAX_ORDER           10                          /* Up to 4MB blocks */

/* Page frame states */
#define PAGE_FREE           0
#define PAGE_USED           1
#define PAGE_RESERVED       2

/* Page frame descriptor */
struct page {
    uint32_t flags;                                     /* Page flags */
    uint32_t order;                                     /* Buddy allocator order */
    uint32_t refcount;                                  /* Reference count */
    struct page *next;                                  /* Free list link */
};

/* Physical memory zones */
struct mem_zone {
    uint64_t base;                                      /* Base physical address */
    uint64_t size;                                      /* Size in bytes */
    struct page *pages;                                 /* Page descriptors */
    struct page *free_lists[MAX_ORDER + 1];             /* Free lists by order */
    uint64_t free_pages;                                /* Total free pages */
};

/* Function prototypes */
void pmm_init(void);
void pmm_mark_region(uint64_t base, uint64_t size, uint32_t flags);

struct page *alloc_pages(int order);
void free_pages(struct page *page, int order);

uint64_t page_to_phys(struct page *page);
struct page *phys_to_page(uint64_t phys);

void pmm_dump_stats(void);

#endif /* _PMM_H */
