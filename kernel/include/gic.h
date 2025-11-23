/*
 * AArch64 Bare Metal OS - GIC (Generic Interrupt Controller) Header
 *
 * GICv2 support for QEMU virt machine
 */

#ifndef _GIC_H
#define _GIC_H

#include "kernel.h"

/* GIC Distributor registers */
#define GICD_CTLR           0x000                       /* Control Register */
#define GICD_TYPER          0x004                       /* Type Register */
#define GICD_IIDR           0x008                       /* Implementer ID */
#define GICD_IGROUPR        0x080                       /* Interrupt Group */
#define GICD_ISENABLER      0x100                       /* Interrupt Set-Enable */
#define GICD_ICENABLER      0x180                       /* Interrupt Clear-Enable */
#define GICD_ISPENDR        0x200                       /* Interrupt Set-Pending */
#define GICD_ICPENDR        0x280                       /* Interrupt Clear-Pending */
#define GICD_ISACTIVER      0x300                       /* Interrupt Set-Active */
#define GICD_ICACTIVER      0x380                       /* Interrupt Clear-Active */
#define GICD_IPRIORITYR     0x400                       /* Interrupt Priority */
#define GICD_ITARGETSR      0x800                       /* Interrupt Targets */
#define GICD_ICFGR          0xC00                       /* Interrupt Config */
#define GICD_SGIR           0xF00                       /* Software Generated Interrupt */

/* GIC CPU Interface registers */
#define GICC_CTLR           0x000                       /* Control Register */
#define GICC_PMR            0x004                       /* Priority Mask */
#define GICC_BPR            0x008                       /* Binary Point */
#define GICC_IAR            0x00C                       /* Interrupt Acknowledge */
#define GICC_EOIR           0x010                       /* End of Interrupt */
#define GICC_RPR            0x014                       /* Running Priority */
#define GICC_HPPIR          0x018                       /* Highest Priority Pending */

/* GICD_CTLR bits */
#define GICD_CTLR_ENABLE    (1 << 0)                    /* Enable distributor */

/* GICC_CTLR bits */
#define GICC_CTLR_ENABLE    (1 << 0)                    /* Enable CPU interface */
#define GICC_CTLR_FIQEN     (1 << 3)                    /* Enable FIQ */

/* Interrupt types */
#define GIC_SGI_BASE        0                           /* Software Generated */
#define GIC_PPI_BASE        16                          /* Private Peripheral */
#define GIC_SPI_BASE        32                          /* Shared Peripheral */

/* Common interrupts (QEMU virt) */
#define IRQ_TIMER_VIRT      27                          /* Virtual timer */
#define IRQ_TIMER_PHYS      30                          /* Physical timer */
#define IRQ_UART0           33                          /* UART0 */

/* Function prototypes */
void gic_init(void);
void gic_enable_interrupt(uint32_t irq);
void gic_disable_interrupt(uint32_t irq);
void gic_set_priority(uint32_t irq, uint8_t priority);
uint32_t gic_acknowledge_interrupt(void);
void gic_end_of_interrupt(uint32_t irq);

#endif /* _GIC_H */
