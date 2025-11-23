# AArch64 Bare Metal Operating System

A bare metal operating system kernel for ARM AArch64 architecture (Cortex-A53) running at EL2 (hypervisor mode) on QEMU.

## Overview

This project implements a preemptive multitasking kernel from scratch, featuring:

- **Hypervisor Mode (EL2)**: Kernel runs at EL2 with full hypervisor capabilities
- **Preemptive Scheduler**: Round-robin task scheduling with timer-based preemption
- **Memory Management**: Physical page allocator and MMU with dual address space support
- **Interrupt Handling**: GICv2 interrupt controller with timer interrupts
- **System Calls**: SVC-based syscall interface for kernel services
- **Exception Handling**: Complete exception vector table with handlers

## Target Platform

- **Architecture**: ARMv8-A AArch64
- **Processor**: ARM Cortex-A53
- **Emulator**: QEMU virt machine (qemu-system-aarch64)
- **Current Exception Level**: EL2 (Hypervisor mode)

## Project Structure

```
aarch64_os/
├── kernel/
│   ├── arch/aarch64/          # Architecture-specific code
│   │   ├── entry.S            # Boot entry point
│   │   ├── vectors.S          # Exception vector table
│   │   ├── context.S          # Context switching
│   │   └── linker.ld          # Linker script
│   ├── core/                  # Core kernel functionality
│   │   ├── main.c             # Kernel main
│   │   ├── exception.c        # Exception handlers
│   │   ├── irq.c              # IRQ management
│   │   └── syscall.c          # System call handler
│   ├── mm/                    # Memory management
│   │   ├── mmu.c              # MMU and page tables
│   │   └── pmm.c              # Physical memory allocator
│   ├── sched/                 # Scheduler
│   │   └── sched.c            # Task scheduler
│   ├── drivers/               # Device drivers
│   │   ├── uart.c             # PL011 UART driver
│   │   ├── gic.c              # GICv2 interrupt controller
│   │   ├── timer.c            # ARM generic timer
│   │   └── hypervisor.c       # EL2 hypervisor support
│   └── include/               # Kernel headers
├── docs/
│   └── HYPERVISOR.md          # Hypervisor architecture documentation
├── build/                     # Build output directory
└── scripts/                   # Build and run scripts
```

## Build Requirements

### Toolchain
- `aarch64-linux-gnu-gcc` (or `aarch64-none-elf-gcc`)
- `aarch64-linux-gnu-binutils`
- GNU Make 4.0+

### Tools
- QEMU 6.0+ (`qemu-system-aarch64`)

### Installation (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    qemu-system-arm \
    build-essential
```

## Quick Start

### 1. Clone the Repository
```bash
git clone <repository-url>
cd aarch64_os
```

### 2. Build the Kernel
```bash
make
```

### 3. Run in QEMU
```bash
make run
```

Press `Ctrl-A X` to exit QEMU.

### 4. Debug with GDB
```bash
# Terminal 1: Start QEMU with GDB stub
make run-debug

# Terminal 2: Connect GDB
gdb-multiarch build/kernel/kernel.elf -ex 'target remote :1234'
```

## Documentation

Detailed technical documentation is available in the `docs/` directory:

- **[HYPERVISOR.md](docs/HYPERVISOR.md)**: Comprehensive guide to the EL2 hypervisor implementation, trap mechanisms, and exception handling

## Current Features

### ✅ Implemented

#### Phase 1: Foundation
- Project structure and build system
- QEMU environment configuration
- Linker script and memory layout

#### Phase 2: Boot and Hardware
- Boot entry point (entry.S)
- Exception level detection
- UART driver (PL011) for debug output

#### Phase 3: Exceptions and Interrupts
- Exception vector table (all 16 vectors)
- Synchronous exception handling
- IRQ/FIQ exception handling
- GICv2 interrupt controller driver
- ARM generic timer driver
- Dynamic IRQ handler registration

#### Phase 4: Memory Management
- Physical page allocator (buddy-like system)
- MMU initialization and page table management
- Dual address space support (TTBR0/TTBR1)
- 4-level page tables (4KB granules)
- User and kernel page table APIs

#### Phase 5: Multitasking
- Task control blocks
- Round-robin preemptive scheduler
- Context switching (assembly)
- Timer-based preemption (10ms time slices)
- Task creation and destruction
- Task states: READY, RUNNING, SLEEPING, DEAD

#### Phase 6: System Calls
- SVC exception handling
- System call interface:
  - `SYS_YIELD`: Voluntary task yield
  - `SYS_EXIT`: Task termination
  - `SYS_SLEEP`: Sleep for milliseconds
  - `SYS_GETPID`: Get current task ID
  - `SYS_WRITE`: Debug output

#### Phase 7: Hypervisor Support
- Kernel runs at EL2 (hypervisor mode)
- HCR_EL2 configuration
- HVC (Hypervisor Call) handling
- Stage-1 translation at EL2
- Foundation for future VM support

### 🚧 In Progress

- User mode task execution (EL0)
- Virtual memory isolation between tasks
- More comprehensive system call API

### 📋 Planned

- Device Tree support
- Block device drivers
- File system support
- Network stack
- Shell and userspace utilities
- Full hypervisor with VM management

## Build Targets

```bash
make              # Build kernel
make run          # Run in QEMU
make run-debug    # Run in QEMU with GDB stub
make clean        # Clean build artifacts
make distclean    # Deep clean (removes all generated files)
make info         # Display build configuration
make help         # Show all available targets
```

## Memory Layout

```
Physical Memory (QEMU virt machine):
  0x00000000 - 0x3FFFFFFF    Reserved / Devices
  0x40000000 - 0x40400000    Reserved
  0x40400000 - 0x44400000    Kernel (64MB)
    0x40400000               Kernel code (.text)
    ...                      Read-only data (.rodata)
    ...                      Data (.data)
    ...                      BSS (.bss)
    ...                      Stacks (EL2, EL1)
    ...                      Page tables (256KB)
    ...                      Dynamic allocations

Virtual Memory Layout:
  User Space (TTBR0_EL1):
    0x0000000000000000 - 0x0000007FFFFFFFFF    512GB user space

  Kernel Space (TTBR1_EL1):
    0xFFFFFF8000000000 - 0xFFFFFFFFFFFFFFFF    512GB kernel space
    (Identity mapped to physical 0x40000000+)
```

## Exception Levels

The kernel currently uses the following exception level configuration:

- **EL3**: Not used (QEMU boots directly to EL2)
- **EL2**: Kernel executes here (hypervisor mode)
- **EL1**: Not currently used
- **EL0**: Planned for user applications

This allows the kernel to have full hypervisor capabilities while providing a foundation for future VM support.

## Performance

- **Task Switch Time**: ~100 cycles
- **Timer Interrupt Frequency**: 100 Hz (10ms intervals)
- **Scheduler Time Slice**: 10 ticks (100ms)
- **Maximum Tasks**: 64 concurrent tasks

## Testing

Current boot output demonstrates:
- Successful EL2 detection and initialization
- MMU enablement with page tables
- GIC and timer initialization
- Multiple tasks running with preemptive scheduling
- Timer interrupts firing at 100Hz
- Clean task termination

## Known Limitations

1. No user mode (EL0) task execution yet
2. No virtual memory isolation between tasks
3. No persistent storage or file system
4. Limited device driver support
5. No network stack
6. Minimal error handling in some paths

## License

[To be determined]

## Contributing

This is a learning and research project. Contributions, issues, and feature requests are welcome.

---

**Note**: This is an educational project demonstrating bare metal OS development for AArch64 architecture with hypervisor capabilities.
