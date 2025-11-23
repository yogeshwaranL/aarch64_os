/*
 * AArch64 Bare Metal OS - IRQ Management Header
 *
 * IRQ handler registration and dispatch
 */

#ifndef _IRQ_H
#define _IRQ_H

#include "kernel.h"

/* IRQ handler function type */
typedef void (*irq_handler_t)(uint32_t irq, void *data);

/* IRQ handler registration */
struct irq_handler {
    irq_handler_t handler;
    void *data;
    const char *name;
    uint32_t count;                             /* Number of times handled */
};

/* Maximum number of IRQs (matching GIC capability) */
#define MAX_IRQS        288

/* Function prototypes */
void irq_init(void);
int irq_register_handler(uint32_t irq, irq_handler_t handler, void *data, const char *name);
void irq_unregister_handler(uint32_t irq);
void irq_handle(uint32_t irq);
void irq_dump_stats(void);

#endif /* _IRQ_H */
