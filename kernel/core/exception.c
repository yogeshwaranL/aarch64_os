/*
 * AArch64 Bare Metal OS - Exception Handlers
 *
 * C-level exception handlers called from assembly
 */

#include "kernel.h"
#include "exception.h"
#include "irq.h"
#include "gic.h"
#include "uart.h"
#include "syscall.h"
#include "hypervisor.h"

/* Exception class names for ESR decoding */
static const char *ec_names[] = {
    "Unknown",                                  /* 0x00 */
    "Trapped WFI/WFE",                          /* 0x01 */
    "Unknown",
    "Trapped MCR/MRC (CP15)",                   /* 0x03 */
    "Trapped MCRR/MRRC (CP15)",                 /* 0x04 */
    "Trapped MCR/MRC (CP14)",                   /* 0x05 */
    "Trapped LDC/STC",                          /* 0x06 */
    "SVE/SIMD/FP trapped",                      /* 0x07 */
    "Unknown", "Unknown",
    "Trapped VMRS",                             /* 0x0A */
    "Unknown",
    "Trapped MRRC (CP14)",                      /* 0x0C */
    "Unknown",
    "Illegal Execution State",                  /* 0x0E */
    "Unknown",
    "Unknown", "Unknown",
    "SVC (AArch32)",                            /* 0x11 */
    "Unknown",
    "Unknown",
    "Unknown",
    "SVC (AArch64)",                            /* 0x15 */
    "HVC (AArch64)",                            /* 0x16 */
    "SMC (AArch64)",                            /* 0x17 */
    "Trapped MSR/MRS/System",                   /* 0x18 */
    "Unknown",
    "Unknown",
    "Unknown",
    "Unknown",
    "Unknown",
    "Unknown",
    "Unknown",
    "Instruction Abort (lower EL)",             /* 0x20 */
    "Instruction Abort (same EL)",              /* 0x21 */
    "PC alignment fault",                       /* 0x22 */
    "Unknown",
    "Data Abort (lower EL)",                    /* 0x24 */
    "Data Abort (same EL)",                     /* 0x25 */
    "SP alignment fault",                       /* 0x26 */
    "Unknown",
    "FP exception (AArch32)",                   /* 0x28 */
    "Unknown", "Unknown",
    "Unknown",
    "FP exception (AArch64)",                   /* 0x2C */
    "Unknown", "Unknown",
    "SError",                                   /* 0x2F */
    "Breakpoint (lower EL)",                    /* 0x30 */
    "Breakpoint (same EL)",                     /* 0x31 */
    "Software Step (lower EL)",                 /* 0x32 */
    "Software Step (same EL)",                  /* 0x33 */
    "Watchpoint (lower EL)",                    /* 0x34 */
    "Watchpoint (same EL)",                     /* 0x35 */
    "Unknown", "Unknown",
    "BKPT (AArch32)",                           /* 0x38 */
    "Unknown",
    "Vector Catch (AArch32)",                   /* 0x3A */
    "Unknown",
    "BRK (AArch64)",                            /* 0x3C */
};

#define EC_NAMES_SIZE   (sizeof(ec_names) / sizeof(ec_names[0]))

/*
 * Get exception class name from ESR
 */
const char *get_exception_class_name(uint32_t ec)
{
    if (ec < EC_NAMES_SIZE && ec_names[ec] != NULL) {
        return ec_names[ec];
    }
    return "Unknown Exception Class";
}

/*
 * Dump exception frame
 */
