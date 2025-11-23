/*
 * AArch64 Bare Metal OS - MMU Implementation
 *
 * Memory Management Unit setup with 4-level page tables
 */

#include "kernel.h"
#include "mmu.h"
#include "uart.h"

/* External symbols from linker script */
extern uint64_t __page_tables_start;
extern uint64_t __page_tables_end;
extern uint64_t __bss_start;
extern uint64_t __bss_end;
extern uint64_t __kernel_end;

/* Page table storage (256KB reserved in linker script) */
static page_table_t *kernel_pgd = NULL;                /* Level 0 for kernel (TTBR1_EL1) */

/* Next available page table */
static uint64_t next_page_table = 0;

/* Current user page table (for context switching) */
static page_table_t *current_user_pgd = NULL;

/*
 * Allocate a page table from reserved space
 */
static page_table_t *alloc_page_table(void)
{
    page_table_t *pt;

    if (next_page_table == 0) {
        next_page_table = (uint64_t)&__page_tables_start;
    }

    /* Check if we have space */
    if (next_page_table + sizeof(page_table_t) > (uint64_t)&__page_tables_end) {
        uart_puts("ERROR: Out of page table space!\n");
        while (1) __asm__ volatile("wfe");
    }

    pt = (page_table_t *)next_page_table;
    next_page_table += sizeof(page_table_t);

    /* Clear the page table */
    memset(pt, 0, sizeof(page_table_t));

    return pt;
}

/*
 * Get or create page table entry
 */
static page_table_t *get_or_create_table(page_table_t *parent, uint64_t index)
{
    uint64_t *entry = &parent->entries[index];
    page_table_t *table;

    /* If entry is valid and is a table, return it */
    if ((*entry & PTE_VALID) && (*entry & PTE_TABLE)) {
        return (page_table_t *)(*entry & PAGE_MASK);
    }

    /* Allocate new table */
    table = alloc_page_table();

    /* Create table descriptor */
    *entry = ((uint64_t)table & PAGE_MASK) | PTE_TYPE_TABLE | PTE_VALID;

    return table;
}

/*
 * Map a single 4KB page
 */
void map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
    page_table_t *pud, *pmd, *pte_table;
    uint64_t l0_idx, l1_idx, l2_idx, l3_idx;

    if (!kernel_pgd) {
        uart_puts("ERROR: MMU not initialized!\n");
        return;
    }

    /* Extract indices */
    l0_idx = VA_L0_INDEX(virt);
    l1_idx = VA_L1_INDEX(virt);
    l2_idx = VA_L2_INDEX(virt);
    l3_idx = VA_L3_INDEX(virt);

    /* Walk/create page tables */
    pud = get_or_create_table(kernel_pgd, l0_idx);
    pmd = get_or_create_table(pud, l1_idx);
    pte_table = get_or_create_table(pmd, l2_idx);

    /* Create page entry */
    pte_table->entries[l3_idx] = (phys & PAGE_MASK) | flags | PTE_TYPE_PAGE;
}

/*
 * Map a range of pages
 */
void map_range(uint64_t virt, uint64_t phys, size_t size, uint64_t flags)
{
    uint64_t virt_addr = page_align_down(virt);
    uint64_t phys_addr = page_align_down(phys);
    uint64_t end_addr = page_align_up(virt + size);

    while (virt_addr < end_addr) {
        map_page(virt_addr, phys_addr, flags);
        virt_addr += PAGE_SIZE;
        phys_addr += PAGE_SIZE;
    }
}

/*
 * Unmap a page
 */
void unmap_page(uint64_t virt)
{
    page_table_t *pud, *pmd, *pte_table;
    uint64_t l0_idx, l1_idx, l2_idx, l3_idx;
    uint64_t *entry;

    if (!kernel_pgd) {
        return;
    }

    /* Extract indices */
    l0_idx = VA_L0_INDEX(virt);
    l1_idx = VA_L1_INDEX(virt);
    l2_idx = VA_L2_INDEX(virt);
    l3_idx = VA_L3_INDEX(virt);

    /* Walk page tables */
    entry = &kernel_pgd->entries[l0_idx];
    if (!(*entry & PTE_VALID)) return;
    pud = (page_table_t *)(*entry & PAGE_MASK);

    entry = &pud->entries[l1_idx];
    if (!(*entry & PTE_VALID)) return;
    pmd = (page_table_t *)(*entry & PAGE_MASK);

    entry = &pmd->entries[l2_idx];
    if (!(*entry & PTE_VALID)) return;
    pte_table = (page_table_t *)(*entry & PAGE_MASK);

    /* Clear page entry */
    pte_table->entries[l3_idx] = 0;

    /* TLB invalidate */
    __asm__ volatile("tlbi vaae1is, %0" :: "r"(virt >> 12));
    dsb();
    isb();
}

