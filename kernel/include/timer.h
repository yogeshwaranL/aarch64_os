/*
 * AArch64 Bare Metal OS - ARM Generic Timer Header
 *
 * ARM Generic Timer support for system tick and scheduling
 */

#ifndef _TIMER_H
#define _TIMER_H

#include "kernel.h"

/* Timer frequency (QEMU virt uses 62.5 MHz) */
#define TIMER_FREQ      62500000UL              /* 62.5 MHz */

/* Timer tick interval (default: 10ms = 100 Hz) */
#define TIMER_HZ        100                     /* 100 ticks per second */
#define TIMER_INTERVAL  (TIMER_FREQ / TIMER_HZ) /* Ticks per interval */

/* Timer control register bits */
#define TIMER_CTRL_ENABLE   (1 << 0)            /* Enable timer */
#define TIMER_CTRL_IMASK    (1 << 1)            /* Interrupt mask */
#define TIMER_CTRL_ISTATUS  (1 << 2)            /* Interrupt status */

/* Function prototypes */
void timer_init(void);
uint64_t timer_get_ticks(void);
uint64_t timer_get_uptime_ms(void);
void timer_delay_ms(uint32_t ms);

#endif /* _TIMER_H */
