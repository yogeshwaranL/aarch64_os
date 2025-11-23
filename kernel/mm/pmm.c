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
    struct page *page;

    if (order > MAX_ORDER || order != 0) {
        /* For Phase 5, only support order-0 allocations */
        return NULL;
    }

    /* Get page from free list */
    page = main_zone.free_lists[order];
    if (!page) {
        return NULL;
    }

    /* Remove from free list */
    main_zone.free_lists[order] = page->next;
    page->next = NULL;
    page->flags = PAGE_USED;
    page->order = order;
    page->refcount = 1;
    main_zone.free_pages--;

    return page;
}

/*
 * Free pages
 */
void free_pages(struct page *page, int order)
{
    if (!page || order > MAX_ORDER) {
        return;
    }

    /* Decrement reference count */
    if (page->refcount > 0) {
        page->refcount--;
    }

    if (page->refcount == 0) {
        /* Add back to free list */
        page->flags = PAGE_FREE;
        page->order = order;
        page->next = main_zone.free_lists[order];
        main_zone.free_lists[order] = page;
        main_zone.free_pages++;
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
