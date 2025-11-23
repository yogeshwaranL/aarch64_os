/*
 * AArch64 Bare Metal OS - Kernel Heap Allocator
 *
 * Simple slab-style allocator for small kernel allocations
 */

#include "kernel.h"
#include "kmalloc.h"
#include "pmm.h"
#include "mmu.h"
#include "uart.h"

/* Slab caches for different sizes */
static struct slab slabs[NUM_SLABS];
static const size_t slab_sizes[NUM_SLABS] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

/* Heap statistics */
static uint64_t total_allocated = 0;
static uint64_t total_freed = 0;

/*
 * Object header for tracking allocations
 */
struct obj_header {
    size_t size;                                        /* Allocation size */
    uint32_t magic;                                     /* Magic number */
    struct obj_header *next;                            /* Free list link */
};

/*
 * Page header for large allocations (>2KB)
 */
struct page_header {
    size_t size;                                        /* Allocation size */
    uint32_t magic;                                     /* Magic number (different from obj_header) */
    uint32_t order;                                     /* Page allocation order */
};

#define KMALLOC_MAGIC   0xDEADBEEF
#define KMALLOC_PAGE_MAGIC 0xCAFEBABE

/*
 * Find appropriate slab for size
 */
static int find_slab(size_t size)
{
    int i;
    for (i = 0; i < NUM_SLABS; i++) {
        if (size <= slab_sizes[i]) {
            return i;
        }
    }
    return -1;                                          /* Too large for slabs */
}

/*
 * Initialize heap allocator
 */
void kmalloc_init(void)
{
    int i;

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Initializing Kernel Heap Allocator\n");
    uart_puts("========================================\n");

    /* Initialize all slabs */
    for (i = 0; i < NUM_SLABS; i++) {
        slabs[i].size = slab_sizes[i];
        slabs[i].free_list = NULL;
        slabs[i].total_objects = 0;
        slabs[i].free_objects = 0;

        uart_puts("  Slab ");
        uart_puthex(slab_sizes[i]);
        uart_puts(" bytes: initialized\n");
    }

    uart_puts("========================================\n");
    uart_puts("Heap Allocator Ready\n");
    uart_puts("========================================\n");
    uart_puts("\n");
}

/*
 * Allocate memory from slab
 */
static void *slab_alloc(struct slab *slab)
{
    struct obj_header *obj;
    struct page *page;
    uint64_t phys_addr, virt_addr;
    size_t objects_per_page;
    size_t obj_size = slab->size + sizeof(struct obj_header);
    int i;

    /* Try to get from free list */
    if (slab->free_list) {
        obj = slab->free_list;
        slab->free_list = obj->next;
        slab->free_objects--;
        return (void *)((uint64_t)obj + sizeof(struct obj_header));
    }

    /* Allocate new page */
    page = alloc_pages(0);
    if (!page) {
        return NULL;
    }

    /* Get physical address and map it (identity mapped) */
    phys_addr = page_to_phys(page);
    virt_addr = phys_addr;                              /* Identity mapping */

    /* Divide page into objects */
    objects_per_page = PAGE_SIZE / obj_size;

    /* Initialize free list from this page */
    for (i = 0; i < (int)objects_per_page; i++) {
        obj = (struct obj_header *)(virt_addr + i * obj_size);
        obj->size = slab->size;
        obj->magic = KMALLOC_MAGIC;
        obj->next = slab->free_list;
        slab->free_list = obj;
        slab->total_objects++;
        slab->free_objects++;
    }

    /* Now allocate from free list */
    obj = slab->free_list;
    slab->free_list = obj->next;
    slab->free_objects--;

    return (void *)((uint64_t)obj + sizeof(struct obj_header));
}

/*
 * Free memory back to slab
 */
