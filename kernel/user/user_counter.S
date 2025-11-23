/*
 * Simple User Program - Counter
 *
 * Counts and prints, demonstrating user mode execution
 */

.section .text
.globl _user_start

_user_start:
    /* Get our PID (SYS_GETPID = 3) */
    mov     x8, #3                  // SYS_GETPID
    svc     #0                      // Make syscall
    mov     x19, x0                 // Save PID

    /* Print start message */
    mov     x8, #4                  // SYS_WRITE
    adr     x0, start_msg           // Message address
    svc     #0                      // Make syscall

    /* Counter loop */
    mov     x20, #3                 // Count to 3

count_loop:
    /* Print counting message */
    mov     x8, #4                  // SYS_WRITE
    adr     x0, count_msg           // Message address
    svc     #0                      // Make syscall

    /* Sleep for a bit (SYS_SLEEP = 2) */
    mov     x8, #2                  // SYS_SLEEP
    mov     x0, #50                 // 50ms
    svc     #0                      // Make syscall

    /* Decrement and loop */
    subs    x20, x20, #1
    b.ne    count_loop

    /* Print done message */
    mov     x8, #4                  // SYS_WRITE
    adr     x0, done_msg            // Message address
    svc     #0                      // Make syscall

    /* Exit */
    mov     x8, #1                  // SYS_EXIT
    svc     #0                      // Make syscall

    b       .

.section .rodata
start_msg:
    .asciz  "[USER] Counter task starting...\n"
count_msg:
    .asciz  "[USER] Counting...\n"
done_msg:
    .asciz  "[USER] Counter task done!\n"
