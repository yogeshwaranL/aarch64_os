/*
 * AArch64 Bare Metal OS - GIC Driver Implementation
 *
 * GICv2 support for interrupt handling
 */

#include "kernel.h"
#include "gic.h"
#include "uart.h"

/* GIC register access macros */
#define GICD_REG(offset)    (*(volatile uint32_t *)(GIC_DIST_BASE + (offset)))
#define GICC_REG(offset)    (*(volatile uint32_t *)(GIC_CPU_BASE + (offset)))

/*
 * Initialize GIC Distributor
 */
static void gicd_init(void)
{
    uint32_t typer, num_irqs;
    int i;

    uart_puts("Initializing GIC Distributor...\n");

    /* Disable distributor */
    GICD_REG(GICD_CTLR) = 0;

    /* Get number of interrupt lines */
    typer = GICD_REG(GICD_TYPER);
    num_irqs = 32 * ((typer & 0x1F) + 1);

    uart_puts("  IRQ lines: ");
    uart_puthex(num_irqs);
    uart_puts("\n");

    /* Disable all interrupts */
    for (i = 0; i < (int)num_irqs; i += 32) {
        GICD_REG(GICD_ICENABLER + (i / 32) * 4) = 0xFFFFFFFF;
    }

    /* Clear all pending interrupts */
    for (i = 0; i < (int)num_irqs; i += 32) {
        GICD_REG(GICD_ICPENDR + (i / 32) * 4) = 0xFFFFFFFF;
    }

    /* Set all interrupts to lowest priority */
    for (i = 0; i < (int)num_irqs; i += 4) {
        GICD_REG(GICD_IPRIORITYR + (i / 4) * 4) = 0xA0A0A0A0;
    }

    /* Set all SPIs to target CPU 0 */
    for (i = 32; i < (int)num_irqs; i += 4) {
        GICD_REG(GICD_ITARGETSR + (i / 4) * 4) = 0x01010101;
    }

    /* Set all interrupts to level-triggered */
    for (i = 0; i < (int)num_irqs; i += 16) {
        GICD_REG(GICD_ICFGR + (i / 16) * 4) = 0;
    }

    /* Enable distributor */
    GICD_REG(GICD_CTLR) = GICD_CTLR_ENABLE;

    uart_puts("  GIC Distributor enabled\n");
}

/*
 * Initialize GIC CPU Interface
 */
static void gicc_init(void)
{
    uart_puts("Initializing GIC CPU Interface...\n");

    /* Set priority mask to allow all priorities */
    GICC_REG(GICC_PMR) = 0xFF;

    /* Set binary point to 0 (no grouping) */
    GICC_REG(GICC_BPR) = 0;

    /* Enable CPU interface */
    GICC_REG(GICC_CTLR) = GICC_CTLR_ENABLE;

    uart_puts("  GIC CPU Interface enabled\n");
}

/*
 * Initialize GIC
 */
void gic_init(void)
{
    uint32_t iidr;

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Initializing GIC\n");
    uart_puts("========================================\n");

    /* Read GIC ID */
    iidr = GICD_REG(GICD_IIDR);
    uart_puts("GIC ID: ");
    uart_puthex(iidr);
    uart_puts("\n");

    /* Initialize distributor */
    gicd_init();

    /* Initialize CPU interface */
    gicc_init();

    uart_puts("========================================\n");
    uart_puts("GIC Initialization Complete\n");
    uart_puts("========================================\n");
    uart_puts("\n");
}

/*
 * Enable an interrupt
 */
void gic_enable_interrupt(uint32_t irq)
{
    uint32_t reg_offset = (irq / 32) * 4;
    uint32_t bit = 1U << (irq % 32);

    GICD_REG(GICD_ISENABLER + reg_offset) = bit;
}

/*
 * Disable an interrupt
 */
void gic_disable_interrupt(uint32_t irq)
{
    uint32_t reg_offset = (irq / 32) * 4;
    uint32_t bit = 1U << (irq % 32);

    GICD_REG(GICD_ICENABLER + reg_offset) = bit;
}

/*
 * Set interrupt priority
 */
void gic_set_priority(uint32_t irq, uint8_t priority)
{
    uint32_t reg_offset = (irq / 4) * 4;
    uint32_t shift = (irq % 4) * 8;
    uint32_t val;

    val = GICD_REG(GICD_IPRIORITYR + reg_offset);
    val &= ~(0xFF << shift);
    val |= (priority << shift);
    GICD_REG(GICD_IPRIORITYR + reg_offset) = val;
}

/*
 * Acknowledge an interrupt
 */
uint32_t gic_acknowledge_interrupt(void)
{
    return GICC_REG(GICC_IAR);
}

/*
 * Signal end of interrupt
 */
void gic_end_of_interrupt(uint32_t irq)
{
    GICC_REG(GICC_EOIR) = irq;
}