static void slab_free(void *ptr)
{
    struct obj_header *obj;
    int slab_idx;

    if (!ptr) {
        return;
    }

    /* Get object header */
    obj = (struct obj_header *)((uint64_t)ptr - sizeof(struct obj_header));

    /* Verify magic number */
    if (obj->magic != KMALLOC_MAGIC) {
        uart_puts("WARNING: kfree() invalid pointer!\n");
        return;
    }

    /* Find slab */
    slab_idx = find_slab(obj->size);
    if (slab_idx < 0) {
        uart_puts("WARNING: kfree() size not in slab range!\n");
        return;
    }

    /* Add to free list */
    obj->next = slabs[slab_idx].free_list;
    slabs[slab_idx].free_list = obj;
    slabs[slab_idx].free_objects++;
}

/*
 * Allocate memory
 */
void *kmalloc(size_t size)
{
    int slab_idx;
    void *ptr;
    struct page_header *hdr;
    struct page *pages;
    uint32_t order;
    size_t total_size;

    if (size == 0) {
        return NULL;
    }

    /* Find appropriate slab */
    slab_idx = find_slab(size);
    if (slab_idx < 0) {
        /* Too large for slabs - use page allocator */
        total_size = size + sizeof(struct page_header);

        /* Calculate order needed (size in pages) */
        order = 0;
        while ((PAGE_SIZE << order) < total_size) {
            order++;
        }

        /* Allocate pages */
        pages = alloc_pages(order);
        if (!pages) {
            return NULL;
        }

        /* Setup header (identity mapped: virt == phys) */
        hdr = (struct page_header *)page_to_phys(pages);
        hdr->size = size;
        hdr->magic = KMALLOC_PAGE_MAGIC;
        hdr->order = order;

        total_allocated += (PAGE_SIZE << order);

        /* Return pointer after header */
        return (void *)((uint64_t)hdr + sizeof(struct page_header));
    }

    /* Allocate from slab */
    ptr = slab_alloc(&slabs[slab_idx]);
    if (ptr) {
        total_allocated += slabs[slab_idx].size;
    }

    return ptr;
}

/*
 * Allocate zeroed memory
 */
void *kzalloc(size_t size)
{
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Allocate array
 */
void *kcalloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    return kzalloc(total);
}

/*
 * Free memory
 */
void kfree(void *ptr)
{
    struct obj_header *obj;
    struct page_header *page_hdr;
    struct page *pages;

    if (!ptr) {
        return;
    }

    /* Check if this is a page-based allocation */
    page_hdr = (struct page_header *)((uint64_t)ptr - sizeof(struct page_header));
    if (page_hdr->magic == KMALLOC_PAGE_MAGIC) {
        /* Free pages (identity mapped: virt == phys) */
        total_freed += (PAGE_SIZE << page_hdr->order);
        pages = phys_to_page((uint64_t)page_hdr);
        free_pages(pages, page_hdr->order);
        return;
    }

    /* Get size from header for statistics (slab allocation) */
    obj = (struct obj_header *)((uint64_t)ptr - sizeof(struct obj_header));
    if (obj->magic == KMALLOC_MAGIC) {
        total_freed += obj->size;
    }

    slab_free(ptr);
}

/*
 * Dump heap statistics
 */
void kmalloc_dump_stats(void)
{
    int i;

    uart_puts("\nKernel Heap Statistics:\n");
    uart_puts("  Total allocated: ");
    uart_puthex(total_allocated);
    uart_puts(" bytes\n");
    uart_puts("  Total freed:     ");
    uart_puthex(total_freed);
    uart_puts(" bytes\n");
    uart_puts("  Currently used:  ");
    uart_puthex(total_allocated - total_freed);
    uart_puts(" bytes\n");

    uart_puts("\nSlab Statistics:\n");
    for (i = 0; i < NUM_SLABS; i++) {
        uart_puts("  Slab ");
        uart_puthex(slab_sizes[i]);
        uart_puts(": ");
        uart_puthex(slabs[i].total_objects);
        uart_puts(" total, ");
        uart_puthex(slabs[i].free_objects);
        uart_puts(" free\n");
    }
}
