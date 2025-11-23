# AArch64 Hypervisor Architecture and Trap Mechanisms

## Table of Contents
1. [Overview](#overview)
2. [ARM Exception Levels](#arm-exception-levels)
3. [Hypervisor Initialization](#hypervisor-initialization)
4. [HCR_EL2 Configuration](#hcr_el2-configuration)
5. [Hypervisor Calls (HVC)](#hypervisor-calls-hvc)
6. [Trap Mechanisms](#trap-mechanisms)
7. [Exception Handling Flow](#exception-handling-flow)
8. [Implementation Details](#implementation-details)
9. [Code Examples](#code-examples)
10. [Future Enhancements](#future-enhancements)

---

## Overview

This document describes the Type-1 hypervisor implementation running at EL2 (Hypervisor Exception Level) in our AArch64 bare-metal operating system. This is an "Option B" implementation focusing on hypervisor foundations rather than full VM isolation.

### Key Features
- **Runs at EL2**: Highest non-secure privilege level
- **HVC Support**: Hypervisor Call instruction handling
- **Trap Configuration**: Ability to trap and handle guest exceptions
- **Statistics Tracking**: Monitor HVC calls, traps, and exceptions
- **Foundation for VMs**: Infrastructure for future full virtualization

### What This Is NOT
- ❌ Full VM isolation (no stage-2 page tables)
- ❌ VCPU management
- ❌ Multiple guest operating systems
- ❌ Device pass-through

This is a **foundation** that demonstrates EL2 capabilities and provides infrastructure for future Type-1 hypervisor features.

---

## ARM Exception Levels

ARM AArch64 architecture defines 4 exception levels (ELs) with different privilege levels:

```
┌─────────────────────────────────────────┐
│  EL3: Secure Monitor (Firmware)         │  ← Highest Privilege
│  - ARM Trusted Firmware (ATF)           │    (Secure World)
│  - SMC instruction                       │
├─────────────────────────────────────────┤
│  EL2: Hypervisor                         │  ← **Our Implementation**
│  - Type-1 Hypervisor                     │    (Non-Secure)
│  - HVC instruction                       │
│  - Controls virtualization               │
├─────────────────────────────────────────┤
│  EL1: Kernel/OS                          │
│  - Operating System kernel               │
│  - SVC instruction                       │
│  - MMU, interrupts, processes            │
├─────────────────────────────────────────┤
│  EL0: User Applications                  │  ← Lowest Privilege
│  - User-space programs                   │
│  - No privileged operations              │
└─────────────────────────────────────────┘
```

### Exception Level Transitions

**Upward (Taking Exception)**:
- EL0 → EL1: System call (SVC)
- EL0/EL1 → EL2: Hypervisor call (HVC) or trap
- Any → EL3: Secure monitor call (SMC)

**Downward (Returning)**:
- Via ERET instruction
- Returns to the ELR_ELx address
- Restores SPSR_ELx to PSTATE

### Our Current Configuration

```
QEMU Boot → EL2 (virtualization=on)
     ↓
Hypervisor Init (stay at EL2)
     ↓
Kernel runs at EL2 (not EL1!)
     ↓
Tasks run at EL2 (no user mode yet)
```

**Key Point**: Our kernel currently runs **at EL2**, not EL1. This is intentional for the hypervisor foundation. Future implementations will:
1. Initialize hypervisor at EL2
2. Drop to EL1 for kernel
3. Use EL0 for user applications

---

## Hypervisor Initialization

### Boot Sequence

Located in `kernel/arch/aarch64/entry.S`:

```assembly
_start:
    /* Save device tree pointer */
    mov     x20, x0

    /* Get current exception level */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2              /* CurrentEL is in bits [3:2] */
    cmp     x0, #2
    b.eq    el2_entry               /* ← We take this path */
    cmp     x0, #1
    b.eq    el1_entry

el2_entry:
    /* Disable MMU and caches */
    mrs     x0, sctlr_el2
    bic     x0, x0, #(1 << 0)       /* Clear M bit (MMU) */
    bic     x0, x0, #(1 << 2)       /* Clear C bit (D-cache) */
    bic     x0, x0, #(1 << 12)      /* Clear I bit (I-cache) */
    msr     sctlr_el2, x0
    isb

    /* Set up stack pointer for EL2 */
    ldr     x0, =__stack_top_el2
    mov     sp, x0

    /* Set up exception vector table for EL2 */
    ldr     x0, =exception_vectors_el2
    msr     vbar_el2, x0            /* Vector Base Address Register */
    isb

    /* Configure HCR_EL2 */
    mov     x0, #(1 << 31)          /* RW bit: EL1 is AArch64 */
    msr     hcr_el2, x0
    isb

    /* Enable FP/SIMD at EL2 */
    msr     cptr_el2, xzr           /* Clear all trap bits */
    isb

    /* Jump to common initialization */
    b       common_init
```

### C-Level Initialization

Located in `kernel/core/hypervisor.c`:

```c
void hypervisor_init(void)
{
    uint64_t el = hypervisor_get_el();

    /* Verify we're at EL2 */
    if (el != 2) {
        uart_puts("ERROR: Not running at EL2!\n");
        return;
    }

    /* Configure HCR_EL2 for basic hypervisor operation */
    uint64_t hcr_flags = HCR_EL2_RW;  /* AArch64 execution state */
    hypervisor_configure_hcr(hcr_flags);

    /* Initialize statistics */
    memset(&stats, 0, sizeof(stats));
}
```

---

## HCR_EL2 Configuration

**HCR_EL2** (Hypervisor Configuration Register) is the master control register for virtualization features.

### Register Layout (64-bit)

```
Bit  Name   Description
───────────────────────────────────────────────────────────
[31] RW     Execution state control: 1=AArch64, 0=AArch32
[27] TGE    Trap General Exceptions to EL2
[13] TWI    Trap WFI (Wait For Interrupt)
[14] TWE    Trap WFE (Wait For Event)
[5]  AMO    SError routing to EL2
[4]  IMO    IRQ routing to EL2
[3]  FMO    FIQ routing to EL2
[2]  PTW    Protected Table Walk
[1]  SWIO   Set/Way Invalidation Override
[0]  VM     Virtualization MMU enable (Stage-2)
```

### Our Current Configuration

```c
#define HCR_EL2_RW      (1UL << 31)  /* AArch64 execution state */

/* In hypervisor_init() */
uint64_t hcr_flags = HCR_EL2_RW;
hypervisor_configure_hcr(hcr_flags);
```

**Result**: We set **only** the RW bit:
- ✅ Lower exception levels run in AArch64 mode
- ❌ Stage-2 MMU disabled (VM bit = 0)
- ❌ Exception routing disabled (IMO/FMO/AMO = 0)
- ❌ Trap features disabled (TWI/TWE/TGE = 0)

### Full Hypervisor Configuration Example

For a complete Type-1 hypervisor, you would configure:

```c
uint64_t hcr_flags = HCR_EL2_RW      |  /* AArch64 */
                     HCR_EL2_VM      |  /* Enable Stage-2 MMU */
                     HCR_EL2_IMO     |  /* Route IRQs to EL2 */
                     HCR_EL2_FMO     |  /* Route FIQs to EL2 */
                     HCR_EL2_AMO     |  /* Route SErrors to EL2 */
                     HCR_EL2_TWI     |  /* Trap WFI */
                     HCR_EL2_TWE;       /* Trap WFE */
```

---

## Hypervisor Calls (HVC)

### HVC Instruction

**HVC** (Hypervisor Call) is a special instruction that causes a **synchronous exception** to EL2.

```assembly
HVC #<imm>    /* imm is ignored, used for debugging/tracing */
```

**Effect**:
1. CPU saves state to ELR_EL2 and SPSR_EL2
2. CPU jumps to exception vector at VBAR_EL2 + offset
3. Exception handler runs at EL2
4. Handler returns via ERET

### HVC Calling Convention

**Registers**:
- **x0**: Function number
- **x1-x3**: Arguments
- **Return**: x0 contains result

**Example**:
```c
/* Get hypervisor version */
uint64_t version;
__asm__ volatile(
    "mov x0, %1\n"      /* x0 = HVC_GET_VERSION */
    "hvc #0\n"          /* Hypervisor call */
    "mov %0, x0\n"      /* version = x0 (result) */
    : "=r"(version)
    : "i"(HVC_GET_VERSION)
    : "x0"
);
```

### Implemented HVC Functions

Located in `kernel/include/hypervisor.h`:

```c
#define HVC_GET_VERSION     0x0000  /* Get hypervisor version */
#define HVC_CONSOLE_PUTC    0x0001  /* Console output character */
#define HVC_CONSOLE_GETC    0x0002  /* Console input character */
#define HVC_VM_INFO         0x0010  /* Get VM information */
#define HVC_TRAP_INFO       0x0011  /* Get trap/exception info */
```

### HVC Handler Implementation

Located in `kernel/core/hypervisor.c`:

```c
uint64_t hypervisor_handle_hvc(uint64_t function, uint64_t arg0,
                               uint64_t arg1, uint64_t arg2)
{
    stats.hvc_count++;

    switch (function) {
    case HVC_GET_VERSION:
        /* Return hypervisor version (16.16 format) */
        return (HYPERVISOR_VERSION_MAJOR << 16) |
                HYPERVISOR_VERSION_MINOR;

    case HVC_CONSOLE_PUTC:
        /* Output character to console */
        uart_putc((char)arg0);
        return 0;

    case HVC_VM_INFO:
        /* Return current EL */
        return hypervisor_get_el();

    case HVC_TRAP_INFO:
        /* Return trap count */
        return stats.trap_count;

    default:
        uart_puts("[HYP] Unknown HVC function\n");
        return (uint64_t)-1;
    }
}
```

---

## Trap Mechanisms

### What is a Trap?

A **trap** is when the CPU automatically redirects execution to the hypervisor when certain events occur. This allows the hypervisor to:
1. Monitor guest behavior
2. Emulate privileged operations
3. Enforce security policies
4. Implement virtualization

### Types of Traps

#### 1. Synchronous Exceptions
Caused by executing specific instructions:
- **HVC**: Hypervisor Call
- **SMC**: Secure Monitor Call
- **SVC**: Supervisor Call (if TGE=1)
- **WFI/WFE**: Wait instructions (if TWI/TWE=1)
- **MSR/MRS**: System register access (if trapped)

#### 2. Asynchronous Exceptions
External events:
- **IRQ**: Interrupt Request (if IMO=1)
- **FIQ**: Fast Interrupt Request (if FMO=1)
- **SError**: System Error (if AMO=1)

#### 3. Memory Access Traps
Via Stage-2 page tables (when VM=1):
- **Translation fault**: IPA has no mapping
- **Permission fault**: Access not allowed
- **Alignment fault**: Unaligned access

### ESR_EL2: Exception Syndrome Register

When a trap occurs, **ESR_EL2** contains information about WHY the exception happened.

```
┌────────────────────────────────────────────────────────┐
│ Bits  │ Field │ Description                            │
├────────────────────────────────────────────────────────┤
│ 63:37 │ RES0  │ Reserved                               │
│ 36:32 │ ISS2  │ Additional syndrome info               │
│ 31:26 │ EC    │ Exception Class (identifies cause)     │
│ 25    │ IL    │ Instruction Length (1=32bit, 0=16bit)  │
│ 24:0  │ ISS   │ Instruction Specific Syndrome          │
└────────────────────────────────────────────────────────┘
```

### Exception Classes (EC field)

```c
#define ESR_EC_UNKNOWN      0x00    /* Unknown reason */
#define ESR_EC_WFI_WFE      0x01    /* WFI or WFE instruction */
#define ESR_EC_HVC          0x16    /* HVC instruction (AArch64) */
#define ESR_EC_SMC          0x17    /* SMC instruction (AArch64) */
#define ESR_EC_SVC          0x15    /* SVC instruction (AArch64) */
#define ESR_EC_IABT_LOW     0x20    /* Instruction abort from lower EL */
#define ESR_EC_DABT_LOW     0x24    /* Data abort from lower EL */
```

### Trap Handler Implementation

Located in `kernel/core/hypervisor.c`:

```c
void hypervisor_handle_trap(uint64_t esr, uint64_t far, uint64_t elr)
{
    uint64_t ec = (esr >> ESR_EL2_EC_SHIFT) & ESR_EL2_EC_MASK;
    uint64_t iss = esr & ESR_EL2_ISS_MASK;

    stats.trap_count++;

    uart_puts("[HYP] Trap to EL2:\n");
    uart_puts("  ESR_EL2: "); uart_puthex(esr); uart_puts("\n");
    uart_puts("  EC:      "); uart_puthex(ec); uart_puts(" (");

    switch (ec) {
    case ESR_EC_WFI_WFE:
        uart_puts("WFI/WFE");
        if (iss & 1)
            stats.wfe_count++;
        else
            stats.wfi_count++;
        break;

    case ESR_EC_HVC:
        uart_puts("HVC");
        break;

    case ESR_EC_IABT_LOW:
        uart_puts("Instruction Abort");
        stats.inst_abort_count++;
        break;

    case ESR_EC_DABT_LOW:
        uart_puts("Data Abort");
        stats.data_abort_count++;
        break;

    default:
        uart_puts("Other");
        break;
    }

    uart_puts(")\n");
    uart_puts("  FAR_EL2: "); uart_puthex(far); uart_puts("\n");
    uart_puts("  ELR_EL2: "); uart_puthex(elr); uart_puts("\n");
}
```

---

## Exception Handling Flow

### Complete HVC Exception Flow

```
User Code                      Exception Vector         Hypervisor Handler
─────────                      ────────────────         ──────────────────

1. Execute HVC
   mov x0, #0
   hvc #0           ──────────>

2. CPU saves state
   ELR_EL2 ← PC + 4
   SPSR_EL2 ← PSTATE

3. CPU jumps to vector
                                VBAR_EL2 + 0x400
                                (sync, current EL, SPx)

4. Vector handler
                                sync_exception_el2:
                                  save_context     ───>

5. Save all registers                              /* Stack frame */
                                                   sp -= 272 bytes
                                                   save x0-x30
                                                   save sp, elr, spsr

6. Call C handler                                  handle_sync_exception()
                                  mrs x1, esr_el2    ├─> Read ESR
                                  mrs x2, far_el2    ├─> Read FAR
                                  bl handle_sync     ├─> Call handler
                                                     │
7. Check exception class                           │
                                                   ec = (esr >> 26) & 0x3F
                                                   │
8. Is it HVC?                                      if (ec == 0x16)
                                                   │  Yes!
                                                   │
9. Call HVC handler                                hypervisor_handle_hvc()
                                                   │  function = frame->x0
                                                   │  arg0 = frame->x1
                                                   │  ...
                                                   │
10. Process HVC                                    switch(function)
                                                   case HVC_GET_VERSION:
                                                   │  return 0x00010000
                                                   │
11. Return result                                  frame->x0 = result
                                                   return
                                                   │
12. Restore context             restore_context  <─┘
                                  load x0-x30
                                  load elr, spsr
                                  sp += 272

13. Return to EL2               eret             <───

14. Resume execution
    <result in x0>
```

### Exception Vector Table

Located in `kernel/arch/aarch64/entry.S`:

```assembly
.balign 0x800
exception_vectors_el2:
    /* Current EL with SP0 */
    .balign 0x80
    b       sync_exception_el2      /* Synchronous */
    .balign 0x80
    b       irq_exception_el2       /* IRQ */
    .balign 0x80
    b       fiq_exception_el2       /* FIQ */
    .balign 0x80
    b       serror_exception_el2    /* SError */

    /* Current EL with SPx */
    .balign 0x80
    b       sync_exception_el2      /* ← HVC comes here */
    .balign 0x80
    b       irq_exception_el2
    .balign 0x80
    b       fiq_exception_el2
    .balign 0x80
    b       serror_exception_el2

    /* Lower EL using AArch64 */
    .balign 0x80
    b       sync_exception_el2      /* Would be used if at EL1 */
    .balign 0x80
    b       irq_exception_el2
    .balign 0x80
    b       fiq_exception_el2
    .balign 0x80
    b       serror_exception_el2

    /* Lower EL using AArch32 */
    .balign 0x80
    b       sync_exception_el2
    .balign 0x80
    b       irq_exception_el2
    .balign 0x80
    b       fiq_exception_el2
    .balign 0x80
    b       serror_exception_el2
```

### Vector Offset Calculation

```
VBAR_EL2 + Offset

Offset = (Exception Type × 0x80) + (Exception Level × 0x200)

Exception Types:
  0x000: Synchronous (HVC, SVC, data aborts, etc.)
  0x080: IRQ
  0x100: FIQ
  0x180: SError

Exception Levels:
  0x000: Current EL with SP0
  0x200: Current EL with SPx
  0x400: Lower EL using AArch64
  0x600: Lower EL using AArch32

Example: HVC from current EL with SPx
  Offset = 0x000 (sync) + 0x200 (current EL, SPx)
  Offset = 0x200
```

### Context Save/Restore

```assembly
.macro save_context
    /* Allocate space for exception_frame (272 bytes) */
    sub     sp, sp, #272

    /* Save x0-x30 (31 registers × 8 bytes = 248 bytes) */
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    /* ... */
    str     x30, [sp, #240]

    /* Save SP (before exception) */
    add     x0, sp, #272            /* Original SP */
    str     x0, [sp, #248]

    /* Save ELR and SPSR */
    mrs     x0, elr_el2
    mrs     x1, spsr_el2
    stp     x0, x1, [sp, #256]
.endm

.macro restore_context
    /* Restore ELR and SPSR */
    ldp     x0, x1, [sp, #256]
    msr     elr_el2, x0
    msr     spsr_el2, x1

    /* Restore x0-x30 */
    ldp     x0, x1, [sp, #0]
    /* ... */
    ldr     x30, [sp, #240]

    /* Deallocate exception_frame */
    add     sp, sp, #272
.endm
```

---

## Implementation Details

### File Structure

```
kernel/
├── include/
│   └── hypervisor.h          # EL2 register definitions, HVC codes
├── core/
│   ├── hypervisor.c          # Hypervisor implementation
│   ├── exception.c           # Exception handlers (HVC dispatch)
│   └── main.c                # Hypervisor init and tests
└── arch/aarch64/
    └── entry.S               # EL2 entry, exception vectors
```

### Key Data Structures

```c
/* Hypervisor statistics */
struct hyp_stats {
    uint64_t hvc_count;         /* Total HVC calls */
    uint64_t trap_count;        /* Total traps */
    uint64_t wfi_count;         /* WFI traps */
    uint64_t wfe_count;         /* WFE traps */
    uint64_t data_abort_count;  /* Data aborts */
    uint64_t inst_abort_count;  /* Instruction aborts */
};

/* Exception frame (saved on stack) */
struct exception_frame {
    uint64_t x0;        /* Offset 0 */
    uint64_t x1;        /* Offset 8 */
    /* ... */
    uint64_t x30;       /* Offset 240 */
    uint64_t sp;        /* Offset 248 */
    uint64_t elr;       /* Offset 256 */
    uint64_t spsr;      /* Offset 264 */
};  /* Total: 272 bytes */
```

### System Registers Used

| Register    | Purpose                                    | Access Level |
|-------------|--------------------------------------------|--------------|
| CurrentEL   | Read current exception level               | Read-only    |
| HCR_EL2     | Hypervisor Configuration Register          | EL2+         |
| VBAR_EL2    | Vector Base Address Register               | EL2+         |
| ESR_EL2     | Exception Syndrome Register                | EL2+ (trap)  |
| FAR_EL2     | Fault Address Register                     | EL2+ (trap)  |
| ELR_EL2     | Exception Link Register (return address)   | EL2+ (trap)  |
| SPSR_EL2    | Saved Program Status Register              | EL2+ (trap)  |
| SCTLR_EL2   | System Control Register                    | EL2+         |
| CPTR_EL2    | Architectural Feature Trap Register        | EL2+         |
| VTCR_EL2    | Virtualization Translation Control         | EL2+         |
| VTTBR_EL2   | Virtualization Translation Table Base      | EL2+         |

### Performance Considerations

**HVC Call Overhead**:
1. Save 272 bytes (34 registers) to stack
2. Read ESR_EL2, FAR_EL2
3. Check exception class
4. Call C handler
5. Restore 272 bytes from stack
6. ERET

**Estimated**: ~100-200 CPU cycles per HVC call (without cache misses)

**Compared to**:
- Function call: ~5-10 cycles
- SVC (syscall): ~80-150 cycles
- SMC (secure call): ~150-300 cycles

---

## Code Examples

### Example 1: Making an HVC Call

```c
/* Test HVC calls */
void test_hypervisor(void)
{
    /* Get hypervisor version */
    uint64_t version;
    __asm__ volatile(
        "mov x0, %1\n"
        "hvc #0\n"
        "mov %0, x0\n"
        : "=r"(version)
        : "i"(HVC_GET_VERSION)
        : "x0"
    );

    uart_puts("Hypervisor version: ");
    uart_puthex(version);
    uart_puts("\n");

    /* Write character via HVC */
    __asm__ volatile(
        "mov x0, %0\n"
        "mov x1, %1\n"
        "hvc #0\n"
        :
        : "i"(HVC_CONSOLE_PUTC), "i"('H')
        : "x0", "x1"
    );
}
```

### Example 2: Adding a New HVC Function

**1. Define function code** (`kernel/include/hypervisor.h`):
```c
#define HVC_ALLOC_PAGE      0x0020  /* Allocate physical page */
```

**2. Implement handler** (`kernel/core/hypervisor.c`):
```c
uint64_t hypervisor_handle_hvc(uint64_t function, uint64_t arg0,
                               uint64_t arg1, uint64_t arg2)
{
    stats.hvc_count++;

    switch (function) {
    /* ... existing cases ... */

    case HVC_ALLOC_PAGE:
        /* Allocate a physical page */
        {
            void *page = pmm_alloc_page();
            return (uint64_t)page;
        }

    default:
        return (uint64_t)-1;
    }
}
```

**3. Use it**:
```c
uint64_t page_addr;
__asm__ volatile(
    "mov x0, %1\n"
    "hvc #0\n"
    "mov %0, x0\n"
    : "=r"(page_addr)
    : "i"(HVC_ALLOC_PAGE)
    : "x0"
);
```

### Example 3: Enabling WFI Traps

**1. Configure HCR_EL2**:
```c
void enable_wfi_traps(void)
{
    uint64_t hcr = HCR_EL2_RW | HCR_EL2_TWI;
    hypervisor_configure_hcr(hcr);
}
```

**2. Handle WFI trap**:
```c
void hypervisor_handle_trap(uint64_t esr, uint64_t far, uint64_t elr)
{
    uint64_t ec = (esr >> 26) & 0x3F;

    if (ec == ESR_EC_WFI_WFE) {
        stats.wfi_count++;

        /* Emulate WFI: just return immediately */
        /* ELR already points to next instruction */
        uart_puts("[HYP] WFI trapped and emulated\n");
    }
}
```

### Example 4: Stage-2 Page Table Setup (Future)

```c
/* NOT YET IMPLEMENTED - Example only */

void setup_stage2_translation(void)
{
    /* Allocate stage-2 page table */
    uint64_t *pgtable = pmm_alloc_page();
    memset(pgtable, 0, 4096);

    /* Configure VTCR_EL2 */
    uint64_t vtcr = VTCR_EL2_T0SZ(25) |     /* 39-bit IPA */
                    VTCR_EL2_SL0(1) |       /* Start at level 1 */
                    VTCR_EL2_IRGN0(1) |     /* Inner write-back */
                    VTCR_EL2_ORGN0(1) |     /* Outer write-back */
                    VTCR_EL2_SH0(3) |       /* Inner shareable */
                    VTCR_EL2_TG0(0) |       /* 4KB granule */
                    VTCR_EL2_PS(2);         /* 40-bit PA */

    __asm__ volatile("msr vtcr_el2, %0" :: "r"(vtcr));

    /* Set VTTBR_EL2 */
    uint64_t vttbr = VTTBR_EL2_VMID(1) |
                     VTTBR_EL2_BADDR((uint64_t)pgtable);

    __asm__ volatile("msr vttbr_el2, %0" :: "r"(vttbr));

    /* Enable VM bit in HCR_EL2 */
    uint64_t hcr = HCR_EL2_RW | HCR_EL2_VM;
    hypervisor_configure_hcr(hcr);
}
```

---

## Future Enhancements

### Phase 9A: Full Type-1 Hypervisor

**Features to add**:
1. ✅ Stage-2 page tables (IPA → PA translation)
2. ✅ VCPU management (virtual CPU contexts)
3. ✅ VM lifecycle (create, run, pause, destroy)
4. ✅ Trap handling for all exceptions
5. ✅ IRQ/FIQ routing to VMs
6. ✅ Device emulation (UART, timer, etc.)

**Estimated**: ~2000 additional lines of code

### Phase 9B: Advanced Hypervisor Features

**Features**:
1. Multiple VMs running concurrently
2. VM scheduling (time-sharing)
3. Inter-VM communication
4. Device pass-through
5. VM migration
6. Nested virtualization

### Phase 10: Drop to EL1

**Current**: Kernel runs at EL2
**Goal**: Hypervisor at EL2, kernel at EL1

**Changes required**:
1. Configure EL1 state before dropping
2. Set up SCTLR_EL1, VBAR_EL1, etc.
3. Use ERET to drop to EL1
4. Keep hypervisor services available via HVC

**Benefit**: Proper separation of hypervisor and OS

---

## Testing and Verification

### Current Test Output

```
========================================
Hypervisor Initialization
========================================
Current Exception Level: EL2
Hypervisor Version: 1.0

Configuring HCR_EL2...
  Current HCR_EL2: 0x0000000080000000
  New HCR_EL2:     0x0000000080000000

Hypervisor Features:
  [*] Running at EL2 (Hypervisor mode)
  [*] AArch64 execution state
  [*] HVC (Hypervisor Call) support
  [*] Trap handling capability
  [ ] Stage-2 page tables (not enabled)
  [ ] Full VM isolation (not enabled)

========================================
Testing Hypervisor Calls (HVC)
========================================

HVC_GET_VERSION:   0x0000000000010000 (v1.0)
HVC_CONSOLE_PUTC:  HVC!
HVC_VM_INFO:       EL2

========================================
Hypervisor State
========================================
Exception Level:  EL2
HCR_EL2:          0x0000000080000000
VTCR_EL2:         0x0000000000000000
VTTBR_EL2:        0x0000000000000000

Statistics:
  HVC calls:      0x0000000000000006
  Total traps:    0x0000000000000000
  WFI traps:      0x0000000000000000
  WFE traps:      0x0000000000000000
  Data aborts:    0x0000000000000000
  Inst aborts:    0x0000000000000000
========================================
```

### Verification Steps

1. ✅ System boots at EL2
2. ✅ HCR_EL2 is configured correctly
3. ✅ HVC calls work and return correct values
4. ✅ Statistics are tracked
5. ✅ All previous features still work (memory, scheduler, etc.)

---

## References

### ARM Architecture Documentation

1. **ARM Architecture Reference Manual ARMv8**
   - Chapter D1: The AArch64 System Level Programmers' Model
   - Chapter D7: The AArch64 Virtual Memory System Architecture
   - Chapter D10: The Generic Timer in AArch64 state
   - Chapter G6: The AArch64 Exception model

2. **ARM Virtualization Extensions**
   - Hypervisor Configuration Register (HCR_EL2)
   - Exception Syndrome Register (ESR_EL2)
   - Virtualization Translation Control Register (VTCR_EL2)

3. **ARM Cortex-A Series Programmer's Guide**
   - Chapter 10: Exception Handling
   - Chapter 11: Virtualization

### External Resources

- [ARM Developer: Exception Levels](https://developer.arm.com/documentation/102412/0103/Privilege-and-Exception-levels)
- [ARM Developer: Virtualization](https://developer.arm.com/documentation/102142/0100/Virtualization)
- [Linaro: ARM Trusted Firmware](https://www.trustedfirmware.org/projects/tf-a/)

---

## Conclusion

This hypervisor implementation provides a **foundation** for Type-1 virtualization on AArch64:

✅ **What We Have**:
- EL2 privilege level
- HVC instruction support
- Exception handling infrastructure
- Trap configuration capability
- Statistics and monitoring

❌ **What's Missing** (for full hypervisor):
- Stage-2 page tables
- VCPU management
- VM lifecycle
- Device emulation
- Full trap handling

This is **Option B**: A practical foundation demonstrating EL2 capabilities and providing infrastructure for future full hypervisor implementation.

The next step would be either:
- **Option A**: Complete the hypervisor with stage-2 tables and VCPU support
- **Option B**: Add user mode (EL0) support first
- **Option C**: Add practical features (shell, file system, drivers)

---

**Document Version**: 1.0
**Date**: November 2024
**Author**: AArch64 Bare Metal OS Project
**License**: MIT
