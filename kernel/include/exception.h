/*
 * AArch64 Bare Metal OS - Exception Handler Header
 *
 * Exception types and handler prototypes
 */

#ifndef _EXCEPTION_H
#define _EXCEPTION_H

#include "kernel.h"

/* Exception types */
#define EXC_SYNC        0                       /* Synchronous exception */
#define EXC_IRQ         1                       /* IRQ */
#define EXC_FIQ         2                       /* FIQ */
#define EXC_SERROR      3                       /* SError */

/* Exception sources */
#define EXC_SRC_CURR_EL_SP0     0               /* Current EL with SP0 */
#define EXC_SRC_CURR_EL_SPX     1               /* Current EL with SPx */
#define EXC_SRC_LOWER_EL_64     2               /* Lower EL (AArch64) */
#define EXC_SRC_LOWER_EL_32     3               /* Lower EL (AArch32) */

/* Saved register context */
struct exception_frame {
    /* General purpose registers */
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7;
    uint64_t x8, x9, x10, x11, x12, x13, x14, x15;
    uint64_t x16, x17, x18, x19, x20, x21, x22, x23;
    uint64_t x24, x25, x26, x27, x28, x29, x30;    /* x30 = LR */

    /* Stack pointer and exception return */
    uint64_t sp;
    uint64_t elr;                               /* Exception Link Register */
    uint64_t spsr;                              /* Saved Program Status Register */
};

/* Exception handler prototypes (called from assembly) */
void handle_sync_exception(struct exception_frame *frame, uint64_t esr, uint64_t far);
void handle_irq_exception(struct exception_frame *frame);
void handle_fiq_exception(struct exception_frame *frame);
void handle_serror_exception(struct exception_frame *frame, uint64_t esr, uint64_t far);

/* Exception utilities */
void dump_exception_frame(struct exception_frame *frame);
const char *get_exception_class_name(uint32_t ec);

#endif /* _EXCEPTION_H */
