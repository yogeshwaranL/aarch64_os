/*
 * AArch64 Bare Metal OS - Main Kernel Header
 */

#ifndef _KERNEL_H
#define _KERNEL_H

/* Standard types */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint64_t           size_t;
typedef uint64_t           uintptr_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Boolean type */
typedef enum {
    false = 0,
    true = 1
} bool;

/* Exception levels */
#define EL0     0
#define EL1     1
#define EL2     2
#define EL3     3

/* Kernel configuration */
#define KERNEL_VERSION_MAJOR    0
#define KERNEL_VERSION_MINOR    1
#define KERNEL_VERSION_PATCH    0

/* Hardware addresses (QEMU virt machine) */
#define UART0_BASE      0x09000000UL    /* PL011 UART */
#define GIC_DIST_BASE   0x08000000UL    /* GIC Distributor */
#define GIC_CPU_BASE    0x08010000UL    /* GIC CPU Interface */

/* Utility macros */
#define ARRAY_SIZE(x)   (sizeof(x) / sizeof((x)[0]))
#define ALIGN_UP(x, a)  (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

/* Memory barriers */
#define dmb()   __asm__ volatile("dmb sy" ::: "memory")
#define dsb()   __asm__ volatile("dsb sy" ::: "memory")
#define isb()   __asm__ volatile("isb" ::: "memory")

/* Function prototypes */
void kernel_main(void *dtb, uint64_t el);
void handle_exception(uint64_t el, uint64_t type);

/* String functions (minimal) */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);

#endif /* _KERNEL_H */
