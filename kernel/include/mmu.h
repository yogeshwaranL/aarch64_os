/*
 * AArch64 Bare Metal OS - MMU Header
 *
 * Memory Management Unit configuration and page table structures
 */

#ifndef _MMU_H
#define _MMU_H

#include "kernel.h"

/* Page sizes */
#define PAGE_SHIFT          12
#define PAGE_SIZE           (1UL << PAGE_SHIFT)         /* 4KB */
#define PAGE_MASK           (~(PAGE_SIZE - 1))

/* Page table levels (ARMv8-A 4KB granule, 48-bit VA) */
#define PT_LEVELS           4
#define PT_ENTRIES          512                         /* 512 entries per table */

/* Virtual address breakdown (48-bit) */
#define VA_BITS             48
#define VA_L0_SHIFT         39                          /* Level 0: bits [47:39] */
#define VA_L1_SHIFT         30                          /* Level 1: bits [38:30] */
#define VA_L2_SHIFT         21                          /* Level 2: bits [29:21] */
#define VA_L3_SHIFT         12                          /* Level 3: bits [20:12] */
#define VA_OFFSET_MASK      0xFFF                       /* Offset: bits [11:0] */

/* User/Kernel address space split (39-bit VA for better separation) */
#define USER_VA_BITS        39                          /* 512GB user space */
#define KERNEL_VA_BITS      39                          /* 512GB kernel space */
#define USER_VA_SIZE        (1UL << USER_VA_BITS)       /* 512GB */
#define KERNEL_VA_BASE      0xFFFFFF8000000000UL        /* Kernel space starts here */

/* Extract page table indices from virtual address */
#define VA_L0_INDEX(va)     (((va) >> VA_L0_SHIFT) & 0x1FF)
#define VA_L1_INDEX(va)     (((va) >> VA_L1_SHIFT) & 0x1FF)
#define VA_L2_INDEX(va)     (((va) >> VA_L2_SHIFT) & 0x1FF)
#define VA_L3_INDEX(va)     (((va) >> VA_L3_SHIFT) & 0x1FF)

/* Page table entry (PTE) descriptor types */
#define PTE_TYPE_INVALID    0x0                         /* Invalid/unmapped */
#define PTE_TYPE_BLOCK      0x1                         /* Block entry (L1/L2) */
#define PTE_TYPE_TABLE      0x3                         /* Table entry (L0/L1/L2) */
#define PTE_TYPE_PAGE       0x3                         /* Page entry (L3) */

/* Page table entry bits */
#define PTE_VALID           (1UL << 0)                  /* Valid entry */
#define PTE_TABLE           (1UL << 1)                  /* Table descriptor */
#define PTE_AF              (1UL << 10)                 /* Access flag */
#define PTE_NG              (1UL << 11)                 /* Not global */
#define PTE_PXN             (1UL << 53)                 /* Privileged XN */
#define PTE_UXN             (1UL << 54)                 /* User XN */

/* Memory attributes (AttrIndx field) */
#define PTE_ATTR_DEVICE     0                           /* Device memory */
#define PTE_ATTR_NORMAL_NC  1                           /* Normal, non-cacheable */
#define PTE_ATTR_NORMAL     2                           /* Normal, cacheable */

#define PTE_ATTRINDX(x)     (((uint64_t)(x)) << 2)      /* AttrIndx field */

/* Shareability */
#define PTE_SH_NONE         (0UL << 8)                  /* Non-shareable */
#define PTE_SH_OUTER        (2UL << 8)                  /* Outer shareable */
#define PTE_SH_INNER        (3UL << 8)                  /* Inner shareable */

/* Access permissions (AP field) */
#define PTE_AP_RW_EL1       (0UL << 6)                  /* RW, EL1 only */
#define PTE_AP_RW_ALL       (1UL << 6)                  /* RW, all ELs */
#define PTE_AP_RO_EL1       (2UL << 6)                  /* RO, EL1 only */
#define PTE_AP_RO_ALL       (3UL << 6)                  /* RO, all ELs */

/* Common page attributes - Kernel */
#define PAGE_KERNEL_RO      (PTE_VALID | PTE_AF | PTE_ATTRINDX(PTE_ATTR_NORMAL) | \
                             PTE_SH_INNER | PTE_AP_RO_EL1 | PTE_PXN | PTE_UXN)

#define PAGE_KERNEL_RW      (PTE_VALID | PTE_AF | PTE_ATTRINDX(PTE_ATTR_NORMAL) | \
                             PTE_SH_INNER | PTE_AP_RW_EL1 | PTE_PXN | PTE_UXN)

#define PAGE_KERNEL_RX      (PTE_VALID | PTE_AF | PTE_ATTRINDX(PTE_ATTR_NORMAL) | \
                             PTE_SH_INNER | PTE_AP_RO_EL1 | PTE_UXN)

#define PAGE_KERNEL_RWX     (PTE_VALID | PTE_AF | PTE_ATTRINDX(PTE_ATTR_NORMAL) | \
                             PTE_SH_INNER | PTE_AP_RW_EL1)