/*
 * Create identity mapping for kernel and devices
 */
static void create_identity_mapping(void)
{
    uart_puts("Creating identity mappings...\n");

    /* Map DRAM region (64MB) for kernel and heap - matching PMM managed region */
    uart_puts("  Mapping DRAM (64MB)... ");
    map_range(DRAM_BASE, DRAM_BASE, 64 * 1024 * 1024, PAGE_KERNEL_RWX);
    uart_puts("OK\n");

    /* Map UART (1 page) */
    uart_puts("  Mapping UART... ");
    map_range(UART0_BASE, UART0_BASE, PAGE_SIZE, PAGE_DEVICE);
    uart_puts("OK\n");

    /* Map GIC Distributor (64KB) */
    uart_puts("  Mapping GIC Distributor... ");
    map_range(GIC_DIST_BASE, GIC_DIST_BASE, 64 * 1024, PAGE_DEVICE);
    uart_puts("OK\n");

    /* Map GIC CPU Interface (8KB) */
    uart_puts("  Mapping GIC CPU Interface... ");
    map_range(GIC_CPU_BASE, GIC_CPU_BASE, 8 * 1024, PAGE_DEVICE);
    uart_puts("OK\n");
}

/*
 * Enable MMU with dual address space (TTBR0 for user, TTBR1 for kernel)
 */
void mmu_enable(void)
{
    uint64_t tcr, mair, sctlr;

    uart_puts("Enabling MMU...\n");

    /* Set Translation Table Base Registers */
    uart_puts("  Setting TTBR0_EL1 (user space)... ");
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"(0UL));  /* No user table yet */
    uart_puts("OK\n");

    uart_puts("  Setting TTBR1_EL1 (kernel space)... ");
    __asm__ volatile("msr ttbr1_el1, %0" :: "r"((uint64_t)kernel_pgd));
    uart_puts("OK\n");

    /* Configure TCR_EL1 for dual address spaces */
    uart_puts("  Configuring TCR_EL1... ");
    tcr = TCR_T0SZ(64 - USER_VA_BITS) |     /* User VA size = 39 bits (512GB) */
          TCR_IRGN0_WBWA |                  /* Inner WB WA cacheable */
          TCR_ORGN0_WBWA |                  /* Outer WB WA cacheable */
          TCR_SH0_INNER |                   /* Inner shareable */
          TCR_TG0_4K |                      /* 4KB granule for TTBR0 */
          TCR_T1SZ(64 - KERNEL_VA_BITS) |   /* Kernel VA size = 39 bits (512GB) */
          TCR_IRGN1_WBWA |                  /* Inner WB WA cacheable */
          TCR_ORGN1_WBWA |                  /* Outer WB WA cacheable */
          TCR_SH1_INNER |                   /* Inner shareable */
          TCR_TG1_4K |                      /* 4KB granule for TTBR1 */
          TCR_IPS_48BIT;                    /* 48-bit physical address */
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr));
    uart_puts("OK\n");

    /* Configure MAIR_EL1 */
    uart_puts("  Configuring MAIR_EL1... ");
    mair = MAIR_VALUE;
    __asm__ volatile("msr mair_el1, %0" :: "r"(mair));
    uart_puts("OK\n");

    /* Enable MMU and caches in SCTLR_EL1 */
    uart_puts("  Enabling MMU and caches... ");
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= SCTLR_M | SCTLR_C | SCTLR_I;   /* Enable MMU, D-cache, I-cache */
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr));
    isb();
    uart_puts("OK\n");

    uart_puts("MMU enabled successfully!\n");
    uart_puts("  User space (TTBR0): 0x0000000000000000 - 0x0000007FFFFFFFFF\n");
    uart_puts("  Kernel space (TTBR1): 0xFFFFFF8000000000 - 0xFFFFFFFFFFFFFFFF\n");
}

/*
 * Initialize MMU
 */
void mmu_init(void)
{
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Initializing MMU\n");
    uart_puts("========================================\n");

    /* Allocate root page table (PGD - Level 0) */
    uart_puts("Allocating page tables...\n");
    kernel_pgd = alloc_page_table();
    uart_puts("  PGD: ");
    uart_puthex((uint64_t)kernel_pgd);
    uart_puts("\n");

    /* Create identity mappings */
    create_identity_mapping();

    /* Enable MMU */
    mmu_enable();

    uart_puts("========================================\n");
    uart_puts("MMU Initialization Complete\n");
    uart_puts("========================================\n");
    uart_puts("\n");
}

