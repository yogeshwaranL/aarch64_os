/*
 * AArch64 Bare Metal OS - Kernel Main
 */

#include "kernel.h"
#include "uart.h"

/* Current exception level (set by entry.S) */
static uint64_t current_el = 0;

/* Device tree blob pointer */
static void *device_tree = NULL;

/* Exception type names */
static const char *exception_names[] = {
    "Synchronous",
    "IRQ",
    "FIQ",
    "SError"
};

/*
 * Print kernel banner
 */
static void print_banner(void)
{
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("  AArch64 Bare Metal OS\n");
    uart_puts("  Version ");
    uart_putc('0' + KERNEL_VERSION_MAJOR);
    uart_putc('.');
    uart_putc('0' + KERNEL_VERSION_MINOR);
    uart_putc('.');
    uart_putc('0' + KERNEL_VERSION_PATCH);
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("\n");
}

/*
 * Print system information
 */
static void print_system_info(void)
{
    uint64_t midr, mpidr;

    /* Read CPU ID registers */
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));

    uart_puts("System Information:\n");
    uart_puts("  Exception Level: EL");
    uart_putc('0' + current_el);
    uart_puts("\n");

    uart_puts("  CPU ID (MIDR):   ");
    uart_puthex(midr);
    uart_puts("\n");

    uart_puts("  CPU Affinity:    ");
    uart_puthex(mpidr);
    uart_puts("\n");

    if (device_tree) {
        uart_puts("  Device Tree:     ");
        uart_puthex((uint64_t)device_tree);
        uart_puts("\n");
    } else {
        uart_puts("  Device Tree:     Not provided\n");
    }

    uart_puts("\n");
}

/*
 * Test basic functionality
 */
static void test_basic_functions(void)
{
    uart_puts("Testing basic functions:\n");

    uart_puts("  [1] UART output:      OK\n");
    uart_puts("  [2] String functions: ");

    /* Test memset */
    char buffer[16];
    memset(buffer, 'A', sizeof(buffer));
    buffer[15] = '\0';
    if (buffer[0] == 'A' && buffer[14] == 'A') {
        uart_puts("OK\n");
    } else {
        uart_puts("FAIL\n");
    }

    uart_puts("  [3] Exception level:  ");
    if (current_el == 2) {
        uart_puts("EL2 (Hypervisor mode) - Perfect for Type-1 HV\n");
    } else if (current_el == 1) {
        uart_puts("EL1 (Kernel mode)\n");
    } else {
        uart_puts("Unknown\n");
    }

    uart_puts("\n");
}

/*
 * Kernel main function
 *
 * Called from entry.S after basic initialization
 * x0 = device tree blob pointer (or NULL)
 * x1 = current exception level
 */
void kernel_main(void *dtb, uint64_t el)
{
    /* Save parameters */
    device_tree = dtb;
    current_el = el;

    /* Initialize UART for console output */
    uart_init();

    /* Print banner */
    print_banner();

    /* Print system information */
    print_system_info();

    /* Test basic functions */
    test_basic_functions();

    uart_puts("Kernel initialization complete!\n");
    uart_puts("\n");

    /* Main kernel loop */
    uart_puts("Entering kernel main loop...\n");
    uart_puts("(Press any key to see echo, Ctrl-A X to exit QEMU)\n");
    uart_puts("\n");

    /* Simple echo loop for Phase 4 */
    while (1) {
        char c = uart_getc();
        uart_puts("Received: '");
        uart_putc(c);
        uart_puts("' (0x");

        /* Print hex value of character */
        const char hex[] = "0123456789ABCDEF";
        uart_putc(hex[(c >> 4) & 0xF]);
        uart_putc(hex[c & 0xF]);
        uart_puts(")\n");
    }

    /* Should never reach here */
    while (1) {
        __asm__ volatile("wfe");
    }
}

/*
 * Exception handler (called from entry.S)
 *
 * el = exception level (1 or 2)
 * type = exception type (0=sync, 1=irq, 2=fiq, 3=serror)
 */
void handle_exception(uint64_t el, uint64_t type)
{
    uint64_t esr, elr, far;

    uart_puts("\n*** EXCEPTION ***\n");
    uart_puts("Exception Level: EL");
    uart_putc('0' + el);
    uart_puts("\n");

    if (type < ARRAY_SIZE(exception_names)) {
        uart_puts("Type: ");
        uart_puts(exception_names[type]);
        uart_puts("\n");
    }

    /* Read exception syndrome register */
    if (el == 2) {
        __asm__ volatile("mrs %0, esr_el2" : "=r"(esr));
        __asm__ volatile("mrs %0, elr_el2" : "=r"(elr));
        __asm__ volatile("mrs %0, far_el2" : "=r"(far));
    } else {
        __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
        __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
        __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    }

    uart_puts("ESR: ");
    uart_puthex(esr);
    uart_puts("\n");

    uart_puts("ELR: ");
    uart_puthex(elr);
    uart_puts("\n");

    uart_puts("FAR: ");
    uart_puthex(far);
    uart_puts("\n");

    uart_puts("\nHalting system...\n");

    /* Halt */
    while (1) {
        __asm__ volatile("wfe");
    }
}
