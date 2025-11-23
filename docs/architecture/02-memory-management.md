# Memory Management

## AArch64 Memory Management Subsystem

**Version:** 0.1.0
**Component:** Memory Management (Physical & Virtual)
**Implementation Phase:** Phase 5

---

## Table of Contents

1. [Overview](#overview)
2. [Physical Memory Management](#physical-memory-management)
3. [Virtual Memory Management](#virtual-memory-management)
4. [Page Table Management](#page-table-management)
5. [Heap Allocator](#heap-allocator)
6. [Memory Zones](#memory-zones)
7. [DMA and Device Memory](#dma-and-device-memory)

---

## Overview

The memory management subsystem provides:

- **Physical Memory Allocator**: Buddy allocator for page-granularity allocation
- **Virtual Memory Manager**: Per-process address spaces with MMU support
- **Page Tables**: 4-level translation tables (48-bit VA)
- **Kernel Heap**: Slab allocator for kernel objects
- **Memory Zones**: DMA, Normal, and High memory
- **Memory Attributes**: Caching, permissions, shareability

### ARMv8-A Memory Model

**Key Features**:
- **48-bit Virtual Addresses** (configurable to 39/42/52-bit)
- **48-bit Physical Addresses** (configurable)
- **4KB page granule** (also supports 16KB, 64KB)
- **4-level page tables** (for 4KB + 48-bit VA)
- **Stage-1 translation** (VA → IPA/PA in kernel/EL1)
- **Stage-2 translation** (IPA → PA in hypervisor/EL2)

---

## Physical Memory Management

### Buddy Allocator

**Design**: Binary buddy system for efficient allocation of physically contiguous pages.

```
Order  Size       Use Case
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  0    4 KB       Single page allocation
  1    8 KB       Small contiguous allocations
  2    16 KB
  3    32 KB
  4    64 KB
  5    128 KB
  6    256 KB
  7    512 KB
  8    1 MB
  9    2 MB       Large contiguous buffers
 10    4 MB
```

#### Data Structures

```c
/**
 * @brief Page frame descriptor
 */
struct page {
    unsigned long flags;          // Page flags (locked, dirty, etc.)
    atomic_t refcount;            // Reference count
    struct list_head lru;         // LRU list linkage
    void *virtual;                // Virtual address (if mapped)
    unsigned int order;           // Buddy allocator order
};

/**
 * @brief Free area for each order in buddy allocator
 */
struct free_area {
    struct list_head free_list;   // List of free blocks
    unsigned long nr_free;        // Number of free blocks
};

/**
 * @brief Memory zone structure
 */
struct zone {
    unsigned long start_pfn;      // Start page frame number
    unsigned long end_pfn;        // End page frame number
    struct free_area free_area[MAX_ORDER];
    spinlock_t lock;              // Protects zone data
    unsigned long pages_free;     // Total free pages
    unsigned long pages_total;    // Total pages in zone
};
```

#### Allocation Algorithm

```c
/**
 * @brief Allocate 2^order contiguous pages
 * @param zone Memory zone to allocate from
 * @param order Order (0 = 4KB, 1 = 8KB, ..., 10 = 4MB)
 * @return Physical address of allocated memory, or 0 on failure
 */
unsigned long buddy_alloc_pages(struct zone *zone, unsigned int order)
{
    unsigned int current_order;
    struct page *page;

    // Find a free block at requested order or higher
    for (current_order = order; current_order < MAX_ORDER; current_order++) {
        if (!list_empty(&zone->free_area[current_order].free_list)) {
            goto found;
        }
    }

    // No free blocks found
    return 0;

found:
    // Remove block from free list
    page = list_first_entry(&zone->free_area[current_order].free_list,
                            struct page, lru);
    list_del(&page->lru);
    zone->free_area[current_order].nr_free--;

    // Split larger blocks if necessary
    while (current_order > order) {
        current_order--;

        // Get buddy page
        struct page *buddy = page + (1 << current_order);

        // Add buddy to free list at lower order
        list_add(&buddy->lru, &zone->free_area[current_order].free_list);
        zone->free_area[current_order].nr_free++;
        buddy->order = current_order;
    }

    page->order = order;
    zone->pages_free -= (1 << order);

    return page_to_phys(page);
}

/**
 * @brief Free pages back to buddy allocator
 */
void buddy_free_pages(struct zone *zone, unsigned long addr, unsigned int order)
{
    struct page *page = phys_to_page(addr);
    unsigned int current_order = order;

    // Coalesce with buddies
    while (current_order < MAX_ORDER - 1) {
        unsigned long buddy_pfn = __find_buddy_pfn(page_to_pfn(page), current_order);
        struct page *buddy = pfn_to_page(buddy_pfn);

        // Check if buddy is free
        if (!page_is_buddy(page, buddy, current_order))
            break;

        // Remove buddy from free list
        list_del(&buddy->lru);
        zone->free_area[current_order].nr_free--;

        // Merge with buddy
        if (page_to_pfn(page) > buddy_pfn)
            page = buddy;

        current_order++;
    }

    // Add merged block to free list
    page->order = current_order;
    list_add(&page->lru, &zone->free_area[current_order].free_list);
    zone->free_area[current_order].nr_free++;
    zone->pages_free += (1 << order);
}
```

---

## Virtual Memory Management

### Address Space Layout

#### Kernel Address Space

```
Virtual Address         Description                  Size
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
0xFFFF_FFFF_FFFF_FFFF  ┌──────────────────────────┐
                       │ Kernel Code & Data       │  2 GB
0xFFFF_FFFF_8000_0000  ├──────────────────────────┤
                       │ Kernel Modules           │  1 GB
0xFFFF_FFFF_4000_0000  ├──────────────────────────┤
                       │ (Guard Page)             │
0xFFFF_FFFF_3FFF_FFFF  ├──────────────────────────┤
                       │ Kernel Heap (Slab)       │  64 GB
0xFFFF_FF00_0000_0000  ├──────────────────────────┤
                       │ vmalloc Area             │  32 GB
0xFFFF_FC00_0000_0000  ├──────────────────────────┤
                       │ Device Mappings (MMIO)   │  16 GB
0xFFFF_F800_0000_0000  ├──────────────────────────┤
                       │ Direct Physical Mapping  │  128 GB
0xFFFF_F000_0000_0000  ├──────────────────────────┤
                       │ (Reserved)               │
0xFFFF_0000_0000_0000  └──────────────────────────┘
```

#### User Address Space

```
Virtual Address         Description
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
0x0000_FFFF_FFFF_FFFF  ┌──────────────────────────┐
                       │ (Reserved - Guard)       │
0x0000_FFFF_0000_0000  ├──────────────────────────┤
                       │ Stack (grows down)       │
                       │         ↓                │
0x0000_F000_0000_0000  ├──────────────────────────┤
                       │                          │
                       │ Memory Mappings (mmap)   │
                       │                          │
0x0000_8000_0000_0000  ├──────────────────────────┤
                       │         ↑                │
                       │ Heap (grows up)          │
0x0000_0040_0000_0000  ├──────────────────────────┤
                       │ BSS (uninitialized data) │
0x0000_0030_0000_0000  ├──────────────────────────┤
                       │ Data (initialized data)  │
0x0000_0020_0000_0000  ├──────────────────────────┤
                       │ Text (code)              │
0x0000_0000_0040_0000  ├──────────────────────────┤
                       │ (NULL guard - unmapped)  │
0x0000_0000_0000_0000  └──────────────────────────┘
```

### Address Space Structure

```c
/**
 * @brief Virtual memory area descriptor
 */
struct vm_area_struct {
    unsigned long vm_start;       // Start virtual address
    unsigned long vm_end;         // End virtual address
    unsigned long vm_flags;       // Permissions (R/W/X)
    struct vm_area_struct *vm_next;
    struct mm_struct *vm_mm;      // Back pointer to mm_struct
    pgprot_t vm_page_prot;        // Page protection
};

/**
 * @brief Memory descriptor (per-process)
 */
struct mm_struct {
    pgd_t *pgd;                   // Page global directory
    struct vm_area_struct *mmap;  // List of VMAs
    unsigned long start_code;     // Start of code section
    unsigned long end_code;       // End of code section
    unsigned long start_data;     // Start of data section
    unsigned long end_data;       // End of data section
    unsigned long start_brk;      // Start of heap
    unsigned long brk;            // Current heap end
    unsigned long start_stack;    // Start of stack
    unsigned long total_vm;       // Total mapped pages
    atomic_t mm_users;            // User count
    atomic_t mm_count;            // Reference count
    spinlock_t page_table_lock;   // Protects page tables
};
```

---

## Page Table Management

### ARMv8-A 4-Level Page Tables

For **4KB granule** with **48-bit VA**:

```
Level  Table Name              Bits   Entries  Coverage
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  0    PGD (Page Global Dir)   [47:39]   512    512 GB each
  1    PUD (Page Upper Dir)    [38:30]   512    1 GB each
  2    PMD (Page Middle Dir)   [29:21]   512    2 MB each
  3    PTE (Page Table Entry)  [20:12]   512    4 KB each
       Page Offset              [11:0]          4 KB
```

#### Page Table Entry Format

```
 63  62  59 58  55 54  52 51  48 47      12 11   10 9  8 7  6 5  2  1  0
┌───┬─────┬─────┬─────┬─────┬───────────┬──────┬───┬───┬───┬───┬────┬───┐
│SW │Rsvd │ UXN │ PXN │Cont │  Address  │ nG │AF│SH │AP │NS│Idx │Type│ V │
└───┴─────┴─────┴─────┴─────┴───────────┴────┴──┴───┴───┴──┴────┴────┴───┘

V     [0]       Valid bit
Type  [1]       Descriptor type (0=block, 1=table/page)
Idx   [4:2]     AttrIndx (index into MAIR_EL1)
NS    [5]       Non-secure (EL3/Secure only)
AP    [7:6]     Access permissions (00=RW_EL1, 01=RW_ALL, 10=RO_EL1, 11=RO_ALL)
SH    [9:8]     Shareability (00=Non, 10=Outer, 11=Inner)
AF    [10]      Access flag (must be 1)
nG    [11]      Not global (0=global, 1=process-specific)
Addr  [47:12]   Physical address (or next level table address)
Cont  [52]      Contiguous hint
PXN   [53]      Privileged Execute Never
UXN   [54]      User Execute Never
SW    [58:55]   Software use
```

#### Page Table API

```c
/**
 * @brief Map a virtual address to physical address
 * @param mm Memory descriptor
 * @param va Virtual address
 * @param pa Physical address
 * @param size Size to map
 * @param prot Protection flags
 * @return 0 on success, negative on error
 */
int vm_map_pages(struct mm_struct *mm, unsigned long va,
                 unsigned long pa, size_t size, pgprot_t prot)
{
    unsigned long addr = va;
    unsigned long end = va + size;
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    while (addr < end) {
        // Traverse page table hierarchy
        pgd = pgd_offset(mm, addr);
        if (pgd_none(*pgd)) {
            // Allocate PUD
            pud = pud_alloc(mm, pgd, addr);
            if (!pud)
                return -ENOMEM;
        } else {
            pud = pud_offset(pgd, addr);
        }

        if (pud_none(*pud)) {
            // Allocate PMD
            pmd = pmd_alloc(mm, pud, addr);
            if (!pmd)
                return -ENOMEM;
        } else {
            pmd = pmd_offset(pud, addr);
        }

        if (pmd_none(*pmd)) {
            // Allocate PTE
            pte = pte_alloc(mm, pmd, addr);
            if (!pte)
                return -ENOMEM;
        } else {
            pte = pte_offset(pmd, addr);
        }

        // Set PTE
        set_pte(pte, pfn_pte(pa >> PAGE_SHIFT, prot));

        addr += PAGE_SIZE;
        pa += PAGE_SIZE;
    }

    // Flush TLB
    flush_tlb_range(mm, va, end);

    return 0;
}
```

### Memory Attributes (MAIR_EL1)

```c
/*
 * Memory Attribute Indirection Register configuration
 * Defines 8 memory attribute configurations (Attr0-Attr7)
 */
#define MAIR_DEVICE_nGnRnE  0x00  // Device, non-Gathering, non-Reordering, no Early-ack
#define MAIR_DEVICE_nGnRE   0x04  // Device, non-Gathering, non-Reordering, Early-ack
#define MAIR_NORMAL_NC      0x44  // Normal, Non-cacheable
#define MAIR_NORMAL_WT      0xBB  // Normal, Write-through
#define MAIR_NORMAL_WB      0xFF  // Normal, Write-back

// Typical MAIR_EL1 configuration
#define MAIR_VALUE  (MAIR_DEVICE_nGnRnE << (0 * 8)) | \
                    (MAIR_DEVICE_nGnRE  << (1 * 8)) | \
                    (MAIR_NORMAL_NC     << (2 * 8)) | \
                    (MAIR_NORMAL_WT     << (3 * 8)) | \
                    (MAIR_NORMAL_WB     << (4 * 8))
```

---

## Heap Allocator

### Slab Allocator

**Design**: Object caching for frequently allocated kernel structures.

```c
/**
 * @brief Slab descriptor
 */
struct slab {
    struct list_head list;        // List linkage
    void *mem;                    // Memory for objects
    unsigned long inuse;          // Number of allocated objects
    void *freelist;               // Free object list
};

/**
 * @brief Cache descriptor
 */
struct kmem_cache {
    const char *name;             // Cache name
    size_t size;                  // Object size
    size_t align;                 // Alignment
    unsigned long flags;          // Cache flags
    struct list_head slabs_full;  // Full slabs
    struct list_head slabs_partial; // Partially used slabs
    struct list_head slabs_free;  // Empty slabs
    spinlock_t lock;              // Protects cache
    void (*ctor)(void *);         // Constructor
};

/**
 * @brief Allocate object from cache
 */
void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags)
{
    struct slab *slab;
    void *obj;

    spin_lock(&cache->lock);

    // Try partial slabs first
    if (!list_empty(&cache->slabs_partial)) {
        slab = list_first_entry(&cache->slabs_partial, struct slab, list);
    }
    // Then try free slabs
    else if (!list_empty(&cache->slabs_free)) {
        slab = list_first_entry(&cache->slabs_free, struct slab, list);
        list_move(&slab->list, &cache->slabs_partial);
    }
    // Allocate new slab
    else {
        spin_unlock(&cache->lock);
        slab = kmem_cache_grow(cache, flags);
        if (!slab)
            return NULL;
        spin_lock(&cache->lock);
    }

    // Get object from freelist
    obj = slab->freelist;
    slab->freelist = *(void **)obj;
    slab->inuse++;

    // Move to full list if needed
    if (slab->freelist == NULL) {
        list_move(&slab->list, &cache->slabs_full);
    }

    spin_unlock(&cache->lock);

    // Call constructor
    if (cache->ctor)
        cache->ctor(obj);

    return obj;
}
```

### General Purpose Allocators

```c
/**
 * @brief General kernel memory allocation (like malloc)
 */
void *kmalloc(size_t size, gfp_t flags);

/**
 * @brief Free kernel memory
 */
void kfree(const void *ptr);

/**
 * @brief Allocate virtually contiguous memory
 */
void *vmalloc(unsigned long size);

/**
 * @brief Free vmalloc memory
 */
void vfree(const void *addr);
```

---

## Memory Zones

```c
enum zone_type {
    ZONE_DMA,      // DMA-capable memory (0-4GB for most devices)
    ZONE_NORMAL,   // Normal memory
    ZONE_HIGHMEM,  // High memory (if physical > virtual address space)
    MAX_NR_ZONES
};

extern struct zone zones[MAX_NR_ZONES];
```

---

## DMA and Device Memory

### DMA Mapping

```c
/**
 * @brief Allocate DMA-capable memory
 */
void *dma_alloc_coherent(struct device *dev, size_t size,
                         dma_addr_t *dma_handle, gfp_t flags);

/**
 * @brief Free DMA memory
 */
void dma_free_coherent(struct device *dev, size_t size,
                       void *cpu_addr, dma_addr_t dma_handle);
```

### Device Memory Mapping

```c
/**
 * @brief Map device MMIO region
 */
void __iomem *ioremap(phys_addr_t phys_addr, size_t size)
{
    return vm_map_device(phys_addr, size, PROT_DEVICE_nGnRnE);
}

/**
 * @brief Unmap device region
 */
void iounmap(void __iomem *addr);
```

---

## Implementation Details (Phase 5)

During Phase 5, we will implement:

1. **Boot-time memory detection** from UEFI memory map
2. **Physical memory allocator** (buddy system)
3. **Page table setup** for kernel
4. **Kernel heap** (slab allocator)
5. **User space support** (per-process page tables)
6. **Copy-on-write** (COW) for efficient fork()
7. **Memory protection** and permissions
8. **TLB management** and shootdown for SMP

---

**Next Document**: [Scheduler](03-scheduler.md)
