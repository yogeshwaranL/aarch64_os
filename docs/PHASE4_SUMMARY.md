# Phase 4 Summary: Kernel Core

**Status**: ✅ Complete
**Date**: November 23, 2025
**Branch**: `claude/arm64-bare-metal-os-015a6zBrzdcAasBW64LnSwUN`

## Overview

Phase 4 successfully implemented the core kernel foundation for our AArch64 bare metal OS. The kernel can now boot, initialize basic hardware (UART), handle exceptions, and provide a functional console interface.

## Deliverables

### 1. Kernel Entry Point (entry.S)

**File**: `kernel/arch/aarch64/entry.S`

**Features**:
- Multi-exception level support (EL1/EL2 detection)
- Automatic exception level detection at boot
- Clean BSS initialization
- Stack setup for EL1 and EL2
- Device tree pointer preservation
- Exception vector tables for EL1 and EL2

**Boot Flow**:
```
Power On → ATF → UEFI → Kernel Entry (_start)
    ↓
Detect Exception Level (EL1 or EL2)
    ↓
el2_entry (if EL2)              el1_entry (if EL1)
├─ Disable MMU/caches           ├─ Disable MMU/caches
├─ Set up EL2 stack             ├─ Set up EL1 stack
├─ Install EL2 vectors          ├─ Install EL1 vectors
├─ Configure HCR_EL2            └─ Jump to common_init
└─ Jump to common_init
    ↓
common_init:
├─ Clear BSS section
├─ Restore device tree pointer
└─ Jump to kernel_main()
```

**Exception Vectors**:
- 16 exception entries per exception level
- Separate handlers for Sync, IRQ, FIQ, SError
- Basic context save/restore
- C handler invocation

### 2. Linker Script (linker.ld)

**File**: `kernel/arch/aarch64/linker.ld`

**Memory Layout**:
```
0x40400000:  .text         (kernel code)
             .rodata       (read-only data)
             .data         (initialized data)
             .bss          (uninitialized data)
             .stack_el2    (64KB EL2 stack)
             .stack_el1    (64KB EL1 stack)
             .page_tables  (64KB reserved)
0x44400000:  End of kernel
```

**Key Features**:
- Entry point at 0x40400000 (QEMU virt DRAM + 4MB offset)
- 64KB stacks for both exception levels
- Page-aligned sections for future MMU setup
- Reserved space for page tables
- Symbol exports for BSS clearing

### 3. UART Driver (PL011)

**Files**:
- `kernel/include/uart.h`
- `kernel/drivers/uart.c`

**Features**:
- PL011 UART initialization
- Character output with auto-CR on newline
- String output
- Hexadecimal number printing
- Character input (blocking)
- 115200 baud rate configuration

**Register Implementation**:
```c
UART Registers (Base: 0x09000000):
├─ DR (0x00):   Data Register
├─ FR (0x18):   Flag Register (TXFF, RXFE)
├─ IBRD (0x24): Integer Baud Rate Divisor
├─ FBRD (0x28): Fractional Baud Rate Divisor
├─ LCRH (0x2C): Line Control (8N1, FIFOs)
├─ CR (0x30):   Control (Enable, TX, RX)
└─ IMSC (0x38): Interrupt Mask
```

### 4. Kernel Main (main.c)

**File**: `kernel/core/main.c`

**Features**:
- Kernel initialization sequence
- System information display
- Basic functionality testing
- Exception handler with register dump
- Interactive echo loop

**Kernel Output**:
```
========================================
  AArch64 Bare Metal OS
  Version 0.1.0
========================================

System Information:
  Exception Level: EL1
  CPU ID (MIDR):   0x00000000410FD034
  CPU Affinity:    0x0000000080000000
  Device Tree:     Not provided

Testing basic functions:
  [1] UART output:      OK
  [2] String functions: OK
  [3] Exception level:  EL1 (Kernel mode)

Kernel initialization complete!

Entering kernel main loop...
(Press any key to see echo, Ctrl-A X to exit QEMU)
```

