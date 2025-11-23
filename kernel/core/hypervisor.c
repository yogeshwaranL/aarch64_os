/*
 * AArch64 Bare Metal OS - Hypervisor Implementation
 *
 * Type-1 Hypervisor Foundations running at EL2
 */

#include "hypervisor.h"
#include "kernel.h"
#include "uart.h"

/* Hypervisor statistics */
static struct hyp_stats stats = {0};

/* Hypervisor version */
#define HYPERVISOR_VERSION_MAJOR    1
#define HYPERVISOR_VERSION_MINOR    0

/*
 * Get current exception level
 */
uint64_t hypervisor_get_el(void)
{
    uint64_t current_el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    return (current_el >> 2) & 0x3;
}

/*
 * Configure HCR_EL2 register
 */
void hypervisor_configure_hcr(uint64_t flags)
{
    uint64_t hcr;

    /* Read current value */
    __asm__ volatile("mrs %0, hcr_el2" : "=r"(hcr));

    uart_puts("  Current HCR_EL2: ");
    uart_puthex(hcr);
    uart_puts("\n");

    /* Set new flags */
    hcr = flags;
    __asm__ volatile("msr hcr_el2, %0" :: "r"(hcr));
    __asm__ volatile("isb");

    /* Read back to verify */
    __asm__ volatile("mrs %0, hcr_el2" : "=r"(hcr));

    uart_puts("  New HCR_EL2:     ");
    uart_puthex(hcr);
    uart_puts("\n");
}

/*
 * Initialize hypervisor
 */
void hypervisor_init(void)
{
    uint64_t el = hypervisor_get_el();

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Hypervisor Initialization\n");
    uart_puts("========================================\n");

    uart_puts("Current Exception Level: EL");
    uart_putc('0' + el);
    uart_puts("\n");

    if (el != 2) {
        uart_puts("ERROR: Not running at EL2!\n");
        uart_puts("Hypervisor requires EL2 privilege level.\n");
        return;
    }

    uart_puts("Hypervisor Version: ");
    uart_putc('0' + HYPERVISOR_VERSION_MAJOR);
    uart_putc('.');
    uart_putc('0' + HYPERVISOR_VERSION_MINOR);
    uart_puts("\n\n");

    /*
     * Configure HCR_EL2 for basic hypervisor operation
     *
     * HCR_EL2_RW:   Lower levels run in AArch64 state
     *
     * For Option B (Hypervisor Foundations), we keep it simple:
     * - Stay at EL2 (don't drop to EL1)
     * - Don't enable VM bit (no stage-2 translation)
     * - Just configure basic EL2 features
     */
    uart_puts("Configuring HCR_EL2...\n");
    uint64_t hcr_flags = HCR_EL2_RW;  /* AArch64 execution state */
    hypervisor_configure_hcr(hcr_flags);

    uart_puts("\n");
    uart_puts("Hypervisor Features:\n");
    uart_puts("  [*] Running at EL2 (Hypervisor mode)\n");
    uart_puts("  [*] AArch64 execution state\n");
    uart_puts("  [*] HVC (Hypervisor Call) support\n");
    uart_puts("  [*] Trap handling capability\n");
    uart_puts("  [ ] Stage-2 page tables (not enabled)\n");
    uart_puts("  [ ] Full VM isolation (not enabled)\n");

    uart_puts("\n");
    uart_puts("Hypervisor initialized successfully!\n");
    uart_puts("========================================\n");
    uart_puts("\n");
}

/*
 * Handle HVC (Hypervisor Call)
 *
 * Called from exception handler when HVC instruction is executed
 * x0 = function code
 * x1-x3 = arguments
 * Returns result in x0
 */
uint64_t hypervisor_handle_hvc(uint64_t function, uint64_t arg0,
                               uint64_t arg1, uint64_t arg2)
{
    (void)arg1;  /* Unused for now */
    (void)arg2;  /* Unused for now */

    stats.hvc_count++;

    switch (function) {
    case HVC_GET_VERSION:
        /* Return hypervisor version */
        return (HYPERVISOR_VERSION_MAJOR << 16) | HYPERVISOR_VERSION_MINOR;

    case HVC_CONSOLE_PUTC:
        /* Output character to console */
        uart_putc((char)arg0);
        return 0;

    case HVC_VM_INFO:
        /* Return current EL */
        return hypervisor_get_el();

    case HVC_TRAP_INFO:
        /* Return trap count */
        return stats.trap_count;

    default:
        uart_puts("[HYP] Unknown HVC function: ");
        uart_puthex(function);
        uart_puts("\n");
        return (uint64_t)-1;
    }
}

