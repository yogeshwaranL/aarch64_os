/*
 * AArch64 Bare Metal OS - Kernel Main
 */

#include "kernel.h"
#include "uart.h"
#include "mmu.h"
#include "pmm.h"
#include "kmalloc.h"
#include "gic.h"
#include "irq.h"
#include "timer.h"
#include "exception.h"

/* Current exception level (set by entry.S) */
static uint64_t current_el = 0;

/* Device tree blob pointer */
static void *device_tree = NULL;

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

    /* Phase 5: Initialize Memory Management */
    pmm_init();                                         /* Physical memory manager */
    mmu_init();                                         /* MMU and page tables */
    kmalloc_init();                                     /* Kernel heap */

    /* Phase 5: Initialize GIC */
    gic_init();                                         /* Interrupt controller */

    /* Phase 6: Initialize IRQ subsystem */
    irq_init();                                         /* IRQ dispatch */

    /* Phase 6: Initialize Timer */
    timer_init();                                       /* ARM Generic Timer */

    /* Enable IRQs at CPU level */
    uart_puts("Enabling IRQs at CPU level...\n");
    __asm__ volatile("msr daifclr, #2");                /* Clear IRQ mask (bit 1) */
    uart_puts("IRQs enabled!\n\n");

    /* Test memory allocator */
    uart_puts("Testing memory allocator:\n");
    void *ptr1 = kmalloc(64);
    if (ptr1) {
        uart_puts("  kmalloc(64): ");
        uart_puthex((uint64_t)ptr1);
        uart_puts(" - OK\n");
        kfree(ptr1);
        uart_puts("  kfree(): OK\n");
    }

    /* Dump statistics */
    pmm_dump_stats();
    kmalloc_dump_stats();

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Kernel initialization complete!\n");
    uart_puts("========================================\n");
    uart_puts("\n");

    /* Display IRQ statistics to show timer interrupts working */
    uart_puts("Waiting 2 seconds to collect timer interrupts...\n");
    timer_delay_ms(2000);
    irq_dump_stats();

    /* Main kernel loop */
    uart_puts("Entering kernel main loop...\n");
    uart_puts("(Timer interrupts running in background at 100 Hz)\n");
    uart_puts("(Press any key to see echo, Ctrl-A X to exit QEMU)\n");
    uart_puts("\n");

    /* Simple echo loop */
    while (1) {
        char c = uart_getc();
        uart_puts("Received: '");
        uart_putc(c);
        uart_puts("' (0x");

        /* Print hex value of character */
        const char hex[] = "0123456789ABCDEF";
        uart_putc(hex[(c >> 4) & 0xF]);
        uart_putc(hex[c & 0xF]);
        uart_puts(") | Uptime: ");
        uart_puthex(timer_get_uptime_ms());
        uart_puts(" ms\n");
    }

    /* Should never reach here */
    while (1) {
        __asm__ volatile("wfe");
    }
}

