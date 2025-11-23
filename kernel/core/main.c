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
#include "sched.h"
#include "task.h"
#include "syscall.h"
#include "hypervisor.h"
#include "user_programs.h"

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
 * Test task functions for demonstrating preemptive multitasking
 */
static void task_a_func(void *arg)
{
    int i = 0;
    (void)arg;

    while (1) {
        sys_write("[Task A] Running iteration ");
        uart_puthex(i++);
        sys_write("\n");
        sys_sleep(500);  /* Sleep 500ms */
    }
}

static void task_b_func(void *arg)
{
    int i = 0;
    (void)arg;

    while (1) {
        sys_write("[Task B] Running iteration ");
        uart_puthex(i++);
        sys_write("\n");
        sys_sleep(700);  /* Sleep 700ms */
    }
}

static void task_c_func(void *arg)
{
    int i = 0;
    (void)arg;

    while (1) {
        sys_write("[Task C] Running iteration ");
        uart_puthex(i++);
        sys_write("\n");
        sys_sleep(1000);  /* Sleep 1000ms */
    }
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

    /* Phase 8: Initialize Hypervisor (if at EL2) */
    if (current_el == 2) {
        hypervisor_init();
    }

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

    /* Phase 7: Initialize Scheduler */
    sched_init();                                       /* Scheduler and idle task */

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

    /* Phase 8: Test Hypervisor Calls (if at EL2) */
    if (current_el == 2) {
        uart_puts("========================================\n");
        uart_puts("Testing Hypervisor Calls (HVC)\n");
        uart_puts("========================================\n\n");

        /* Test HVC: Get hypervisor version */
        uint64_t version;
        __asm__ volatile(
            "mov x0, %1\n"
            "hvc #0\n"
            "mov %0, x0\n"
            : "=r"(version)
            : "i"(HVC_GET_VERSION)
            : "x0"
        );
        uart_puts("HVC_GET_VERSION:   ");
        uart_puthex(version);
        uart_puts(" (v");
        uart_putc('0' + ((version >> 16) & 0xFF));
        uart_putc('.');
        uart_putc('0' + (version & 0xFF));
        uart_puts(")\n");

        /* Test HVC: Console putc */
        uart_puts("HVC_CONSOLE_PUTC:  ");
        __asm__ volatile(
            "mov x0, %0\n"
            "mov x1, %1\n"
            "hvc #0\n"
            :
            : "i"(HVC_CONSOLE_PUTC), "i"('H')
            : "x0", "x1"
        );
        __asm__ volatile(
            "mov x0, %0\n"
            "mov x1, %1\n"
            "hvc #0\n"
            :
            : "i"(HVC_CONSOLE_PUTC), "i"('V')
            : "x0", "x1"
        );
        __asm__ volatile(
            "mov x0, %0\n"
            "mov x1, %1\n"
            "hvc #0\n"
            :
            : "i"(HVC_CONSOLE_PUTC), "i"('C')
            : "x0", "x1"
        );
        __asm__ volatile(
            "mov x0, %0\n"
            "mov x1, %1\n"
            "hvc #0\n"
            :
            : "i"(HVC_CONSOLE_PUTC), "i"('!')
            : "x0", "x1"
        );
        uart_puts("\n");

        /* Test HVC: Get VM info */
        uint64_t vm_info;
        __asm__ volatile(
            "mov x0, %1\n"
            "hvc #0\n"
            "mov %0, x0\n"
            : "=r"(vm_info)
            : "i"(HVC_VM_INFO)
            : "x0"
        );
        uart_puts("HVC_VM_INFO:       EL");
        uart_putc('0' + vm_info);
        uart_puts("\n");

        /* Display hypervisor state */
        hypervisor_dump_state();
    }

    /* Phase 7: Create test tasks to demonstrate multitasking */
    uart_puts("Creating test tasks...\n");
    task_create("Task-A", task_a_func, NULL, 10);
    task_create("Task-B", task_b_func, NULL, 10);
    task_create("Task-C", task_c_func, NULL, 10);
    uart_puts("\n");

    /* Phase 9: Create user mode tasks to demonstrate EL0 execution */
    uart_puts("========================================\n");
    uart_puts("Creating User Mode Tasks\n");
    uart_puts("========================================\n");

    /* Calculate binary sizes */
    size_t hello_size = (size_t)(_binary_user_hello_bin_end - _binary_user_hello_bin_start);
    size_t counter_size = (size_t)(_binary_user_counter_bin_end - _binary_user_counter_bin_start);

    /* Create user tasks */
    task_create_user("user-hello", _binary_user_hello_bin_start, hello_size, 10);
    task_create_user("user-counter", _binary_user_counter_bin_start, counter_size, 10);

    uart_puts("User tasks created!\n");
    uart_puts("========================================\n");
    uart_puts("\n");

    /* Display task list */
    sched_dump_tasks();

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Starting Preemptive Multitasking!\n");
    uart_puts("========================================\n");
    uart_puts("\n");

    /* Kernel now enters idle loop - scheduler will run tasks */
    while (1) {
        __asm__ volatile("wfi");  /* Wait for interrupt */
    }
}

