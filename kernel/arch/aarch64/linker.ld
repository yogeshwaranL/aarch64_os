/*
 * AArch64 Bare Metal OS - Linker Script
 *
 * Memory layout for QEMU virt machine:
 * - DRAM starts at 0x40000000
 * - Kernel loaded at 0x40400000 (4MB offset for safety)
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

/* Memory regions */
MEMORY
{
    /* QEMU virt machine DRAM: 0x40000000 - 0x7FFFFFFF (1GB) */
    RAM (rwx) : ORIGIN = 0x40400000, LENGTH = 64M
}

SECTIONS
{
    . = 0x40400000;

    /* Kernel code and read-only data */
    .text : {
        *(.text.boot)      /* Entry point first */
        *(.text*)          /* All code */
    } > RAM

    .rodata : ALIGN(4096) {
        *(.rodata*)        /* Read-only data */
    } > RAM

    /* Initialized data */
    .data : ALIGN(4096) {
        *(.data*)
    } > RAM

    /* Uninitialized data (zeroed by entry.S) */
    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Stack for EL2 (64KB) */
    .stack_el2 (NOLOAD) : ALIGN(16) {
        . = . + 0x10000;   /* 64KB stack */
        __stack_top_el2 = .;
    } > RAM

    /* Stack for EL1 (64KB) */
    .stack_el1 (NOLOAD) : ALIGN(16) {
        . = . + 0x10000;   /* 64KB stack */
        __stack_top_el1 = .;
    } > RAM

    /* Kernel page tables (Phase 4: simple identity mapping) */
    .page_tables (NOLOAD) : ALIGN(4096) {
        __page_tables_start = .;
        . = . + 0x10000;   /* 64KB for page tables */
        __page_tables_end = .;
    } > RAM

    /* End of kernel */
    __kernel_end = .;

    /* Discard debug info for now */
    /DISCARD/ : {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
    }
}