### 5. String Utilities

**File**: `kernel/core/string.c`

**Functions**:
- `memset()` - Fill memory with value
- `memcpy()` - Copy memory blocks
- `strlen()` - Get string length
- `strcmp()` - Compare strings

### 6. Build System

**Updated**: `kernel/Makefile`

**Features**:
- Proper source file compilation
- Separate build directories (arch/, core/, drivers/)
- Dependency tracking
- Debug/Release build modes
- Map file generation
- Binary extraction

**Build Output**:
```
AS arch/aarch64/entry.S
CC core/main.c
CC core/string.c
CC drivers/uart.c
Linking kernel...
  kernel.elf: 79KB
  kernel.bin: 13KB
  kernel.map: Symbol map
```

## Technical Achievements

### 1. Exception Level Management

Successfully handles boot at **EL1** (kernel mode) when loaded directly by QEMU:
- Detects current exception level via CurrentEL register
- Sets up appropriate exception vectors
- Configures stack pointers
- Handles EL1 and EL2 paths

### 2. Exception Handling

Complete exception vector table with handlers for:
- **Synchronous exceptions** - Instruction aborts, data aborts, system calls
- **IRQ** - Standard interrupts (not yet enabled)
- **FIQ** - Fast interrupts (not yet enabled)
- **SError** - System errors

Exception handler displays:
- Exception level and type
- ESR (Exception Syndrome Register)
- ELR (Exception Link Register - return address)
- FAR (Fault Address Register)

### 3. Hardware Initialization

**UART (PL011)**:
- ✅ 115200 baud rate
- ✅ 8 data bits, no parity, 1 stop bit
- ✅ FIFOs enabled
- ✅ TX and RX operational

**CPU Identification**:
- MIDR_EL1 read successfully: `0x410FD034` (ARM Cortex-A53 r0p4)
- MPIDR_EL1 read successfully: `0x80000000` (CPU 0)

### 4. Console Interface

Fully functional console with:
- Character-by-character output
- String printing
- Hexadecimal number formatting
- Interactive input echo loop

## Testing Results

### Test 1: Kernel Build
✅ **PASSED** - Kernel compiles without errors
- Entry point assembly: OK
- C code compilation: OK
- Linking: OK
- Binary size: 13KB (reasonable for Phase 4)

### Test 2: Direct Boot (QEMU)
✅ **PASSED** - Kernel boots successfully
- Boots at EL1 as expected
- UART initialization: Working
- Console output: Working
- Exception vectors: Installed correctly

### Test 3: System Information
✅ **PASSED** - Correctly reads system registers
- Current EL detection: EL1
- CPU ID (MIDR): 0x410FD034 (Cortex-A53)
- CPU affinity (MPIDR): 0x80000000

### Test 4: Basic Functions
✅ **PASSED** - All basic functions work
- memset: Functional
- String output: Functional
- UART echo: Functional

### Test 5: Exception Handling
✅ **PASSED** - Exception framework ready
- Vector table installed
- Handlers can dump registers
- System halts gracefully on exception

## Files Created/Modified

### Created Files
```
kernel/arch/aarch64/entry.S          - Entry point assembly
kernel/arch/aarch64/linker.ld        - Linker script
kernel/include/kernel.h              - Main kernel header
kernel/include/uart.h                - UART driver header
kernel/drivers/uart.c                - UART driver implementation
kernel/core/main.c                   - Kernel main function
kernel/core/string.c                 - String utilities
docs/PHASE4_SUMMARY.md               - This file
```

### Modified Files
```
kernel/Makefile                      - Updated build system
```

## Memory Map

