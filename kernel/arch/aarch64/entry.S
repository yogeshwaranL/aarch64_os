/*
 * AArch64 Bare Metal OS - Kernel Entry Point
 *
 * This is the first code executed when UEFI hands control to the kernel.
 * Entry can be at EL2 or EL1 depending on UEFI configuration.
 *
 * We will:
 * 1. Determine current exception level
 * 2. Set up initial stack
 * 3. Initialize exception vectors
 * 4. Set up basic MMU (identity mapping)
 * 5. Jump to C code (kernel_main)
 */

.section ".text.boot"

.global _start
.type _start, %function

_start:
    /*
     * Entry point - we're here from UEFI
     * x0 = device tree blob pointer (or 0)
     * x1-x3 = reserved/unused
     */

    /* Save device tree pointer (passed by UEFI) */
    mov     x20, x0

    /* Get current exception level */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2              /* CurrentEL is in bits [3:2] */
    cmp     x0, #2
    b.eq    el2_entry
    cmp     x0, #1
    b.eq    el1_entry

    /* If we're at EL3 or EL0, something is wrong */
    b       hang

el2_entry:
    /*
     * We're at EL2 (hypervisor mode)
     * This is where we want to be for our Type-1 hypervisor
     * For now, we'll stay at EL2
     */

    /* Disable MMU and caches for now */
    mrs     x0, sctlr_el2
    bic     x0, x0, #(1 << 0)       /* Clear M bit (MMU) */
    bic     x0, x0, #(1 << 2)       /* Clear C bit (D-cache) */
    bic     x0, x0, #(1 << 12)      /* Clear I bit (I-cache) */
    msr     sctlr_el2, x0
    isb

    /* Set up stack pointer for EL2 */
    ldr     x0, =__stack_top_el2
    mov     sp, x0

    /* Set up exception vector table for EL2 */
    ldr     x0, =exception_vectors_el2
    msr     vbar_el2, x0
    isb

    /* Configure HCR_EL2 (Hypervisor Configuration Register) */
    mov     x0, #(1 << 31)          /* RW bit: EL1 is AArch64 */
    msr     hcr_el2, x0
    isb

    /* Enable FP/SIMD at EL2 (CPTR_EL2) */
    msr     cptr_el2, xzr           /* Clear all trap bits */
    isb

    /* Jump to common initialization */
    b       common_init

el1_entry:
    /*
     * We're at EL1 (kernel mode)
     * This happens if UEFI already dropped us to EL1
     */

    /* Disable MMU and caches */
    mrs     x0, sctlr_el1
    bic     x0, x0, #(1 << 0)       /* Clear M bit (MMU) */
    bic     x0, x0, #(1 << 2)       /* Clear C bit (D-cache) */
    bic     x0, x0, #(1 << 12)      /* Clear I bit (I-cache) */
    msr     sctlr_el1, x0
    isb

    /* Set up stack pointer for EL1 */
    ldr     x0, =__stack_top_el1
    mov     sp, x0

    /* Set up exception vector table for EL1 */
    ldr     x0, =exception_vectors_el1
    msr     vbar_el1, x0
    isb

    /* Fall through to common_init */

common_init:
    /* Enable FP/SIMD (prevents traps from compiler-generated SIMD code) */
    mrs     x0, cpacr_el1
    orr     x0, x0, #(3 << 20)      /* FPEN bits: no trapping to EL1 */
    msr     cpacr_el1, x0
    isb

    /* Clear BSS section */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    sub     x1, x1, x0              /* x1 = size of BSS */
    cbz     x1, bss_done            /* Skip if BSS is empty */

clear_bss:
    str     xzr, [x0], #8           /* Clear 8 bytes, increment pointer */
    subs    x1, x1, #8
    b.gt    clear_bss

bss_done:
    /* Restore device tree pointer to x0 for kernel_main */
    mov     x0, x20

    /* Get current exception level again for kernel_main */
    mrs     x1, CurrentEL
    lsr     x1, x1, #2

    /* Jump to C code */
    bl      kernel_main

    /* If kernel_main returns, hang */
hang:
    wfe
    b       hang

.size _start, . - _start

/*
 * Exception Vector Tables
 *
 * ARM requires exception vectors to be aligned to 2KB (0x800)
 * Each exception level has its own vector table
 * Each table has 16 entries of 0x80 bytes each
 */

