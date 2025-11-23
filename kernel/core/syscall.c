/*
 * AArch64 Bare Metal OS - System Call Handler
 *
 * Implements system call dispatch for kernel services
 */

#include "kernel.h"
#include "syscall.h"
#include "uart.h"
#include "sched.h"
#include "task.h"

/*
 * System call handler
 *
 * Called from exception.c when SVC instruction is executed
 * syscall_num = system call number (from x8)
 * arg0-arg3 = arguments (from x0-x3)
 * Returns result in x0
 */
uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg0, uint64_t arg1,
                         uint64_t arg2, uint64_t arg3)
{
    (void)arg1;  /* Unused for now */
    (void)arg2;  /* Unused for now */
    (void)arg3;  /* Unused for now */

    switch (syscall_num) {
    case SYS_YIELD:
        /* Yield CPU to scheduler */
        task_yield();
        return 0;

    case SYS_EXIT:
        /* Exit current task */
        task_exit();
        return 0;

    case SYS_SLEEP:
        /* Sleep for specified milliseconds */
        task_sleep((uint32_t)arg0);
        return 0;

    case SYS_GETPID:
        /* Get current task ID */
        {
            struct task *current = sched_get_current();
            return current ? current->id : 0;
        }

    case SYS_WRITE:
        /* Write string to UART */
        uart_puts((const char *)arg0);
        return 0;

    default:
        uart_puts("Unknown syscall: ");
        uart_puthex(syscall_num);
        uart_puts("\n");
        return (uint64_t)-1;
    }
}