/*
 * Handle trap to EL2
 *
 * Called when an exception is taken to EL2
 * esr = Exception Syndrome Register
 * far = Fault Address Register
 * elr = Exception Link Register
 */
void hypervisor_handle_trap(uint64_t esr, uint64_t far, uint64_t elr)
{
    uint64_t ec = (esr >> ESR_EL2_EC_SHIFT) & ESR_EL2_EC_MASK;
    uint64_t iss = esr & ESR_EL2_ISS_MASK;

    stats.trap_count++;

    uart_puts("[HYP] Trap to EL2:\n");
    uart_puts("  ESR_EL2: ");
    uart_puthex(esr);
    uart_puts("\n");

    uart_puts("  EC:      ");
    uart_puthex(ec);
    uart_puts(" (");

    switch (ec) {
    case ESR_EC_UNKNOWN:
        uart_puts("Unknown");
        break;
    case ESR_EC_WFI_WFE:
        uart_puts("WFI/WFE");
        if (iss & 1)
            stats.wfe_count++;
        else
            stats.wfi_count++;
        break;
    case ESR_EC_HVC:
        uart_puts("HVC");
        break;
    case ESR_EC_SMC:
        uart_puts("SMC");
        break;
    case ESR_EC_IABT_LOW:
        uart_puts("Instruction Abort");
        stats.inst_abort_count++;
        break;
    case ESR_EC_DABT_LOW:
        uart_puts("Data Abort");
        stats.data_abort_count++;
        break;
    default:
        uart_puts("Other");
        break;
    }

    uart_puts(")\n");
    uart_puts("  FAR_EL2: ");
    uart_puthex(far);
    uart_puts("\n");
    uart_puts("  ELR_EL2: ");
    uart_puthex(elr);
    uart_puts("\n");
}

/*
 * Get hypervisor statistics
 */
void hypervisor_get_stats(struct hyp_stats *out_stats)
{
    if (out_stats) {
        *out_stats = stats;
    }
}

/*
 * Dump hypervisor state and statistics
 */
void hypervisor_dump_state(void)
{
    uint64_t el = hypervisor_get_el();
    uint64_t hcr, vtcr, vttbr;

    __asm__ volatile("mrs %0, hcr_el2" : "=r"(hcr));
    __asm__ volatile("mrs %0, vtcr_el2" : "=r"(vtcr));
    __asm__ volatile("mrs %0, vttbr_el2" : "=r"(vttbr));

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Hypervisor State\n");
    uart_puts("========================================\n");

    uart_puts("Exception Level:  EL");
    uart_putc('0' + el);
    uart_puts("\n");

    uart_puts("HCR_EL2:          ");
    uart_puthex(hcr);
    uart_puts("\n");

    uart_puts("VTCR_EL2:         ");
    uart_puthex(vtcr);
    uart_puts("\n");

    uart_puts("VTTBR_EL2:        ");
    uart_puthex(vttbr);
    uart_puts("\n");

    uart_puts("\nStatistics:\n");
    uart_puts("  HVC calls:      ");
    uart_puthex(stats.hvc_count);
    uart_puts("\n");

    uart_puts("  Total traps:    ");
    uart_puthex(stats.trap_count);
    uart_puts("\n");

    uart_puts("  WFI traps:      ");
    uart_puthex(stats.wfi_count);
    uart_puts("\n");

    uart_puts("  WFE traps:      ");
    uart_puthex(stats.wfe_count);
    uart_puts("\n");

    uart_puts("  Data aborts:    ");
    uart_puthex(stats.data_abort_count);
    uart_puts("\n");

    uart_puts("  Inst aborts:    ");
    uart_puthex(stats.inst_abort_count);
    uart_puts("\n");

    uart_puts("========================================\n");
    uart_puts("\n");
}
