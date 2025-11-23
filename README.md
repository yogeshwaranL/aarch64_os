# AArch64 Bare Metal Operating System

A complete bare metal operating system implementation for ARM AArch64 architecture (Cortex-A53) with hypervisor support and full ACPI 6.5 compliance.

## Overview

This project implements a full-featured operating system from the ground up, including:

- **ARM Trusted Firmware (ATF)**: Secure boot chain with BL1, BL2, and BL31
- **UEFI EDK2**: Complete UEFI implementation for platform initialization
- **Bare Metal Kernel**: Custom OS kernel with advanced memory management and scheduling
- **Type-1 Hypervisor**: Full hypervisor implementation running at EL2
- **ACPI 6.5 Support**: Complete ACPI tables and AML interpreter

## Target Platform

- **Architecture**: ARMv8-A AArch64
- **Processor**: ARM Cortex-A53
- **Emulator**: QEMU virt machine (qemu-system-aarch64)
- **Exception Levels**:
  - EL3: ARM Trusted Firmware (Secure Monitor)
  - EL2: Hypervisor
  - EL1: OS Kernel
  - EL0: User applications

## Project Structure

```
aarch64_os/
├── firmware/
│   ├── atf/                    # ARM Trusted Firmware (submodule)
│   └── edk2/                   # UEFI EDK2 implementation (submodule)
├── kernel/
│   ├── arch/aarch64/          # Architecture-specific code
│   ├── core/                   # Core kernel functionality
│   ├── mm/                     # Memory management
│   ├── sched/                  # Scheduler and process management
│   ├── drivers/                # Device drivers
│   └── include/                # Kernel headers
├── hypervisor/
│   ├── core/                   # Hypervisor core (EL2)
│   ├── vm/                     # Virtual machine management
│   ├── vcpu/                   # Virtual CPU management
│   └── include/                # Hypervisor headers
├── acpi/
│   ├── interpreter/            # AML interpreter implementation
│   └── tables/                 # ACPI table generation
├── docs/
│   ├── architecture/           # Architecture documentation
│   ├── api/                    # API documentation (Doxygen)
│   └── guides/                 # Build and user guides
├── build/                      # Build output directory
├── scripts/                    # Build and run scripts
└── tests/                      # Test suites

```

## Build Requirements

### Toolchain
- `aarch64-linux-gnu-gcc` (or `aarch64-none-elf-gcc`)
- `aarch64-linux-gnu-binutils`
- GNU Make 4.0+
- Python 3.8+ (for EDK2 build)
- Git (for submodules)

### Tools
- QEMU 6.0+ (`qemu-system-aarch64`)
- Doxygen (for documentation generation)
- Device Tree Compiler (`dtc`)

### Installation (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    qemu-system-arm \
    build-essential \
    python3 \
    python3-pip \
    device-tree-compiler \
    doxygen \
    graphviz \
    git
```

## Quick Start

### 1. Clone and Initialize
```bash
git clone <repository-url>
cd aarch64_os
git submodule update --init --recursive
```

### 2. Build Firmware
```bash
make firmware          # Build ATF and EDK2
```

### 3. Build Kernel and Hypervisor
```bash
make kernel            # Build kernel
make hypervisor        # Build hypervisor
```

### 4. Build Everything
```bash
make all
```

### 5. Run in QEMU
```bash
make run
```

## Documentation

Comprehensive documentation is available in the `docs/` directory:

- **[Architecture Overview](docs/architecture/00-overview.md)**: System architecture and design
- **[Boot Process](docs/architecture/01-boot-process.md)**: Detailed boot flow from power-on to kernel
- **[Memory Management](docs/architecture/02-memory-management.md)**: Memory subsystem design
- **[Scheduler](docs/architecture/03-scheduler.md)**: Process scheduling and management
- **[Hypervisor](docs/architecture/04-hypervisor.md)**: Type-1 hypervisor implementation
- **[ACPI](docs/architecture/05-acpi.md)**: ACPI 6.5 compliance and AML interpreter
- **[API Reference](docs/api/)**: Doxygen-generated API documentation
- **[Build Guide](docs/guides/building.md)**: Detailed build instructions
- **[Development Guide](docs/guides/development.md)**: Contributing and development workflow

## Features

### Phase 1: Foundation ✓
- Project structure and build system
- QEMU environment configuration
- Documentation framework

### Phase 2: ARM Trusted Firmware (In Progress)
- BL1: Boot ROM and initial platform setup
- BL2: Trusted boot firmware
- BL31: EL3 runtime and PSCI implementation

### Phase 3: UEFI EDK2 (Planned)
- Platform initialization
- UEFI boot services
- Runtime services
- Boot manager

### Phase 4: Kernel Core (Planned)
- Exception vector tables
- UART driver for debugging
- GICv3 interrupt controller
- MMU and page tables

### Phase 5: Memory Management (Planned)
- Physical page allocator
- Virtual memory manager
- Page table management
- Kernel heap (buddy + slab)

### Phase 6: Scheduler (Planned)
- Task control blocks
- Context switching
- Priority-based scheduling
- System call interface

### Phase 7: Hypervisor (Planned)
- EL2 initialization
- Stage-2 address translation
- VM lifecycle management
- Virtual GIC (vGIC)
- Virtual CPU scheduling

### Phase 8: ACPI 6.5 (Planned)
- ACPI tables (RSDP, XSDT, MADT, GTDT, FADT, DSDT)
- Full AML interpreter
- ACPI namespace
- Power management

### Phase 9: Testing & Documentation (Planned)
- Unit tests
- Integration tests
- Complete documentation
- API reference (Doxygen)

## Development Phases

This project is developed in **9 phases**, each building upon the previous:

1. **Foundation**: Project setup and build system
2. **ATF Integration**: ARM Trusted Firmware configuration
3. **UEFI**: EDK2 platform implementation
4. **Kernel Core**: Basic kernel with interrupts and MMU
5. **Memory Management**: Complete memory subsystem
6. **Scheduler**: Process and thread management
7. **Hypervisor**: Full Type-1 hypervisor
8. **ACPI**: ACPI 6.5 compliance and AML interpreter
9. **Documentation**: Complete documentation and testing

Current status: **Phase 1 - Foundation**

## License

[To be determined]

## Contributing

See [DEVELOPMENT.md](docs/guides/development.md) for contribution guidelines.

## Contact

[To be added]

---

**Note**: This is an educational and research project demonstrating bare metal OS development for AArch64 architecture.
