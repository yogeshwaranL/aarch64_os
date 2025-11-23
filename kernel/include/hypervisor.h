/*
 * AArch64 Bare Metal OS - Hypervisor Support (EL2)
 *
 * Hypervisor Configuration and Control
 * Implements basic Type-1 hypervisor foundations at EL2
 */

#ifndef HYPERVISOR_H
#define HYPERVISOR_H

#include "kernel.h"

/*
 * HCR_EL2 - Hypervisor Configuration Register
 * Controls virtualization features and traps
 */
#define HCR_EL2_RW      (1UL << 31)  /* Execution state control: 1=AArch64, 0=AArch32 */
#define HCR_EL2_TGE     (1UL << 27)  /* Trap General Exceptions */
#define HCR_EL2_VM      (1UL << 0)   /* Virtualization MMU enable */
#define HCR_EL2_SWIO    (1UL << 1)   /* Set/Way Invalidation Override */
#define HCR_EL2_PTW     (1UL << 2)   /* Protected Table Walk */
#define HCR_EL2_FMO     (1UL << 3)   /* FIQ Mask Override */
#define HCR_EL2_IMO     (1UL << 4)   /* IRQ Mask Override */
#define HCR_EL2_AMO     (1UL << 5)   /* SError Mask Override */
#define HCR_EL2_TWI     (1UL << 13)  /* Trap WFI */
#define HCR_EL2_TWE     (1UL << 14)  /* Trap WFE */

/*
 * VTCR_EL2 - Virtualization Translation Control Register
 * Controls stage-2 translation table
 */
#define VTCR_EL2_T0SZ(x)    ((x) & 0x3F)        /* Size offset of memory region */
#define VTCR_EL2_SL0(x)     (((x) & 0x3) << 6)  /* Starting level */
#define VTCR_EL2_IRGN0(x)   (((x) & 0x3) << 8)  /* Inner cacheability */
#define VTCR_EL2_ORGN0(x)   (((x) & 0x3) << 10) /* Outer cacheability */
#define VTCR_EL2_SH0(x)     (((x) & 0x3) << 12) /* Shareability */
#define VTCR_EL2_TG0(x)     (((x) & 0x3) << 14) /* Granule size */
#define VTCR_EL2_PS(x)      (((x) & 0x7) << 16) /* Physical address size */

/*
 * VTTBR_EL2 - Virtualization Translation Table Base Register
 * Holds base address of stage-2 translation table
 */
#define VTTBR_EL2_VMID(x)   (((uint64_t)(x) & 0xFF) << 48) /* VM ID */
#define VTTBR_EL2_BADDR(x)  ((uint64_t)(x) & 0xFFFFFFFFF000UL) /* Base address */

/*
 * Hypervisor Call (HVC) Function Codes
 * Used for hypercalls from guest to hypervisor
 */
#define HVC_GET_VERSION     0x0000  /* Get hypervisor version */
#define HVC_CONSOLE_PUTC    0x0001  /* Console output character */
#define HVC_CONSOLE_GETC    0x0002  /* Console input character */
#define HVC_VM_INFO         0x0010  /* Get VM information */
#define HVC_TRAP_INFO       0x0011  /* Get trap/exception info */

/*
 * Exception Syndrome Register (ESR_EL2) fields
 * For analyzing exceptions taken to EL2
 */
#define ESR_EL2_EC_SHIFT    26      /* Exception Class shift */
#define ESR_EL2_EC_MASK     0x3F    /* Exception Class mask */
#define ESR_EL2_IL          (1 << 25) /* Instruction length */
#define ESR_EL2_ISS_MASK    0x1FFFFFF /* Instruction Specific Syndrome */

/* Exception Classes */
#define ESR_EC_UNKNOWN      0x00    /* Unknown reason */
#define ESR_EC_WFI_WFE      0x01    /* WFI or WFE instruction */
#define ESR_EC_HVC          0x16    /* HVC instruction in AArch64 */
#define ESR_EC_SMC          0x17    /* SMC instruction in AArch64 */
#define ESR_EC_IABT_LOW     0x20    /* Instruction abort from lower EL */
#define ESR_EC_DABT_LOW     0x24    /* Data abort from lower EL */

/*
 * Hypervisor statistics and state
 */
struct hyp_stats {
    uint64_t hvc_count;         /* Total HVC calls */
    uint64_t trap_count;        /* Total traps */
    uint64_t wfi_count;         /* WFI traps */
    uint64_t wfe_count;         /* WFE traps */
    uint64_t data_abort_count;  /* Data aborts */
    uint64_t inst_abort_count;  /* Instruction aborts */
};

/*
 * Hypervisor functions
 */

/* Initialize hypervisor at EL2 */
void hypervisor_init(void);

/* Configure HCR_EL2 register */
void hypervisor_configure_hcr(uint64_t flags);

/* Get current exception level */
uint64_t hypervisor_get_el(void);

/* Handle HVC call */
uint64_t hypervisor_handle_hvc(uint64_t function, uint64_t arg0,
                               uint64_t arg1, uint64_t arg2);

/* Handle EL2 trap */
void hypervisor_handle_trap(uint64_t esr, uint64_t far, uint64_t elr);

/* Get hypervisor statistics */
void hypervisor_get_stats(struct hyp_stats *stats);

/* Dump hypervisor state */
void hypervisor_dump_state(void);

#endif /* HYPERVISOR_H */