.balign 0x800
exception_vectors_el2:
    /* Current EL with SP0 */
    .balign 0x80
    b       sync_exception_el2      /* Synchronous */
    .balign 0x80
    b       irq_exception_el2       /* IRQ */
    .balign 0x80
    b       fiq_exception_el2       /* FIQ */
    .balign 0x80
    b       serror_exception_el2    /* SError */

    /* Current EL with SPx */
    .balign 0x80
    b       sync_exception_el2
    .balign 0x80
    b       irq_exception_el2
    .balign 0x80
    b       fiq_exception_el2
    .balign 0x80
    b       serror_exception_el2

    /* Lower EL using AArch64 */
    .balign 0x80
    b       sync_exception_el2
    .balign 0x80
    b       irq_exception_el2
    .balign 0x80
    b       fiq_exception_el2
    .balign 0x80
    b       serror_exception_el2

    /* Lower EL using AArch32 */
    .balign 0x80
    b       sync_exception_el2
    .balign 0x80
    b       irq_exception_el2
    .balign 0x80
    b       fiq_exception_el2
    .balign 0x80
    b       serror_exception_el2

.balign 0x800
exception_vectors_el1:
    /* Current EL with SP0 */
    .balign 0x80
    b       sync_exception_el1
    .balign 0x80
    b       irq_exception_el1
    .balign 0x80
    b       fiq_exception_el1
    .balign 0x80
    b       serror_exception_el1

    /* Current EL with SPx */
    .balign 0x80
    b       sync_exception_el1
    .balign 0x80
    b       irq_exception_el1
    .balign 0x80
    b       fiq_exception_el1
    .balign 0x80
    b       serror_exception_el1

    /* Lower EL using AArch64 */
    .balign 0x80
    b       sync_exception_el1
    .balign 0x80
    b       irq_exception_el1
    .balign 0x80
    b       fiq_exception_el1
    .balign 0x80
    b       serror_exception_el1

    /* Lower EL using AArch32 */
    .balign 0x80
    b       sync_exception_el1
    .balign 0x80
    b       irq_exception_el1
    .balign 0x80
    b       fiq_exception_el1
    .balign 0x80
    b       serror_exception_el1

/*
 * Exception handlers (Phase 6: Full context save/restore)
 */

/*
 * Macro to save exception context
 * Saves all registers to match struct exception_frame
 */
.macro save_context
    /* Allocate space for exception_frame (272 bytes) */
    sub     sp, sp, #272

    /* Save x0-x30 */
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30, [sp, #240]

    /* Save SP (before exception) */
    add     x0, sp, #272            /* Original SP */
    str     x0, [sp, #248]

    /* Save ELR and SPSR */
    mrs     x0, elr_el1
    mrs     x1, spsr_el1
    stp     x0, x1, [sp, #256]
.endm

/*
 * Macro to restore exception context
 */
.macro restore_context
    /* Restore ELR and SPSR */
    ldp     x0, x1, [sp, #256]
    msr     elr_el1, x0
    msr     spsr_el1, x1

    /* Restore x0-x30 */
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30, [sp, #240]

    /* Deallocate exception_frame */
    add     sp, sp, #272
.endm

.global sync_exception_el2
.global irq_exception_el2
.global fiq_exception_el2
.global serror_exception_el2

sync_exception_el2:
    save_context
    mov     x0, sp                  /* exception_frame pointer */
    mrs     x1, esr_el2             /* ESR */
    mrs     x2, far_el2             /* FAR */
    bl      handle_sync_exception
    restore_context
    eret

irq_exception_el2:
    save_context
    mov     x0, sp                  /* exception_frame pointer */
    bl      handle_irq_exception
    restore_context
    eret

fiq_exception_el2:
    save_context
    mov     x0, sp                  /* exception_frame pointer */
    bl      handle_fiq_exception
    restore_context
    eret

serror_exception_el2:
    save_context
    mov     x0, sp                  /* exception_frame pointer */
    mrs     x1, esr_el2
    mrs     x2, far_el2
    bl      handle_serror_exception
    restore_context
    eret

.global sync_exception_el1
.global irq_exception_el1
.global fiq_exception_el1
.global serror_exception_el1

sync_exception_el1:
    save_context
    mov     x0, sp                  /* exception_frame pointer */
    mrs     x1, esr_el1             /* ESR */
    mrs     x2, far_el1             /* FAR */
    bl      handle_sync_exception
    restore_context
    eret

irq_exception_el1:
    save_context
    mov     x0, sp                  /* exception_frame pointer */
    bl      handle_irq_exception
    restore_context
    eret

fiq_exception_el1:
    save_context
    mov     x0, sp                  /* exception_frame pointer */
    bl      handle_fiq_exception
    restore_context
    eret

serror_exception_el1:
    save_context
    mov     x0, sp                  /* exception_frame pointer */
    mrs     x1, esr_el1
    mrs     x2, far_el1
    bl      handle_serror_exception
    restore_context
    eret
