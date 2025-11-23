/*
 * Simple User Program - Hello World
 *
 * This program runs at EL0 and demonstrates:
 * - System calls from user mode
 * - User program execution
 * - Clean exit
 */

.section .text
.globl _user_start

_user_start:
    /* Print hello message using SYS_WRITE (syscall 4) */
    mov     x8, #4                  // SYS_WRITE
    adr     x0, hello_msg           // Message address
    svc     #0                      // Make syscall

    /* Loop a few times with yield */
    mov     x19, #5                 // Counter

loop:
    /* Yield to other tasks (SYS_YIELD = 0) */
    mov     x8, #0                  // SYS_YIELD
    svc     #0                      // Make syscall

    /* Decrement counter */
    subs    x19, x19, #1
    b.ne    loop

    /* Print goodbye message */
    mov     x8, #4                  // SYS_WRITE
    adr     x0, bye_msg             // Message address
    svc     #0                      // Make syscall

    /* Exit (SYS_EXIT = 1) */
    mov     x8, #1                  // SYS_EXIT
    svc     #0                      // Make syscall

    /* Should never reach here */
    b       .

.section .rodata
hello_msg:
    .asciz  "[USER] Hello from user mode!\n"
bye_msg:
    .asciz  "[USER] Goodbye from user mode!\n"
