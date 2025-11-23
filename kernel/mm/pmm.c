/*
 * AArch64 Bare Metal OS - Physical Memory Manager
 *
 * Simple buddy allocator for physical page frames
 */

#include "kernel.h"
#include "pmm.h"
#include "mmu.h"
#include "uart.h"

/* Memory zone (simplified - single zone for now) */
static struct mem_zone main_zone;

/* Page descriptors array */
static struct page pages[16384];                       /* 16K pages = 64MB worth */
static uint64_t num_pages = 0;

/*
 * Initialize physical memory manager
 */
void pmm_init(void)
{
    uint64_t i;

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Initializing Physical Memory Manager\n");
    uart_puts("========================================\n");

    /* Initialize main zone (1GB DRAM at 0x40000000) */
    main_zone.base = DRAM_BASE;
    main_zone.size = DRAM_SIZE;
    main_zone.pages = pages;
    main_zone.free_pages = 0;

    /* Calculate number of pages */
    num_pages = DRAM_SIZE / PAGE_SIZE;
    if (num_pages > 16384) {
        num_pages = 16384;                              /* Limit to our array size */
    }

    uart_puts("Memory zone:\n");
    uart_puts("  Base: ");
    uart_puthex(main_zone.base);
    uart_puts("\n");
    uart_puts("  Size: ");
    uart_puthex(main_zone.size);
    uart_puts(" (");
    uart_puthex(main_zone.size / (1024 * 1024));
    uart_puts(" MB)\n");
    uart_puts("  Pages: ");
    uart_puthex(num_pages);
    uart_puts("\n");

    /* Initialize all pages as free */
    for (i = 0; i < num_pages; i++) {
        pages[i].flags = PAGE_FREE;
        pages[i].order = 0;
        pages[i].refcount = 0;
        pages[i].next = NULL;
    }

    /* Mark kernel region as reserved */
    uart_puts("Reserving kernel memory...\n");
    pmm_mark_region(KERNEL_BASE, KERNEL_SIZE, PAGE_RESERVED);

    /* Initialize free lists */
    for (i = 0; i <= MAX_ORDER; i++) {
        main_zone.free_lists[i] = NULL;
    }

    /* Add pages to free lists (simplified - just add individual pages) */
    for (i = 0; i < num_pages; i++) {
        if (pages[i].flags == PAGE_FREE) {
            /* Add to order-0 list */
            pages[i].next = main_zone.free_lists[0];
            main_zone.free_lists[0] = &pages[i];
            main_zone.free_pages++;
        }
    }

    uart_puts("Free pages: ");
    uart_puthex(main_zone.free_pages);
    uart_puts("\n");

    uart_puts("========================================\n");
    uart_puts("PMM Initialization Complete\n");
    uart_puts("========================================\n");
    uart_puts("\n");
}

/*
 * Mark a memory region with specific flags
 */
void pmm_mark_region(uint64_t base, uint64_t size, uint32_t flags)
{
    uint64_t start_page = (base - DRAM_BASE) / PAGE_SIZE;
    uint64_t end_page = ((base + size - DRAM_BASE) + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t i;

    if (start_page >= num_pages) return;
    if (end_page > num_pages) end_page = num_pages;

    for (i = start_page; i < end_page; i++) {
        pages[i].flags = flags;
    }
}

/*
 * Allocate pages (simplified buddy allocator)
 */
struct page *alloc_pages(int order)
{
    struct page *page, *p;
    uint64_t i, count;

    if (order > MAX_ORDER) {
        return NULL;
    }

    /* For order-0, use the free list directly */
    if (order == 0) {
        page = main_zone.free_lists[0];
        if (!page) {
            return NULL;
        }

        /* Remove from free list */
        main_zone.free_lists[0] = page->next;
        page->next = NULL;
        page->flags = PAGE_USED;
        page->order = 0;
        page->refcount = 1;
        main_zone.free_pages--;

        return page;
    }

    /* For higher orders, allocate contiguous order-0 pages */
    count = 1UL << order;  /* 2^order pages */

    /* Find contiguous free pages */
    for (i = 0; i <= num_pages - count; i++) {
        /* Check if we have 'count' contiguous free pages starting at i */
        uint64_t j;
        int all_free = 1;

        for (j = 0; j < count; j++) {
            if (pages[i + j].flags != PAGE_FREE) {
                all_free = 0;
                break;
            }
        }

        if (all_free) {
            /* Found contiguous block - allocate all pages */
            for (j = 0; j < count; j++) {
                p = &pages[i + j];

                /* Remove from free list */
                if (j == 0) {
                    /* First page tracks the whole allocation */
                    struct page **prev_ptr = &main_zone.free_lists[0];
                    while (*prev_ptr && *prev_ptr != p) {
                        prev_ptr = &(*prev_ptr)->next;
                    }
                    if (*prev_ptr) {
                        *prev_ptr = p->next;
                    }
                } else {
                    /* Remove subsequent pages from free list */
                    struct page **prev_ptr = &main_zone.free_lists[0];
                    while (*prev_ptr && *prev_ptr != p) {
                        prev_ptr = &(*prev_ptr)->next;
                    }
                    if (*prev_ptr) {
                        *prev_ptr = p->next;
                    }
                }

                p->next = NULL;
                p->flags = PAGE_USED;
                p->order = (j == 0) ? order : 0;  /* Only first page tracks order */
                p->refcount = 1;
                main_zone.free_pages--;
            }

            return &pages[i];  /* Return first page */
        }
    }

    return NULL;  /* No contiguous block found */
}

/*
 * Free pages
 */
void free_pages(struct page *page, int order)
{
    uint64_t i, count;
    struct page *p;

    if (!page || order > MAX_ORDER) {
        return;
    }

    /* Decrement reference count */
    if (page->refcount > 0) {
        page->refcount--;
    }

    if (page->refcount == 0) {
        count = 1UL << order;  /* 2^order pages */

        /* Free all pages in the allocation */
        for (i = 0; i < count; i++) {
            p = page + i;

            /* Add back to order-0 free list */
            p->flags = PAGE_FREE;
            p->order = 0;
            p->next = main_zone.free_lists[0];
            main_zone.free_lists[0] = p;
            main_zone.free_pages++;
        }
    }
}

/*
 * Convert page descriptor to physical address
 */
uint64_t page_to_phys(struct page *page)
{
    uint64_t index = page - pages;
    return DRAM_BASE + (index * PAGE_SIZE);
}

/*
 * Convert physical address to page descriptor
 */
struct page *phys_to_page(uint64_t phys)
{
    if (phys < DRAM_BASE || phys >= DRAM_BASE + DRAM_SIZE) {
        return NULL;
    }

    uint64_t index = (phys - DRAM_BASE) / PAGE_SIZE;
    if (index >= num_pages) {
        return NULL;
    }

    return &pages[index];
}

/*
 * Dump PMM statistics
 */
void pmm_dump_stats(void)
{
    uart_puts("\nPhysical Memory Manager Statistics:\n");
    uart_puts("  Total pages:    ");
    uart_puthex(num_pages);
    uart_puts("\n");
    uart_puts("  Free pages:     ");
    uart_puthex(main_zone.free_pages);
    uart_puts("\n");
    uart_puts("  Used pages:     ");
    uart_puthex(num_pages - main_zone.free_pages);
    uart_puts("\n");
}
