/*
 * AArch64 Bare Metal OS - ARM Generic Timer Driver
 *
 * System timer using ARM Generic Timer (EL1 Physical Timer)
 */

#include "kernel.h"
#include "timer.h"
#include "irq.h"
#include "gic.h"
#include "uart.h"
#include "sched.h"

/* Global tick counter */
static volatile uint64_t system_ticks = 0;

/*
 * Timer interrupt handler
 */
static void timer_irq_handler(uint32_t irq, void *data)
{
    (void)irq;
    (void)data;

    /* Increment tick counter */
    system_ticks++;

    /* Call scheduler for preemptive multitasking */
    schedule();

    /* Set next timer interrupt */
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"((uint64_t)TIMER_INTERVAL));
}

/*
 * Initialize ARM Generic Timer
 */
void timer_init(void)
{
    uint64_t freq;
    uint64_t ctrl;

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Initializing ARM Generic Timer\n");
    uart_puts("========================================\n");

    /* Read timer frequency */
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    uart_puts("Timer frequency: ");
    uart_puthex(freq);
    uart_puts(" Hz (");
    uart_puthex(freq / 1000000);
    uart_puts(" MHz)\n");

    /* Verify frequency matches expected */
    if (freq != TIMER_FREQ) {
        uart_puts("WARNING: Expected ");
        uart_puthex(TIMER_FREQ);
        uart_puts(" Hz, got ");
        uart_puthex(freq);
        uart_puts(" Hz\n");
    }

    uart_puts("Timer interval: ");
    uart_puthex(TIMER_INTERVAL);
    uart_puts(" ticks (");
    uart_puthex(TIMER_HZ);
    uart_puts(" Hz)\n");

    /* Register timer IRQ handler */
    irq_register_handler(IRQ_TIMER_PHYS, timer_irq_handler, NULL, "ARM Generic Timer");

    /* Set initial timer value */
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"((uint64_t)TIMER_INTERVAL));

    /* Enable timer: unmask interrupt and enable */
    ctrl = TIMER_CTRL_ENABLE;                   /* Enable, interrupt unmasked */
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(ctrl));
    __asm__ volatile("isb");

    uart_puts("Timer enabled and running\n");
    uart_puts("========================================\n");
    uart_puts("\n");
}

/*
 * Get current system tick count
 */
uint64_t timer_get_ticks(void)
{
    return system_ticks;
}

/*
 * Get uptime in milliseconds
 */
uint64_t timer_get_uptime_ms(void)
{
    return (system_ticks * 1000) / TIMER_HZ;
}

/*
 * Delay for specified milliseconds
 * NOTE: This is a busy-wait delay, not recommended for production use
 */
void timer_delay_ms(uint32_t ms)
{
    uint64_t start = timer_get_uptime_ms();
    while ((timer_get_uptime_ms() - start) < ms) {
        __asm__ volatile("nop");
    }
}
