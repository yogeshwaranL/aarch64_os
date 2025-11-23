/*
 * AArch64 Bare Metal OS - IRQ Management
 *
 * IRQ handler registration and dispatch system
 */

#include "kernel.h"
#include "irq.h"
#include "uart.h"

/* IRQ handler table */
static struct irq_handler irq_handlers[MAX_IRQS];

/*
 * Initialize IRQ subsystem
 */
void irq_init(void)
{
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Initializing IRQ Subsystem\n");
    uart_puts("========================================\n");

    /* Clear all handlers */
    for (uint32_t i = 0; i < MAX_IRQS; i++) {
        irq_handlers[i].handler = NULL;
        irq_handlers[i].data = NULL;
        irq_handlers[i].name = NULL;
        irq_handlers[i].count = 0;
    }

    uart_puts("IRQ subsystem initialized\n");
    uart_puts("Maximum IRQs: ");
    uart_puthex(MAX_IRQS);
    uart_puts("\n");
    uart_puts("========================================\n");
}

/*
 * Register an IRQ handler
 */
int irq_register_handler(uint32_t irq, irq_handler_t handler, void *data, const char *name)
{
    if (irq >= MAX_IRQS) {
        return -1;
    }

    if (irq_handlers[irq].handler != NULL) {
        uart_puts("Warning: IRQ ");
        uart_puthex(irq);
        uart_puts(" already has a handler\n");
        return -1;
    }

    irq_handlers[irq].handler = handler;
    irq_handlers[irq].data = data;
    irq_handlers[irq].name = name;
    irq_handlers[irq].count = 0;

    uart_puts("Registered IRQ ");
    uart_puthex(irq);
    uart_puts(": ");
    uart_puts(name);
    uart_puts("\n");

    return 0;
}

/*
 * Unregister an IRQ handler
 */
void irq_unregister_handler(uint32_t irq)
{
    if (irq >= MAX_IRQS) {
        return;
    }

    irq_handlers[irq].handler = NULL;
    irq_handlers[irq].data = NULL;
    irq_handlers[irq].name = NULL;
    irq_handlers[irq].count = 0;
}

/*
 * Handle an IRQ
 */
void irq_handle(uint32_t irq)
{
    if (irq >= MAX_IRQS) {
        uart_puts("Invalid IRQ number: ");
        uart_puthex(irq);
        uart_puts("\n");
        return;
    }

    if (irq_handlers[irq].handler == NULL) {
        uart_puts("No handler for IRQ ");
        uart_puthex(irq);
        uart_puts("\n");
        return;
    }

    /* Increment count and call handler */
    irq_handlers[irq].count++;
    irq_handlers[irq].handler(irq, irq_handlers[irq].data);
}

/*
 * Dump IRQ statistics
 */
void irq_dump_stats(void)
{
    uart_puts("\nIRQ Statistics:\n");
    uart_puts("===============\n");

    for (uint32_t i = 0; i < MAX_IRQS; i++) {
        if (irq_handlers[i].handler != NULL) {
            uart_puts("IRQ ");
            uart_puthex(i);
            uart_puts(": ");
            uart_puts(irq_handlers[i].name);
            uart_puts(" (count: ");
            uart_puthex(irq_handlers[i].count);
            uart_puts(")\n");
        }
    }
}
