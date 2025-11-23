# Architecture Overview

## AArch64 Bare Metal OS - System Architecture

**Version:** 0.1.0
**Target:** QEMU virt machine (ARM Cortex-A53)
**Architecture:** ARMv8-A AArch64

---

## Table of Contents

1. [Introduction](#introduction)
2. [System Components](#system-components)
3. [Exception Levels](#exception-levels)
4. [Boot Flow](#boot-flow)
5. [Memory Map](#memory-map)
6. [Design Principles](#design-principles)

---

## Introduction

This document provides a high-level overview of the AArch64 Bare Metal Operating System architecture. The system is designed as a complete, from-scratch implementation demonstrating modern OS concepts on ARM 64-bit architecture.

### Key Features

- **Secure Boot Chain**: ARM Trusted Firmware (ATF) with BL1, BL2, and BL31
- **UEFI Firmware**: EDK2-based platform initialization
- **Type-1 Hypervisor**: Full hypervisor running at EL2
- **Modern Kernel**: Memory management, scheduling, and IPC at EL1
- **ACPI 6.5 Compliance**: Complete ACPI tables with AML interpreter
- **Multi-core Support**: SMP-capable for up to 4 cores

### Target Platform

**QEMU virt Machine Specification:**
- **CPU**: ARM Cortex-A53 (ARMv8-A)
- **Cores**: 4 (configurable)
- **Memory**: 1GB RAM (configurable)
- **GIC**: Generic Interrupt Controller v3
- **UART**: PL011 for serial console
- **Timer**: ARM Generic Timer

---

## System Components

The operating system is composed of several major components:

### 1. Firmware Layer

#### ARM Trusted Firmware (ATF)
- **BL1 (Boot Loader stage 1)**: Boot ROM, runs at EL3
  - First code executed after reset
  - Initializes secure world
  - Loads and authenticates BL2

- **BL2 (Boot Loader stage 2)**: Trusted Boot Firmware
  - Runs at Secure EL1
  - Initializes platform hardware
  - Loads BL31 and BL33 (UEFI)

- **BL31 (Boot Loader stage 3-1)**: EL3 Runtime Software
  - Secure Monitor providing runtime services
  - Implements PSCI (Power State Coordination Interface)
  - Handles SMC (Secure Monitor Call) interface
  - Manages transitions between secure and non-secure worlds

#### UEFI EDK2
- **BL33**: UEFI firmware implementation
  - Platform initialization (PEI phase)
  - Driver execution environment (DXE)
  - UEFI Boot Services and Runtime Services
  - Loads and transfers control to hypervisor/kernel

### 2. Hypervisor (EL2)

A full Type-1 hypervisor providing:

- **Virtual Machine Management**
  - VM lifecycle (create, start, pause, stop, destroy)
  - Resource allocation and isolation
  - Inter-VM communication

- **Virtual CPU (vCPU) Management**
  - vCPU scheduling across physical CPUs
  - Register state virtualization
  - Trap and emulate privileged operations

- **Memory Virtualization**
  - Stage-2 address translation
  - Nested page tables
  - Memory protection between VMs

- **Interrupt Virtualization**
  - Virtual GIC (vGIC) emulation
  - Interrupt routing to VMs
  - Virtual interrupt injection

- **I/O Virtualization**
  - Device assignment
  - Virtual device emulation
  - DMA protection (SMMU support planned)

### 3. Operating System Kernel (EL1)

Core kernel running in non-secure EL1:

- **Memory Management**
  - Physical memory allocator (buddy system)
  - Virtual memory manager (4KB pages)
  - Page table management (4-level translation)
  - Kernel heap allocator (slab allocator)
  - Memory zones (DMA, Normal, HighMem)

- **Process/Thread Management**
  - Task Control Blocks (TCB)
  - Process creation and termination
  - Thread support
  - Process isolation

- **Scheduler**
  - Preemptive multi-tasking
  - Priority-based scheduling
  - Round-robin within priority levels
  - SMP load balancing
  - Real-time scheduling support

- **Interrupt Handling**
  - GICv3 driver
  - IRQ routing and handling
  - Interrupt controller abstraction
  - Deferred interrupt handling (bottom halves)

- **System Calls**
  - System call interface (SVC instruction)
  - Parameter passing and validation
  - Security checks

- **Synchronization**
  - Spinlocks (for SMP)
  - Mutexes
  - Semaphores
  - Condition variables

### 4. ACPI Subsystem

Full ACPI 6.5 implementation:

- **ACPI Tables**
  - RSDP (Root System Description Pointer)
  - XSDT (Extended System Description Table)
  - MADT (Multiple APIC Description Table) - for GIC
  - GTDT (Generic Timer Description Table)
  - FADT (Fixed ACPI Description Table)
  - DSDT (Differentiated System Description Table)
  - SSDT (Secondary System Description Tables)

- **AML Interpreter**
  - Complete AML bytecode interpreter
  - ACPI namespace management
  - Method execution
  - Device object handling

- **Power Management**
  - CPU idle states (C-states)
  - Performance states (P-states)
  - Device power management (D-states)
  - System sleep states (S-states)

---

## Exception Levels

ARMv8-A defines four exception levels (EL0-EL3), with higher levels having greater privilege:

```
┌─────────────────────────────────────────────────┐
│ EL3 (Exception Level 3) - Secure Monitor       │
│ ┌─────────────────────────────────────────────┐ │
│ │ ARM Trusted Firmware (ATF) - BL31           │ │
│ │ - Secure Monitor Call (SMC) handler         │ │
│ │ - PSCI implementation                       │ │
│ │ - Secure/Non-secure world switching         │ │
│ └─────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────┐
│ EL2 (Exception Level 2) - Hypervisor           │
│ ┌─────────────────────────────────────────────┐ │
│ │ Type-1 Hypervisor                           │ │
│ │ - VM management                             │ │
│ │ - Stage-2 translation                       │ │
│ │ - Virtual interrupt controller              │ │
│ │ - Guest VM scheduling                       │ │
│ └─────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────┐
│ EL1 (Exception Level 1) - OS Kernel            │
│ ┌─────────────────────────────────────────────┐ │
│ │ Operating System Kernel                     │ │
│ │ - Memory management (MMU)                   │ │
│ │ - Process/thread management                 │ │
│ │ - Scheduler                                 │ │
│ │ - Device drivers                            │ │
│ │ - System call interface                     │ │
│ └─────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────┐
│ EL0 (Exception Level 0) - User Space           │
│ ┌─────────────────────────────────────────────┐ │
│ │ User Applications                           │ │
│ │ - Unprivileged code                         │ │
│ │ - System calls to kernel                    │ │
│ └─────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

### Exception Level Details

| EL  | Name | Usage | MMU | Features |
|-----|------|-------|-----|----------|
| EL3 | Secure Monitor | ATF BL31 | Optional | SMC, Secure/Non-secure switching |
| EL2 | Hypervisor | Hypervisor | Yes | Stage-2 translation, VM control |
| EL1 | Kernel | OS Kernel | Yes | Stage-1 translation, privileged ops |
| EL0 | User | Applications | Via EL1 | Unprivileged, SVC to kernel |

---

## Boot Flow

The system follows a multi-stage boot process:

```
Power-On Reset
      ↓
┌──────────────────┐
│ BL1 (ATF)        │ ← Boot ROM at EL3
│ - Platform init  │
│ - Load BL2       │
└──────────────────┘
      ↓
┌──────────────────┐
│ BL2 (ATF)        │ ← Trusted Boot Firmware
│ - HW init        │
│ - Load BL31      │
│ - Load BL33      │
└──────────────────┘
      ↓
┌──────────────────┐
│ BL31 (ATF)       │ ← EL3 Runtime (stays resident)
│ - PSCI setup     │
│ - SMC handler    │
└──────────────────┘
      ↓
┌──────────────────┐
│ BL33 (UEFI)      │ ← UEFI EDK2
│ - PEI phase      │
│ - DXE phase      │
│ - Load HV        │
└──────────────────┘
      ↓
┌──────────────────┐
│ Hypervisor (EL2) │ ← Type-1 Hypervisor
│ - EL2 setup      │
│ - Stage-2 MMU    │
│ - Load Kernel    │
└──────────────────┘
      ↓
┌──────────────────┐
│ Kernel (EL1)     │ ← OS Kernel
│ - MMU init       │
│ - Scheduler init │
│ - Start init     │
└──────────────────┘
      ↓
   [Running System]
```

**Detailed boot process documentation**: See [01-boot-process.md](01-boot-process.md)

---

## Memory Map

### Physical Memory Layout (QEMU virt machine)

```
0x0000_0000_0000  ┌─────────────────────┐
                  │ Flash (64MB)        │ ← ATF BL1
0x0000_0400_0000  ├─────────────────────┤
                  │ Device Memory       │
                  │ - GIC               │
                  │ - UART              │
                  │ - Timer             │
0x0000_0800_0000  ├─────────────────────┤
                  │ Reserved            │
0x0000_4000_0000  ├─────────────────────┤
                  │ RAM (1GB)           │
                  │                     │
                  │ ┌─────────────────┐ │
                  │ │ ATF (BL31)      │ │ ← EL3 Runtime
                  │ ├─────────────────┤ │
                  │ │ UEFI            │ │ ← BL33
                  │ ├─────────────────┤ │
                  │ │ Hypervisor      │ │ ← EL2
                  │ ├─────────────────┤ │
                  │ │ Kernel          │ │ ← EL1
                  │ ├─────────────────┤ │
                  │ │ Free RAM        │ │
                  │ └─────────────────┘ │
0x0000_8000_0000  └─────────────────────┘
```

### Virtual Memory Layout (Kernel)

```
0xFFFF_FFFF_FFFF  ┌─────────────────────┐
                  │ Kernel Code/Data    │
0xFFFF_0000_0000  ├─────────────────────┤
                  │ Kernel Heap         │
0xFFFC_0000_0000  ├─────────────────────┤
                  │ Device Mappings     │
0xFFF8_0000_0000  ├─────────────────────┤
                  │ Physical Mem Map    │
0xFFF0_0000_0000  ├─────────────────────┤
                  │                     │
                  │ User Space          │
                  │ (per-process)       │
                  │                     │
0x0000_0000_0000  └─────────────────────┘
```

**Detailed memory management**: See [02-memory-management.md](02-memory-management.md)

---

## Design Principles

### 1. Security

- **Secure Boot**: Verified boot chain from BL1 → BL2 → BL31 → UEFI → Hypervisor
- **Isolation**: Hardware-enforced isolation using exception levels
- **Memory Protection**: Page table permissions and ASLR
- **Principle of Least Privilege**: Minimal privilege for each component

### 2. Performance

- **Zero-copy**: Where possible, minimize data copying
- **Lock-free Algorithms**: For hot paths in multi-core scenarios
- **Cache-friendly**: Data structure alignment and layout
- **Lazy Allocation**: Allocate resources only when needed

### 3. Modularity

- **Clean Interfaces**: Well-defined APIs between components
- **Loose Coupling**: Minimal dependencies between modules
- **Pluggable**: Easy to replace implementations

### 4. Portability

- **HAL (Hardware Abstraction Layer)**: Platform-specific code isolated
- **Architecture-specific Code**: Separated from generic code
- **Standards Compliance**: UEFI, ACPI, ARM specifications

### 5. Maintainability

- **Comprehensive Documentation**: Both inline and external
- **Consistent Coding Style**: Throughout the codebase
- **Testing**: Unit tests and integration tests
- **Version Control**: Git with meaningful commit messages

---

## Component Interaction

```
┌─────────────────────────────────────────────────────────┐
│                    User Applications                     │
│                         (EL0)                           │
└────────────────┬────────────────────────────────────────┘
                 │ System Calls (SVC)
                 ↓
┌─────────────────────────────────────────────────────────┐
│                    Kernel (EL1)                         │
├─────────────────────────────────────────────────────────┤
│  Scheduler  │  Memory Mgr  │  Drivers  │  ACPI  │  IPC  │
└────┬───────────────┬──────────────┬─────────────────────┘
     │               │              │ HVC (Hypervisor Call)
     │               │              ↓
     │               │    ┌──────────────────────────────┐
     │               │    │    Hypervisor (EL2)          │
     │               │    ├──────────────────────────────┤
     │               │    │ VM Mgr │ vCPU │ Stage-2 MMU │
     │               │    └────┬─────────────────────────┘
     │               │         │ SMC (Secure Monitor Call)
     │               │         ↓
     └───────────────┴────┌──────────────────────────────┐
                          │     ATF BL31 (EL3)           │
                          ├──────────────────────────────┤
                          │   PSCI   │   Secure Services │
                          └──────────────────────────────┘
```

---

## Development Phases

The project is developed in 9 phases:

1. **Phase 1**: Project Setup & Foundation ✓
2. **Phase 2**: ARM Trusted Firmware Integration
3. **Phase 3**: UEFI EDK2 Implementation
4. **Phase 4**: Kernel Core (Entry, MMU, Interrupts)
5. **Phase 5**: Memory Management
6. **Phase 6**: Scheduler & Process Management
7. **Phase 7**: Hypervisor Implementation
8. **Phase 8**: ACPI 6.5 Compliance
9. **Phase 9**: Documentation & Testing

**Current Phase**: Phase 1

---

## References

- ARM Architecture Reference Manual ARMv8-A
- ARM Cortex-A53 Technical Reference Manual
- ARM Generic Interrupt Controller Architecture Specification (GICv3)
- ARM Trusted Firmware Design Documentation
- UEFI Specification 2.10
- ACPI Specification 6.5
- QEMU Documentation

---

**Next**: [Boot Process](01-boot-process.md)
