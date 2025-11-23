/*
 * AArch64 Bare Metal OS - Context Switch
 *
 * Low-level context switching between tasks
 */

.global switch_context

/*
 * switch_context - Switch from old task to new task
 *
 * Arguments:
 *   x0 = pointer to old task's exception_frame
 *   x1 = pointer to new task's exception_frame
 *
 * The exception_frame layout matches our exception handler:
 *   offset 0-240: x0-x30
 *   offset 248: sp
 *   offset 256: elr
 *   offset 264: spsr
 */
switch_context:
    /* Save old task's context */
    /* x0 points to old task's context structure */

    /* Save general purpose registers x0-x30 */
    stp     x0, x1, [x0, #0]
    stp     x2, x3, [x0, #16]
    stp     x4, x5, [x0, #32]
    stp     x6, x7, [x0, #48]
    stp     x8, x9, [x0, #64]
    stp     x10, x11, [x0, #80]
    stp     x12, x13, [x0, #96]
    stp     x14, x15, [x0, #112]
    stp     x16, x17, [x0, #128]
    stp     x18, x19, [x0, #144]
    stp     x20, x21, [x0, #160]
    stp     x22, x23, [x0, #176]
    stp     x24, x25, [x0, #192]
    stp     x26, x27, [x0, #208]
    stp     x28, x29, [x0, #224]
    str     x30, [x0, #240]

    /* Save SP */
    mov     x2, sp
    str     x2, [x0, #248]

    /* Save ELR (return address) */
    /* Use the link register (x30) as our return point */
    str     x30, [x0, #256]

    /* Save SPSR */
    mrs     x2, spsr_el1
    str     x2, [x0, #264]

    /* Now restore new task's context */
    /* x1 points to new task's context structure */

    /* Restore SPSR */
    ldr     x2, [x1, #264]
    msr     spsr_el1, x2

    /* Restore ELR */
    ldr     x2, [x1, #256]
    msr     elr_el1, x2

    /* Restore SP */
    ldr     x2, [x1, #248]
    mov     sp, x2

    /* Restore general purpose registers x0-x30 */
    ldp     x2, x3, [x1, #16]
    ldp     x4, x5, [x1, #32]
    ldp     x6, x7, [x1, #48]
    ldp     x8, x9, [x1, #64]
    ldp     x10, x11, [x1, #80]
    ldp     x12, x13, [x1, #96]
    ldp     x14, x15, [x1, #112]
    ldp     x16, x17, [x1, #128]
    ldp     x18, x19, [x1, #144]
    ldp     x20, x21, [x1, #160]
    ldp     x22, x23, [x1, #176]
    ldp     x24, x25, [x1, #192]
    ldp     x26, x27, [x1, #208]
    ldp     x28, x29, [x1, #224]
    ldr     x30, [x1, #240]

    /* Restore x0 and x1 last */
    ldp     x0, x1, [x1, #0]

    /* Return to new task using exception return */
    /* This uses ELR_EL1 which we've already restored */
    eret