/*
 * Get kernel page global directory
 */
page_table_t *mmu_get_kernel_pgd(void)
{
    return kernel_pgd;
}

/*
 * Create a new user page table
 */
page_table_t *mmu_create_user_table(void)
{
    page_table_t *user_pgd;

    /* Allocate root page table for user space */
    user_pgd = alloc_page_table();

    uart_puts("Created user page table at ");
    uart_puthex((uint64_t)user_pgd);
    uart_puts("\n");

    return user_pgd;
}

/*
 * Destroy a user page table
 */
void mmu_destroy_user_table(page_table_t *user_pgd)
{
    /* TODO: Walk page tables and free all allocated pages */
    /* For now, we just note that this table is no longer in use */
    if (current_user_pgd == user_pgd) {
        current_user_pgd = NULL;
        __asm__ volatile("msr ttbr0_el1, %0" :: "r"(0UL));
        isb();
    }
}

/*
 * Map a single page in user page table
 */
void mmu_map_user_page(page_table_t *user_pgd, uint64_t virt, uint64_t phys, uint64_t flags)
{
    page_table_t *pud, *pmd, *pte_table;
    uint64_t l0_idx, l1_idx, l2_idx, l3_idx;

    if (!user_pgd) {
        uart_puts("ERROR: Invalid user page table!\n");
        return;
    }

    /* Verify this is a user space address */
    if (virt >= USER_VA_SIZE) {
        uart_puts("ERROR: Address not in user space: ");
        uart_puthex(virt);
        uart_puts("\n");
        return;
    }

    /* Extract indices */
    l0_idx = VA_L0_INDEX(virt);
    l1_idx = VA_L1_INDEX(virt);
    l2_idx = VA_L2_INDEX(virt);
    l3_idx = VA_L3_INDEX(virt);

    /* Walk/create page tables (using same helper as kernel) */
    pud = get_or_create_table(user_pgd, l0_idx);
    pmd = get_or_create_table(pud, l1_idx);
    pte_table = get_or_create_table(pmd, l2_idx);

    /* Create page entry */
    pte_table->entries[l3_idx] = (phys & PAGE_MASK) | flags | PTE_TYPE_PAGE;
}

/*
 * Map a range of pages in user page table
 */
void mmu_map_user_range(page_table_t *user_pgd, uint64_t virt, uint64_t phys, size_t size, uint64_t flags)
{
    uint64_t virt_addr = page_align_down(virt);
    uint64_t phys_addr = page_align_down(phys);
    uint64_t end_addr = page_align_up(virt + size);

    while (virt_addr < end_addr) {
        mmu_map_user_page(user_pgd, virt_addr, phys_addr, flags);
        virt_addr += PAGE_SIZE;
        phys_addr += PAGE_SIZE;
    }
}

/*
 * Switch to user page table (update TTBR0_EL1)
 */
void mmu_switch_user_table(page_table_t *user_pgd)
{
    if (user_pgd == current_user_pgd) {
        return;  /* Already using this table */
    }

    current_user_pgd = user_pgd;

    /* Update TTBR0_EL1 */
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"((uint64_t)user_pgd));

    /* Invalidate TLB for user space (ASID 0) */
    __asm__ volatile("tlbi vmalle1is");
    dsb();
    isb();
}

/*
 * Get physical address from virtual address
 */
uint64_t mmu_get_physical(page_table_t *pgd, uint64_t virt)
{
    page_table_t *pud, *pmd, *pte_table;
    uint64_t l0_idx, l1_idx, l2_idx, l3_idx;
    uint64_t *entry;
    uint64_t pte;

    if (!pgd) {
        return 0;
    }

    /* Extract indices */
    l0_idx = VA_L0_INDEX(virt);
    l1_idx = VA_L1_INDEX(virt);
    l2_idx = VA_L2_INDEX(virt);
    l3_idx = VA_L3_INDEX(virt);

    /* Walk page tables */
    entry = &pgd->entries[l0_idx];
    if (!(*entry & PTE_VALID)) return 0;
    pud = (page_table_t *)(*entry & PAGE_MASK);

    entry = &pud->entries[l1_idx];
    if (!(*entry & PTE_VALID)) return 0;
    pmd = (page_table_t *)(*entry & PAGE_MASK);

    entry = &pmd->entries[l2_idx];
    if (!(*entry & PTE_VALID)) return 0;
    pte_table = (page_table_t *)(*entry & PAGE_MASK);

    /* Get page entry */
    pte = pte_table->entries[l3_idx];
    if (!(pte & PTE_VALID)) return 0;

    /* Return physical address */
    return (pte & PAGE_MASK) | (virt & ~PAGE_MASK);
}