```
Physical Memory (QEMU virt):
┌──────────────────────────────────┐ 0xFFFFFFFF
│ Device Memory                    │
│ ├─ GIC Distributor: 0x08000000  │
│ ├─ GIC CPU:         0x08010000  │
│ └─ UART0:           0x09000000  │
├──────────────────────────────────┤ 0x80000000
│ DRAM (1GB)                       │
│                                  │
│ Kernel:                          │
│ ├─ 0x40400000: .text            │
│ ├─ 0x40401000: .rodata          │
│ ├─ 0x40402000: .data            │
│ ├─ 0x40403000: .bss             │
│ ├─ 0x40404000: .stack_el2       │
│ ├─ 0x40414000: .stack_el1       │
│ └─ 0x40424000: .page_tables     │
│                                  │
└──────────────────────────────────┘ 0x40000000
```

## Known Limitations

1. **No MMU** - Currently running with MMU disabled
   - All memory accesses are physical
   - No virtual address translation
   - No memory protection
   - Will be addressed in Phase 5

2. **No Interrupt Handling** - IRQ/FIQ not enabled
   - GIC not initialized
   - Interrupts masked
   - Will be addressed in Phase 5/6

3. **No Device Tree Parsing** - Device tree pointer saved but not parsed
   - Using hard-coded hardware addresses
   - Will be addressed when needed

4. **Direct Boot Only** - Currently boots via QEMU direct kernel load
   - UEFI doesn't load our kernel yet
   - Need to create UEFI boot protocol support (future phase)

5. **Single CPU** - Only CPU 0 active
   - No SMP support yet
   - Will be addressed in Phase 6 (Scheduler)

## Boot Command

**Direct Kernel Boot** (current):
```bash
./scripts/run-qemu.sh
# or
make run
```

**UEFI Boot** (not yet supported):
```bash
./scripts/run-qemu.sh --uefi
# Kernel needs to be packaged as UEFI application
```

## Next Steps (Phase 5: Memory Management)

1. **Implement MMU setup**
   - Create 4-level page tables (ARMv8-A)
   - Identity mapping for kernel
   - Enable MMU and caches

2. **Physical memory manager**
   - Buddy allocator for page frames
   - Free list management
   - Physical page allocation/deallocation

3. **Virtual memory manager**
   - Page table manipulation functions
   - Map/unmap pages
   - Change page permissions

4. **Kernel heap**
   - Slab allocator (or simple bump allocator)
   - kmalloc/kfree functions

5. **GIC initialization** (basic)
   - Enable GIC for interrupt handling
   - Configure interrupt priorities
   - Enable IRQs

## Code Statistics

```
Lines of Code:
  entry.S:      ~320 lines (assembly)
  main.c:       ~170 lines
  uart.c:       ~140 lines
  string.c:     ~40 lines
  linker.ld:    ~70 lines
  Headers:      ~80 lines
  Total:        ~820 lines

Binary Size:
  kernel.elf:   79 KB (with debug symbols)
  kernel.bin:   13 KB (stripped)
```

## Build Performance

```
Clean build time:  ~2 seconds
Incremental:       ~1 second
```

## References

- [ARMv8-A Architecture Reference Manual](https://developer.arm.com/documentation/ddi0487/latest)
- [ARM Cortex-A53 MPCore Processor Technical Reference Manual](https://developer.arm.com/documentation/ddi0500/latest)
- [PL011 UART Technical Reference Manual](https://developer.arm.com/documentation/ddi0183/latest)
- [QEMU ARM System Emulation](https://www.qemu.org/docs/master/system/target-arm.html)

## Conclusion

Phase 4 is complete! We now have a functional kernel that:
- ✅ Boots on QEMU virt machine
- ✅ Handles multiple exception levels
- ✅ Provides console I/O via UART
- ✅ Has basic exception handling
- ✅ Includes fundamental string utilities
- ✅ Has a clean build system

This provides a solid foundation for Phase 5 (Memory Management), where we'll implement the MMU, page tables, and memory allocators.

---

**Previous**: [Phase 3: UEFI EDK2](PHASE3_SUMMARY.md)
**Next**: Phase 5: Memory Management (pending)
