/*
 * Linker script for user programs
 *
 * User programs are loaded at 0x0000000000400000 (4MB in user space)
 * This provides a clean, standard user space layout
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_user_start)

SECTIONS
{
    /* User programs start at 4MB in user virtual address space */
    . = 0x0000000000400000;

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4096) {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(4096) {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
    }
}