#define PAGE_DEVICE         (PTE_VALID | PTE_AF | PTE_ATTRINDX(PTE_ATTR_DEVICE) | \
                             PTE_AP_RW_EL1 | PTE_PXN | PTE_UXN)

/* Common page attributes - User */
#define PAGE_USER_RO        (PTE_VALID | PTE_AF | PTE_ATTRINDX(PTE_ATTR_NORMAL) | \
                             PTE_SH_INNER | PTE_AP_RO_ALL | PTE_PXN | PTE_UXN | PTE_NG)

#define PAGE_USER_RW        (PTE_VALID | PTE_AF | PTE_ATTRINDX(PTE_ATTR_NORMAL) | \
                             PTE_SH_INNER | PTE_AP_RW_ALL | PTE_PXN | PTE_UXN | PTE_NG)

#define PAGE_USER_RX        (PTE_VALID | PTE_AF | PTE_ATTRINDX(PTE_ATTR_NORMAL) | \
                             PTE_SH_INNER | PTE_AP_RO_ALL | PTE_PXN | PTE_NG)

/* Page table structure */
typedef struct {
    uint64_t entries[PT_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

/* TCR_EL1 - Translation Control Register */
#define TCR_T0SZ(x)         ((uint64_t)(x) << 0)        /* Size offset for TTBR0 */
#define TCR_IRGN0_WBWA      (1UL << 8)                  /* Inner WB WA cacheable */
#define TCR_ORGN0_WBWA      (1UL << 10)                 /* Outer WB WA cacheable */
#define TCR_SH0_INNER       (3UL << 12)                 /* Inner shareable */
#define TCR_TG0_4K          (0UL << 14)                 /* 4KB granule */
#define TCR_T1SZ(x)         ((uint64_t)(x) << 16)       /* Size offset for TTBR1 */
#define TCR_A1              (1UL << 22)                 /* ASID in TTBR1 */
#define TCR_IRGN1_WBWA      (1UL << 24)                 /* Inner WB WA cacheable */
#define TCR_ORGN1_WBWA      (1UL << 26)                 /* Outer WB WA cacheable */
#define TCR_SH1_INNER       (3UL << 28)                 /* Inner shareable */
#define TCR_TG1_4K          (2UL << 30)                 /* 4KB granule */
#define TCR_IPS_48BIT       (5UL << 32)                 /* 48-bit physical address */

/* MAIR_EL1 - Memory Attribute Indirection Register */
#define MAIR_DEVICE_nGnRnE  0x00                        /* Device-nGnRnE */
#define MAIR_NORMAL_NC      0x44                        /* Normal, non-cacheable */
#define MAIR_NORMAL         0xFF                        /* Normal, WB RW-Allocate */

#define MAIR_VALUE          (MAIR_DEVICE_nGnRnE | \
                             (MAIR_NORMAL_NC << 8) | \
                             (MAIR_NORMAL << 16))

/* SCTLR_EL1 bits */
#define SCTLR_M             (1UL << 0)                  /* MMU enable */
#define SCTLR_A             (1UL << 1)                  /* Alignment check */
#define SCTLR_C             (1UL << 2)                  /* Data cache enable */
#define SCTLR_SA            (1UL << 3)                  /* Stack alignment */
#define SCTLR_I             (1UL << 12)                 /* Instruction cache */
#define SCTLR_WXN           (1UL << 19)                 /* Write implies XN */
#define SCTLR_EE            (1UL << 25)                 /* Exception endianness */

/* Memory regions */
#define KERNEL_BASE         0x40400000UL                /* Kernel load address */
#define KERNEL_SIZE         (16 * 1024 * 1024)          /* 16MB for kernel */
#define DRAM_BASE           0x40000000UL                /* DRAM start */
#define DRAM_SIZE           (1UL << 30)                 /* 1GB */

/* Function prototypes */
void mmu_init(void);
void mmu_enable(void);
page_table_t *mmu_get_kernel_pgd(void);

/* Kernel mapping functions */
void map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void map_range(uint64_t virt, uint64_t phys, size_t size, uint64_t flags);
void unmap_page(uint64_t virt);

/* User page table management */
page_table_t *mmu_create_user_table(void);
void mmu_destroy_user_table(page_table_t *user_pgd);
void mmu_map_user_page(page_table_t *user_pgd, uint64_t virt, uint64_t phys, uint64_t flags);
void mmu_map_user_range(page_table_t *user_pgd, uint64_t virt, uint64_t phys, size_t size, uint64_t flags);
void mmu_switch_user_table(page_table_t *user_pgd);
uint64_t mmu_get_physical(page_table_t *pgd, uint64_t virt);

/* Utility functions */
static inline uint64_t page_align_down(uint64_t addr)
{
    return addr & PAGE_MASK;
}

static inline uint64_t page_align_up(uint64_t addr)
{
    return (addr + PAGE_SIZE - 1) & PAGE_MASK;
}

static inline bool is_page_aligned(uint64_t addr)
{
    return (addr & ~PAGE_MASK) == 0;
}

#endif /* _MMU_H */
