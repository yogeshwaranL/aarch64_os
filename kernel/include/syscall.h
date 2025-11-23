/*
 * AArch64 Bare Metal OS - System Call Header
 *
 * System call numbers and interface
 */

#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "kernel.h"

/* System call numbers */
#define SYS_YIELD       0                       /* Yield CPU */
#define SYS_EXIT        1                       /* Exit task */
#define SYS_SLEEP       2                       /* Sleep for N ms */
#define SYS_GETPID      3                       /* Get task ID */
#define SYS_WRITE       4                       /* Write to UART */

/* Maximum syscall number */
#define SYS_MAX         4

/* System call handler */
uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg0, uint64_t arg1,
                         uint64_t arg2, uint64_t arg3);

/* User-space syscall wrappers (inline assembly) */
static inline void sys_yield(void)
{
    register uint64_t x8 __asm__("x8") = SYS_YIELD;
    __asm__ volatile("svc #0" :: "r"(x8) : "memory");
}

static inline void sys_exit(void)
{
    register uint64_t x8 __asm__("x8") = SYS_EXIT;
    __asm__ volatile("svc #0" :: "r"(x8) : "memory");
}

static inline void sys_sleep(uint32_t ms)
{
    register uint64_t x8 __asm__("x8") = SYS_SLEEP;
    register uint64_t x0 __asm__("x0") = ms;
    __asm__ volatile("svc #0" :: "r"(x8), "r"(x0) : "memory");
}

static inline uint32_t sys_getpid(void)
{
    register uint64_t x8 __asm__("x8") = SYS_GETPID;
    register uint64_t x0 __asm__("x0");
    __asm__ volatile("svc #0" : "=r"(x0) : "r"(x8) : "memory");
    return (uint32_t)x0;
}

static inline void sys_write(const char *str)
{
    register uint64_t x8 __asm__("x8") = SYS_WRITE;
    register uint64_t x0 __asm__("x0") = (uint64_t)str;
    __asm__ volatile("svc #0" :: "r"(x8), "r"(x0) : "memory");
}

#endif /* _SYSCALL_H */