void dump_exception_frame(struct exception_frame *frame)
{
    uart_puts("\nException Frame:\n");
    uart_puts("  x0:  "); uart_puthex(frame->x0);  uart_puts("  x1:  "); uart_puthex(frame->x1);  uart_puts("\n");
    uart_puts("  x2:  "); uart_puthex(frame->x2);  uart_puts("  x3:  "); uart_puthex(frame->x3);  uart_puts("\n");
    uart_puts("  x4:  "); uart_puthex(frame->x4);  uart_puts("  x5:  "); uart_puthex(frame->x5);  uart_puts("\n");
    uart_puts("  x6:  "); uart_puthex(frame->x6);  uart_puts("  x7:  "); uart_puthex(frame->x7);  uart_puts("\n");
    uart_puts("  x8:  "); uart_puthex(frame->x8);  uart_puts("  x9:  "); uart_puthex(frame->x9);  uart_puts("\n");
    uart_puts("  x10: "); uart_puthex(frame->x10); uart_puts("  x11: "); uart_puthex(frame->x11); uart_puts("\n");
    uart_puts("  x12: "); uart_puthex(frame->x12); uart_puts("  x13: "); uart_puthex(frame->x13); uart_puts("\n");
    uart_puts("  x14: "); uart_puthex(frame->x14); uart_puts("  x15: "); uart_puthex(frame->x15); uart_puts("\n");
    uart_puts("  x16: "); uart_puthex(frame->x16); uart_puts("  x17: "); uart_puthex(frame->x17); uart_puts("\n");
    uart_puts("  x18: "); uart_puthex(frame->x18); uart_puts("  x19: "); uart_puthex(frame->x19); uart_puts("\n");
    uart_puts("  x20: "); uart_puthex(frame->x20); uart_puts("  x21: "); uart_puthex(frame->x21); uart_puts("\n");
    uart_puts("  x22: "); uart_puthex(frame->x22); uart_puts("  x23: "); uart_puthex(frame->x23); uart_puts("\n");
    uart_puts("  x24: "); uart_puthex(frame->x24); uart_puts("  x25: "); uart_puthex(frame->x25); uart_puts("\n");
    uart_puts("  x26: "); uart_puthex(frame->x26); uart_puts("  x27: "); uart_puthex(frame->x27); uart_puts("\n");
    uart_puts("  x28: "); uart_puthex(frame->x28); uart_puts("  x29: "); uart_puthex(frame->x29); uart_puts("\n");
    uart_puts("  x30: "); uart_puthex(frame->x30); uart_puts("  SP:  "); uart_puthex(frame->sp);  uart_puts("\n");
    uart_puts("  ELR: "); uart_puthex(frame->elr); uart_puts("  SPSR:"); uart_puthex(frame->spsr); uart_puts("\n");
}

/*
 * Synchronous exception handler
 */
void handle_sync_exception(struct exception_frame *frame, uint64_t esr, uint64_t far)
{
    uint32_t ec = (esr >> 26) & 0x3F;           /* Exception class */
    uint32_t iss = esr & 0x1FFFFFF;             /* Instruction Specific Syndrome */

    /* Check if this is a system call (SVC from AArch64) */
    if (ec == 0x15) {                           /* SVC (AArch64) */
        /* System call - arguments in x0-x3, syscall number in x8 */
        uint64_t result = syscall_handler(frame->x8, frame->x0, frame->x1,
                                          frame->x2, frame->x3);

        /* Return value goes back in x0 */
        frame->x0 = result;
        return;
    }

    /* Check if this is a hypervisor call (HVC from AArch64) */
    if (ec == 0x16) {                           /* HVC (AArch64) */
        /* Hypervisor call - function in x0, arguments in x1-x3 */
        uint64_t result = hypervisor_handle_hvc(frame->x0, frame->x1,
                                                frame->x2, frame->x3);

        /* Return value goes back in x0 */
        frame->x0 = result;
        return;
    }

    /* Not a syscall - this is a real exception */
    uart_puts("\n");
    uart_puts("*** SYNCHRONOUS EXCEPTION ***\n");
    uart_puts("ESR:   "); uart_puthex(esr); uart_puts("\n");
    uart_puts("FAR:   "); uart_puthex(far); uart_puts("\n");
    uart_puts("Class: "); uart_puts(get_exception_class_name(ec));
    uart_puts(" (0x"); uart_puthex(ec); uart_puts(")\n");
    uart_puts("ISS:   "); uart_puthex(iss); uart_puts("\n");

    dump_exception_frame(frame);

    uart_puts("\nSystem halted.\n");
    while (1) {
        __asm__ volatile("wfe");
    }
}

/*
 * IRQ exception handler
 */
void handle_irq_exception(struct exception_frame *frame)
{
    uint32_t irq;

    (void)frame;  /* Unused in normal IRQ handling */

    /* Acknowledge interrupt and get IRQ number */
    irq = gic_acknowledge_interrupt();

    /* Spurious interrupt check */
    if (irq >= 1020) {
        return;
    }

    /* Dispatch to registered handler */
    irq_handle(irq);

    /* Signal end of interrupt */
    gic_end_of_interrupt(irq);
}

/*
 * FIQ exception handler
 */
void handle_fiq_exception(struct exception_frame *frame)
{
    uart_puts("\n*** FIQ EXCEPTION ***\n");
    dump_exception_frame(frame);
    uart_puts("\nSystem halted.\n");
    while (1) {
        __asm__ volatile("wfe");
    }
}

/*
 * SError exception handler
 */
void handle_serror_exception(struct exception_frame *frame, uint64_t esr, uint64_t far)
{
    uart_puts("\n*** SERROR EXCEPTION ***\n");
    uart_puts("ESR: "); uart_puthex(esr); uart_puts("\n");
    uart_puts("FAR: "); uart_puthex(far); uart_puts("\n");

    dump_exception_frame(frame);

    uart_puts("\nSystem halted.\n");
    while (1) {
        __asm__ volatile("wfe");
    }
}
