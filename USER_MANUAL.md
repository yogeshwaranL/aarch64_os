# AArch64 Bare Metal OS - User Manual

**Version:** 0.1.0
**Architecture:** ARMv8-A (AArch64)
**Target Platform:** QEMU virt machine (Cortex-A53)

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Components](#components)
4. [Boot Process](#boot-process)
5. [Memory Management](#memory-management)
6. [Task Scheduling](#task-scheduling)
7. [Interrupt Handling](#interrupt-handling)
8. [User Mode Support](#user-mode-support)
9. [System Calls](#system-calls)
10. [Hypervisor Features](#hypervisor-features)
11. [Building and Running](#building-and-running)
12. [Development Guide](#development-guide)

---

## Overview

AArch64 Bare Metal OS is a minimal operating system kernel written from scratch for 64-bit ARM processors. It demonstrates fundamental OS concepts including:

- **Exception Level Management**: Runs at EL2 (Hypervisor mode)
- **Memory Management**: Virtual memory with MMU, physical page allocation
- **Preemptive Multitasking**: Round-robin scheduler with timer-based preemption
- **Interrupt Handling**: GIC (Generic Interrupt Controller) integration
- **User Mode Support**: EL0 task execution with system calls
- **Hypervisor Calls**: HVC interface for hypervisor services

### Key Features

- ✅ Boots on QEMU ARM virt machine
- ✅ Exception handling for all exception levels
- ✅ Virtual memory with two-level page tables
- ✅ Physical memory manager with buddy allocator
- ✅ Kernel heap allocator with slab caches
- ✅ Preemptive multitasking (100ms time slices)
- ✅ User mode (EL0) task support
- ✅ System call interface
- ✅ ARM Generic Timer integration
- ✅ GICv2 interrupt controller support

---

## Architecture

### Exception Levels

The OS utilizes ARM's privilege levels:

```
EL3 (Secure Monitor)     [Not used]
EL2 (Hypervisor)         [Kernel runs here] ← Current level
EL1 (OS Kernel)          [Not used - running at EL2 instead]
EL0 (User Applications)  [User tasks run here]
```

**Why EL2?**
- Provides hypervisor capabilities
- Access to virtualization extensions
- Can trap and emulate lower privilege operations
- Foundation for future VM support

### Memory Layout

```
Physical Memory (64MB DRAM):
┌─────────────────────────────────────────┐
│ 0x40000000 - 0x40400000  Kernel (4MB)   │
│ 0x40400000 - 0x44000000  Free (60MB)    │
└─────────────────────────────────────────┘

Virtual Memory Layout:
┌─────────────────────────────────────────┐
│ User Space (TTBR0_EL1)                  │
│ 0x00000000 - 0x0000007FFFFFFFFF         │
│   0x00400000  User program code         │
│   0x7FFFF000  User stack                │
├─────────────────────────────────────────┤
│ Kernel Space (TTBR1_EL1)                │
│ 0xFFFFFF8000000000 - 0xFFFFFFFFFFFFFFFF │
│   Identity mapped kernel and devices    │
└─────────────────────────────────────────┘
```

### Component Hierarchy

```
Boot (entry.S)
    ↓
Hypervisor Init (hypervisor.c)
    ↓
Physical Memory Manager (pmm.c)
    ↓
MMU & Virtual Memory (mmu.c)
    ↓
Kernel Heap (kmalloc.c)
    ↓
Interrupt Controller (gic.c)
    ↓
IRQ Subsystem (irq.c)
    ↓
Timer (timer.c)
    ↓
Scheduler (sched.c)
    ↓
User Tasks (user programs)
```

---

## Components

### 1. Boot Loader (`kernel/arch/aarch64/entry.S`)

**Purpose:** Initial boot code that sets up the execution environment.

**Process:**
1. **Entry Point (`_start`):**
   - Executed by QEMU at 0x40000000
   - Running at EL2 with MMU disabled
   - Sets up stack pointer (16KB boot stack)

2. **CPU Initialization:**
   - Clears BSS section (zero-initialized data)
   - Checks exception level (should be EL2)
   - Initializes VBAR_EL2 (vector table base)

3. **Jump to C Code:**
   - Calls `kernel_main()` in `main.c`
   - Never returns

**Key Registers:**
- `SP_EL2`: Stack pointer at EL2
- `VBAR_EL2`: Exception vector table base
- `MPIDR_EL1`: CPU affinity/ID

**Code Flow:**
```asm
_start:
    mov sp, #0x40000000      ; Set stack pointer
    bl  clear_bss            ; Zero BSS section
    bl  kernel_main          ; Jump to C code
    b   .                    ; Halt if returns
```

---

### 2. Main Kernel (`kernel/core/main.c`)

**Purpose:** Central initialization and kernel entry point.

**Initialization Sequence:**

```c
kernel_main():
1. Print boot banner
2. Initialize hypervisor (hypervisor_init)
3. Initialize PMM (pmm_init)
4. Initialize MMU (mmu_init)
5. Initialize kernel heap (kmalloc_init)
6. Initialize GIC (gic_init)
7. Initialize IRQ subsystem (irq_init)
8. Initialize timer (timer_init)
9. Initialize scheduler (sched_init)
10. Enable IRQs
11. Run tests and demos
12. Create user mode tasks
13. Start multitasking
14. Idle loop (WFI)
```

**Test Suite:**
- UART output test
- String function tests
- Exception level verification
- Memory allocator tests
- Hypervisor call tests

---

### 3. Hypervisor (`kernel/core/hypervisor.c`)

**Purpose:** Manages EL2 hypervisor features and HVC interface.

#### Configuration

**HCR_EL2 (Hypervisor Configuration Register):**
- `VM = 0`: Not trapping to virtual machine
- `RW = 1`: EL1/EL0 execute in AArch64
- Controls trap behavior for various operations

**Features:**
- HVC (Hypervisor Call) handling
- Trap configuration
- VM state management (for future use)

#### Hypervisor Calls (HVC)

| HVC Number | Function | Description |
|------------|----------|-------------|
| 0 | GET_VERSION | Returns hypervisor version (0x10000) |
| 1 | CONSOLE_PUTC | Print character to console |
| 2 | VM_INFO | Get VM/exception level info |

**Usage Example:**
```asm
mov x0, #'H'              ; Character to print
mov x8, #1                ; HVC_CONSOLE_PUTC
hvc #0                    ; Make hypervisor call
```

**Statistics Tracked:**
- Total HVC calls
- Trap counts (WFI, WFE, data abort, instruction abort)

---

### 4. Physical Memory Manager (`kernel/mm/pmm.c`)

**Purpose:** Manages physical page frames using a buddy allocator.

#### Page Structure

```c
struct page {
    uint32_t flags;      // PAGE_FREE, PAGE_USED, PAGE_RESERVED
    uint32_t order;      // Allocation order (0-3)
    uint32_t refcount;   // Reference count
    struct page *next;   // Free list linkage
};
```

#### Memory Zones

```
Total Memory: 64MB (16,384 pages of 4KB each)
Kernel Reserved: ~4MB (1,024 pages)
Free Memory: ~60MB (12,288 pages)
```

#### Allocation Orders

| Order | Pages | Size | Use Case |
|-------|-------|------|----------|
| 0 | 1 | 4KB | Single page (stacks, page tables) |
| 1 | 2 | 8KB | User program code |
| 2 | 4 | 16KB | Larger allocations |
| 3 | 8 | 32KB | Large buffers |

#### Key Functions

**`pmm_init()`**
- Initializes page descriptor array (16,384 pages)
- Marks kernel region as reserved
- Adds free pages to order-0 free list

**`alloc_pages(int order)`**
- **Order 0:** Fast allocation from free list
- **Order 1-3:** Searches for contiguous free pages
- Returns pointer to first page descriptor
- Updates free page count

**`free_pages(struct page *page, int order)`**
- Frees all pages in allocation (2^order pages)
- Returns pages to order-0 free list
- Updates free page count

**`page_to_phys(struct page *page)`**
- Converts page descriptor to physical address
- Formula: `DRAM_BASE + (page_index * PAGE_SIZE)`

**`phys_to_page(uint64_t phys)`**
- Converts physical address to page descriptor
- Used for freeing pages

#### Example Usage

```c
// Allocate single page for stack
struct page *stack = alloc_pages(0);  // 4KB
uint64_t stack_phys = page_to_phys(stack);

// Allocate 2 pages for user program
struct page *prog = alloc_pages(1);   // 8KB
uint64_t prog_phys = page_to_phys(prog);

// Free when done
free_pages(stack, 0);
free_pages(prog, 1);
```

---

### 5. Memory Management Unit (`kernel/mm/mmu.c`)

**Purpose:** Manages virtual memory with two-level page tables.

#### Page Table Structure

```
AArch64 Virtual Address (48-bit):
┌────────┬────────┬────────┬────────┐
│  L0    │  L1    │  L2    │ Offset │
│ 9 bits │ 9 bits │ 9 bits │ 12 bits│
└────────┴────────┴────────┴────────┘

Page Table Entry (64-bit):
┌──────────────────────────────────────┐
│ Physical Address [47:12] | Attrs    │
│ - Valid bit                          │
│ - Access permissions (AP)            │
│ - Memory type (Normal/Device)        │
│ - Shareable                          │
│ - Access flag                        │
└──────────────────────────────────────┘
```

#### Translation Tables

**TTBR0_EL1 (User Space):**
- Range: 0x0000000000000000 - 0x0000007FFFFFFFFF
- Per-process page tables
- User mode mappings

**TTBR1_EL1 (Kernel Space):**
- Range: 0xFFFFFF8000000000 - 0xFFFFFFFFFFFFFFFF
- Global kernel page table
- Identity mapped kernel and devices

#### Memory Attributes

| Attribute | Value | Description |
|-----------|-------|-------------|
| PAGE_KERNEL_RO | 0x401 | Kernel read-only |
| PAGE_KERNEL_RW | 0x403 | Kernel read-write |
| PAGE_KERNEL_RX | 0x405 | Kernel executable |
| PAGE_USER_RO | 0x421 | User read-only |
| PAGE_USER_RW | 0x423 | User read-write |
| PAGE_USER_RX | 0x425 | User executable |
| PAGE_DEVICE | 0x60F | Device memory (UART, GIC) |

#### Key Functions

**`mmu_init()`**
1. Allocates kernel page tables (PGD, PUD, PMD)
2. Creates identity mappings:
   - DRAM (64MB at 0x40000000)
   - UART (0x09000000)
   - GIC Distributor (0x08000000)
   - GIC CPU Interface (0x08010000)
3. Configures TCR_EL1 (Translation Control Register)
4. Configures MAIR_EL1 (Memory Attribute Indirection Register)
5. Sets TTBR0_EL1 and TTBR1_EL1
6. Enables MMU via SCTLR_EL1

**`mmu_create_user_table()`**
- Allocates new PGD for user process
- Returns page table pointer
- Used when creating user mode tasks

**`mmu_map_user_page(pgd, vaddr, paddr, attrs)`**
- Maps single page in user page table
- Creates intermediate tables as needed
- Sets page attributes (RW, RX, etc.)

**`mmu_map_user_range(pgd, vaddr, paddr, size, attrs)`**
- Maps multiple contiguous pages
- Used for user program code/data

**`mmu_switch_user_table(pgd)`**
- Switches TTBR0_EL1 to new user page table
- NULL switches to kernel-only mode
- Flushes TLB after switch

#### Example: User Task Memory Layout

```c
// User program at 0x400000
mmu_map_user_range(user_pgd, 0x400000, prog_phys,
                   8192, PAGE_USER_RX);

// User stack at 0x7FFFF000
mmu_map_user_page(user_pgd, 0x7FFFF000, stack_phys,
                  PAGE_USER_RW);
```

---

### 6. Kernel Heap Allocator (`kernel/mm/kmalloc.c`)

**Purpose:** Provides dynamic memory allocation using slab allocator.

#### Slab Cache Structure

```c
struct slab_cache {
    uint32_t obj_size;      // Size of objects in this slab
    struct slab *slabs;     // List of slabs
    uint32_t total_objs;    // Total objects
    uint32_t free_objs;     // Free objects
};
```

#### Slab Sizes

| Size | Objects per Page | Use Case |
|------|------------------|----------|
| 16 bytes | 256 | Small structures |
| 32 bytes | 128 | Descriptors |
| 64 bytes | 64 | Medium structures |
| 128 bytes | 32 | Task structures |
| 256 bytes | 16 | Buffers |
| 512 bytes | 8 | Large buffers |
| 1024 bytes | 4 | Page table entries |
| 2048 bytes | 2 | Large allocations |

#### Key Functions

**`kmalloc_init()`**
- Initializes 8 slab caches
- Pre-allocates pages for each cache
- Sets up free lists

**`kmalloc(size_t size)`**
1. Finds appropriate slab cache (smallest size >= requested)
2. Allocates from free list
3. Expands slab if no free objects
4. Returns pointer to allocated memory

**`kfree(void *ptr)`**
1. Determines which slab owns the pointer
2. Returns object to free list
3. Updates statistics

**Statistics:**
- Total allocated bytes
- Total freed bytes
- Currently used bytes
- Per-slab object counts

#### Example Usage

```c
// Allocate structure
struct my_struct *s = kmalloc(sizeof(*s));

// Use it
s->field = 42;

// Free when done
kfree(s);
```

---

### 7. Interrupt Controller (`kernel/drivers/gic.c`)

**Purpose:** Manages ARM Generic Interrupt Controller v2.

#### GIC Architecture

```
┌──────────────────────────────────────┐
│         GIC Distributor              │
│  (GICD - 0x08000000)                 │
│  - Enable/disable IRQs               │
│  - Set priority                      │
│  - Route to CPUs                     │
└──────────────────────────────────────┘
            ↓
┌──────────────────────────────────────┐
│      GIC CPU Interface               │
│  (GICC - 0x08010000)                 │
│  - Acknowledge IRQs                  │
│  - Signal EOI (End of Interrupt)     │
│  - Set priority mask                 │
└──────────────────────────────────────┘
```

#### IRQ Numbers

| IRQ | Device | Description |
|-----|--------|-------------|
| 27 | Timer (Virtual) | Virtual timer interrupt |
| 30 | Timer (Physical) | Physical timer interrupt |
| 33 | UART | UART receive/transmit |

**Total IRQs:** 288 (0-287)

#### Key Functions

**`gic_init()`**
1. Reads GIC ID and IRQ count
2. Disables all IRQs in distributor
3. Sets all IRQs to lowest priority
4. Enables distributor
5. Sets CPU interface priority mask
6. Enables CPU interface

**`gic_enable_irq(irq)`**
- Enables specific IRQ in distributor
- Sets to target CPU 0
- Sets default priority

**`gic_disable_irq(irq)`**
- Disables specific IRQ

**`gic_get_irq()`**
- Reads interrupt acknowledge register
- Returns IRQ number (0-1019)
- 1023 = spurious interrupt

**`gic_end_irq(irq)`**
- Writes to EOI register
- Signals interrupt handling complete

---

### 8. IRQ Subsystem (`kernel/core/irq.c`)

**Purpose:** High-level interrupt handling and dispatch.

#### IRQ Handler Structure

```c
struct irq_handler {
    void (*handler)(void *data);  // Handler function
    void *data;                    // Handler context
    const char *name;              // Handler name
};
```

#### Key Functions

**`irq_init()`**
- Initializes handler table (288 entries)
- Clears all handlers

**`irq_register_handler(irq, handler, data, name)`**
- Registers handler for specific IRQ
- Stores handler function and context
- Enables IRQ in GIC

**`irq_handler()`**
1. Called from exception vector on IRQ
2. Gets IRQ number from GIC
3. Looks up registered handler
4. Calls handler function
5. Signals EOI to GIC

**Example: Timer IRQ Flow**

```
Timer fires (IRQ 30)
    ↓
Exception vector (vectors_el2)
    ↓
irq_handler()
    ↓
timer_irq_handler()
    ↓
schedule()  (task switching)
    ↓
gic_end_irq(30)
    ↓
Return from exception
```

---

### 9. ARM Generic Timer (`kernel/drivers/timer.c`)

**Purpose:** Provides periodic timer interrupts for scheduling.

#### Timer Configuration

```
Frequency: 62.5 MHz (from QEMU)
Tick Rate: 100 Hz (10ms per tick)
Interval: 625,000 ticks
IRQ: 30 (physical timer)
```

#### Key Functions

**`timer_init()`**
1. Reads timer frequency (CNTFRQ_EL0)
2. Calculates interval for 100 Hz
3. Registers IRQ handler
4. Enables timer with interval
5. Enables timer IRQ in GIC

**`timer_irq_handler()`**
1. Increments tick counter
2. Calls `schedule()` for task switching
3. Reloads timer with next interval

**`timer_get_ticks()`**
- Returns total timer ticks since boot
- Used for time measurements

**`timer_get_uptime_ms()`**
- Converts ticks to milliseconds
- Formula: `(ticks * 1000) / frequency`

#### Usage in Scheduler

```c
// Every 10ms (100 Hz):
timer_irq_handler()
    ↓
schedule()
    ↓
Check current task time slice
    ↓
If expired, switch to next task
```

---

### 10. Task Scheduler (`kernel/sched/sched.c`)

**Purpose:** Implements preemptive multitasking with round-robin scheduling.

#### Task Structure

```c
struct task {
    task_id_t id;                    // Unique task ID
    char name[32];                   // Task name
    task_state_t state;              // READY, RUNNING, BLOCKED, TERMINATED
    uint32_t priority;               // Priority (lower = higher priority)
    uint32_t time_slice;             // Remaining time slice
    uint64_t run_count;              // Times scheduled
    uint64_t total_runtime;          // Total CPU time

    // Memory
    void *kernel_stack_base;         // Kernel stack (4KB)
    void *kernel_stack_top;          // Stack pointer
    void *user_stack_base;           // User stack (4KB)
    void *user_stack_top;            // User stack pointer
    void *page_table;                // User page table (EL0 tasks)

    // Context
    struct exception_frame context;   // Saved registers

    // Task level
    task_level_t level;              // KERNEL or USER

    // Queue linkage
    struct task *next, *prev;        // Ready queue
};
```

#### Task States

```
TASK_READY      → Ready to run, in ready queue
TASK_RUNNING    → Currently executing
TASK_BLOCKED    → Waiting for event (future)
TASK_TERMINATED → Exited, slot can be reused
```

#### Scheduling Algorithm

**Round-Robin with Time Slices:**
1. Each task gets 10 timer ticks (100ms)
2. When time slice expires, task moves to end of queue
3. Next ready task selected from queue head
4. Context switch to new task

```
Ready Queue:  [Task-A] → [Task-B] → [Task-C] → [Task-A] ...
                  ↑
            Current task
```

#### Key Functions

**`sched_init()`**
1. Initializes task pool (16 task slots)
2. Creates idle task (lowest priority, always ready)
3. Enables scheduling

**`task_create(name, func, arg, priority)`**
1. Finds free task slot
2. Allocates kernel stack (4KB)
3. Initializes task structure
4. Sets up initial context (registers, PC, SP)
5. Adds to ready queue
6. Returns task ID

**`task_create_user(name, binary, size, priority)`**
1. Finds free task slot
2. Allocates kernel stack (exception handling)
3. Allocates user stack
4. Allocates pages for user program
5. Copies binary to allocated memory
6. Creates user page table
7. Maps program code (0x400000, RX)
8. Maps user stack (0x7FFFF000, RW)
9. Sets up EL0 context (SPSR = 0x3C0)
10. Adds to ready queue

**`schedule()` - Called from timer IRQ**
1. Decrement current task's time slice
2. If time slice expired or task blocked:
   - Save current task to ready queue
   - Get next ready task
   - Switch page table if needed
   - Call `switch_context()` (assembly)

**`task_yield()`**
- Voluntarily give up CPU
- Sets time slice to 0
- Triggers reschedule on next timer tick

**`task_exit()`**
1. Marks task as TERMINATED
2. Frees kernel stack
3. Frees user stack (if exists)
4. TODO: Free user page table
5. Forces immediate reschedule

**`task_sleep(ms)`**
- Busy-wait sleep (simple implementation)
- Yields CPU periodically
- Phase 8 will add proper blocking

#### Context Switching (`kernel/sched/switch.S`)

```asm
switch_context(old_ctx, new_ctx):
1. Save all registers to old_ctx
   - x0-x30 (general purpose)
   - SP, ELR, SPSR
2. Restore all registers from new_ctx
3. Return (now in new task context)
```

#### Example: Task Creation and Execution

```c
// Create kernel task
void my_task(void *arg) {
    while (1) {
        uart_puts("Hello from task!\n");
        task_sleep(1000);  // Sleep 1 second
    }
}

task_id_t id = task_create("my_task", my_task, NULL, 10);

// Task added to ready queue, will run on next schedule
```

---

### 11. Exception Handling (`kernel/core/exception.c`)

**Purpose:** Handles synchronous and asynchronous exceptions.

#### Exception Vector Table

```asm
vectors_el2:
    // Current EL with SP0
    curr_el_sp0_sync        // Synchronous exception
    curr_el_sp0_irq         // IRQ
    curr_el_sp0_fiq         // FIQ
    curr_el_sp0_serror      // SError

    // Current EL with SPx
    curr_el_spx_sync        // Synchronous exception
    curr_el_spx_irq         // IRQ  ← Timer interrupts
    curr_el_spx_fiq         // FIQ
    curr_el_spx_serror      // SError

    // Lower EL (AArch64)
    lower_el_aarch64_sync   // Synchronous ← Syscalls from EL0
    lower_el_aarch64_irq    // IRQ
    lower_el_aarch64_fiq    // FIQ
    lower_el_aarch64_serror // SError

    // Lower EL (AArch32)
    lower_el_aarch32_sync   // Not supported
    lower_el_aarch32_irq
    lower_el_aarch32_fiq
    lower_el_aarch32_serror
```

#### Exception Syndrome Register (ESR_EL2)

```
ESR_EL2 encodes exception cause:
- EC (bits 26-31): Exception Class
  - 0x15: SVC from AArch64
  - 0x16: HVC from AArch64
  - 0x24: Data abort
  - 0x20: Instruction abort
- ISS (bits 0-24): Instruction Specific Syndrome
```

#### Key Functions

**`exception_handler(struct exception_frame *frame)`**
- Generic exception handler
- Prints exception details
- Shows register dump
- Halts system

**`sync_exception_handler(struct exception_frame *frame)`**
- Handles synchronous exceptions
- Checks ESR_EL2 for exception class
- Routes to appropriate handler (syscall, HVC, abort)

---

### 12. System Calls (`kernel/core/syscall.c`)

**Purpose:** Provides interface for user mode to request kernel services.

#### System Call Interface

**Invocation from User Mode:**
```asm
mov x8, #syscall_number    ; Syscall number
mov x0, #arg1              ; Argument 1
mov x1, #arg2              ; Argument 2
svc #0                     ; Supervisor call
; Return value in x0
```

#### System Call Table

| Number | Name | Arguments | Description |
|--------|------|-----------|-------------|
| 0 | SYS_YIELD | None | Yield CPU to next task |
| 1 | SYS_EXIT | None | Exit current task |
| 2 | SYS_SLEEP | x0=milliseconds | Sleep for specified time |
| 3 | SYS_GETPID | None | Get current task ID |
| 4 | SYS_WRITE | x0=string | Write string to console |

#### System Call Handler

```c
syscall_handler(struct exception_frame *frame):
1. Extract syscall number from x8
2. Switch on syscall number:
   case SYS_YIELD:
       task_yield()
   case SYS_EXIT:
       task_exit()
   case SYS_WRITE:
       uart_puts((char*)frame->x0)
   case SYS_SLEEP:
       task_sleep(frame->x0)
   case SYS_GETPID:
       frame->x0 = current_task->id
3. Return to user mode
```

#### Example: User Program Syscall

```asm
; Print message
mov x8, #4                  ; SYS_WRITE
adr x0, message            ; Message pointer
svc #0                     ; System call

; Exit
mov x8, #1                 ; SYS_EXIT
svc #0                     ; Never returns
```

---

### 13. User Mode Support

**Purpose:** Allows unprivileged code execution at EL0.

#### User Task Structure

```
┌─────────────────────────────────────┐
│ Kernel Stack (4KB)                  │ ← For exception handling
│ - Used when syscall/IRQ occurs      │
│ - SP_EL2 points here                │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│ User Program Code (4-32KB)          │ ← Mapped at 0x400000
│ - Read + Execute permissions        │
│ - Entry point at 0x400000           │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│ User Stack (4KB)                    │ ← Mapped at 0x7FFFF000
│ - Read + Write permissions          │
│ - Grows downward                    │
│ - SP_EL0 points here                │
└─────────────────────────────────────┘
```

#### User Task Creation Process

1. **Allocate Memory:**
   - Kernel stack (order 0)
   - User stack (order 0)
   - Program pages (order based on size)

2. **Create Page Table:**
   - New PGD for user process
   - Map program at 0x400000 (RX)
   - Map stack at 0x7FFFF000 (RW)

3. **Initialize Context:**
   ```c
   context.elr = 0x400000;       // Entry point
   context.sp = 0x7FFFF000;      // User stack
   context.spsr = 0x3C0;         // EL0t, IRQs enabled
   ```

4. **Add to Ready Queue:**
   - Task will be scheduled normally
   - Page table switched on context switch

#### Context Switch to User Mode

```c
schedule():
1. Select next task
2. If task->level == TASK_USER:
       mmu_switch_user_table(task->page_table)
3. switch_context(&old->context, &new->context)
4. ERET instruction returns to EL0
```

#### User to Kernel Transition

```
User code at EL0
    ↓
SVC #0 (system call)
    ↓
CPU takes exception
    ↓
Save context to kernel stack
    ↓
Vector: lower_el_aarch64_sync
    ↓
sync_exception_handler()
    ↓
syscall_handler()
    ↓
Process syscall
    ↓
ERET back to EL0
```

---

## Boot Process

### Detailed Boot Sequence

```
1. QEMU loads kernel.elf at 0x40000000
   - Entry point: _start (entry.S)
   - Exception Level: EL2
   - MMU: Disabled
   - Caches: Disabled

2. _start (entry.S):
   - Set stack pointer (SP_EL2 = 0x40000000)
   - Clear BSS section
   - Initialize VBAR_EL2 (exception vectors)
   - Jump to kernel_main()

3. kernel_main() (main.c):
   a. Print boot banner
   b. Display system information
   c. Run basic tests (UART, strings, exception level)

4. hypervisor_init():
   - Verify running at EL2
   - Configure HCR_EL2
   - Set up hypervisor call handlers

5. pmm_init():
   - Initialize 16,384 page descriptors
   - Mark kernel pages (0x40000000-0x40400000) as reserved
   - Add 12,288 free pages to order-0 list

6. mmu_init():
   - Allocate kernel page tables (PGD, PUD, PMD)
   - Identity map:
     * DRAM: 0x40000000-0x44000000 (64MB)
     * UART: 0x09000000
     * GIC Distributor: 0x08000000
     * GIC CPU Interface: 0x08010000
   - Configure TCR_EL1, MAIR_EL1
   - Set TTBR0_EL1 and TTBR1_EL1
   - Enable MMU (SCTLR_EL1.M = 1)
   - Enable caches (SCTLR_EL1.C = 1, I = 1)

7. kmalloc_init():
   - Initialize 8 slab caches (16-2048 bytes)
   - Pre-allocate pages for each cache
   - Set up free lists

8. gic_init():
   - Detect GIC version (0x43B)
   - Count IRQ lines (288)
   - Disable all IRQs
   - Set priorities
   - Enable distributor
   - Enable CPU interface

9. irq_init():
   - Clear handler table (288 entries)

10. timer_init():
    - Read timer frequency (62.5 MHz)
    - Calculate interval for 100 Hz
    - Register timer IRQ handler (IRQ 30)
    - Enable and start timer

11. sched_init():
    - Initialize task pool (16 slots)
    - Create idle task (ID 1, priority 255)
    - Enable scheduling

12. Enable IRQs:
    - Clear DAIF.I bit
    - IRQs now trigger timer_irq_handler()

13. Run Tests:
    - Memory allocator test (kmalloc/kfree)
    - Print memory statistics
    - Hypervisor call tests (HVC)

14. Create Tasks:
    - Task-A, Task-B, Task-C (kernel tasks)
    - user-hello, user-counter (user mode tasks)

15. Start Multitasking:
    - Call schedule() explicitly
    - Timer now triggers automatic context switches

16. Idle Loop:
    - Current task = idle
    - Execute WFI (wait for interrupt)
    - Timer IRQ wakes CPU
    - Schedule next task
    - Repeat
```

### Memory State After Boot

```
Physical Memory (64MB):
┌──────────────────────────────────────┐
│ 0x40000000  Kernel code/data (1MB)   │
│ 0x40100000  Page tables (~512KB)     │
│ 0x40200000  Kernel heap (256KB)      │
│ 0x40300000  Task stacks (6 tasks)    │
│ 0x40400000  User programs (8KB)      │
│ 0x40500000  Free memory (58.5MB)     │
└──────────────────────────────────────┘

Virtual Memory:
┌──────────────────────────────────────┐
│ TTBR0_EL1 (User Space):              │
│   0x00400000  user-hello code (4KB)  │
│   0x00402000  user-counter code(4KB) │
│   0x7FFFF000  User stacks            │
├──────────────────────────────────────┤
│ TTBR1_EL1 (Kernel Space):            │
│   Identity mapped to physical        │
└──────────────────────────────────────┘
```

---

## Building and Running

### Prerequisites

```bash
# Required tools (Ubuntu/Debian):
sudo apt-get install gcc-aarch64-linux-gnu \
                     qemu-system-aarch64 \
                     make \
                     git

# Verify installation:
aarch64-linux-gnu-gcc --version
qemu-system-aarch64 --version
```

### Build Commands

```bash
# Full build
make

# Clean build
make clean
make

# Build info
make info

# Build kernel only (no user programs)
cd kernel && make
```

### Build Output

```
build/
├── kernel/
│   ├── kernel.elf       # ELF executable (113KB)
│   ├── kernel.bin       # Raw binary (41KB)
│   ├── kernel.map       # Linker map file
│   ├── arch/            # Architecture objects
│   ├── core/            # Core kernel objects
│   ├── mm/              # Memory management objects
│   ├── sched/           # Scheduler objects
│   ├── drivers/         # Driver objects
│   └── user/            # User program objects
│       ├── user_hello.bin
│       ├── user_hello.o
│       ├── user_counter.bin
│       └── user_counter.o
```

### Running

```bash
# Run with default settings
make run

# Run with QEMU options
qemu-system-aarch64 \
    -machine virt,virtualization=on \
    -cpu cortex-a53 \
    -smp 4 \
    -m 1G \
    -nographic \
    -serial mon:stdio \
    -kernel build/kernel/kernel.elf

# Run with GDB debugging
make debug
# In another terminal:
gdb-multiarch build/kernel/kernel.elf
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

### QEMU Options Explained

| Option | Description |
|--------|-------------|
| `-machine virt,virtualization=on` | ARM virt machine with EL2 support |
| `-cpu cortex-a53` | Cortex-A53 processor (ARMv8-A) |
| `-smp 4` | 4 CPU cores (currently only CPU0 used) |
| `-m 1G` | 1GB RAM (64MB used, rest ignored) |
| `-nographic` | No graphical output |
| `-serial mon:stdio` | Serial console to terminal |
| `-kernel` | Kernel ELF to load |

### Expected Output

```
========================================
  AArch64 Bare Metal OS
  Version 0.1.0
========================================

System Information:
  Exception Level: EL2
  CPU ID (MIDR):   0x410FD034
  ...

[All initialization phases]

Task List:
==========
  [0x1] idle - RUNNING
  [0x2] Task-A - READY
  [0x3] Task-B - READY
  [0x4] Task-C - READY
  [0x5] user-hello - READY
  [0x6] user-counter - READY

========================================
Starting Preemptive Multitasking!
========================================

[Tasks execute, context switching occurs]
```

---

## Development Guide

### Adding a New Kernel Task

```c
// Define task function
void my_task_func(void *arg) {
    uint32_t *counter = (uint32_t *)arg;

    while (1) {
        uart_puts("Task running: ");
        uart_puthex(*counter);
        uart_puts("\n");

        (*counter)++;
        task_sleep(500);  // Sleep 500ms
    }
}

// Create task in main.c
uint32_t my_counter = 0;
task_create("my_task", my_task_func, &my_counter, 10);
```

### Adding a New System Call

1. **Define syscall number** (`kernel/include/syscall.h`):
```c
#define SYS_MY_SYSCALL  5
```

2. **Implement handler** (`kernel/core/syscall.c`):
```c
case SYS_MY_SYSCALL:
    uart_puts("My syscall called!\n");
    frame->x0 = 42;  // Return value
    break;
```

3. **Use from user program**:
```asm
mov x8, #5        ; SYS_MY_SYSCALL
svc #0            ; Make syscall
; Return value in x0
```

### Adding a New User Program

1. **Create assembly file** (`kernel/user/my_program.S`):
```asm
.section .text
.globl _user_start

_user_start:
    ; Your code here
    mov x8, #4              ; SYS_WRITE
    adr x0, message
    svc #0

    mov x8, #1              ; SYS_EXIT
    svc #0

.section .rodata
message:
    .asciz "Hello from my program!\n"
```

2. **Update Makefile** (`kernel/user/Makefile`):
```makefile
USER_PROGS := user_hello user_counter my_program
```

3. **Add to kernel** (`kernel/include/user_programs.h`):
```c
extern uint8_t _binary_my_program_bin_start[];
extern uint8_t _binary_my_program_bin_end[];
extern uint8_t _binary_my_program_bin_size[];
```

4. **Create task** (`kernel/core/main.c`):
```c
size_t size = (size_t)_binary_my_program_bin_size;
task_create_user("my_program",
                 _binary_my_program_bin_start,
                 size, 10);
```

### Debugging Tips

**1. Add Debug Prints:**
```c
uart_puts("DEBUG: Value = ");
uart_puthex(value);
uart_puts("\n");
```

**2. Check Exception Level:**
```c
uint64_t el;
__asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
uart_puts("Current EL: ");
uart_puthex(el >> 2);
uart_puts("\n");
```

**3. Dump Registers:**
```c
void dump_regs(struct exception_frame *frame) {
    uart_puts("  x0  = "); uart_puthex(frame->x0);  uart_puts("\n");
    uart_puts("  x1  = "); uart_puthex(frame->x1);  uart_puts("\n");
    // ... etc
    uart_puts("  ELR = "); uart_puthex(frame->elr); uart_puts("\n");
    uart_puts("  SP  = "); uart_puthex(frame->sp);  uart_puts("\n");
}
```

**4. Memory Inspection:**
```c
void dump_mem(uint64_t addr, size_t len) {
    uint8_t *p = (uint8_t *)addr;
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) {
            uart_puts("\n");
            uart_puthex(addr + i);
            uart_puts(": ");
        }
        uart_puthex(p[i]);
        uart_puts(" ");
    }
    uart_puts("\n");
}
```

**5. GDB Commands:**
```
# Break at function
break kernel_main
break schedule

# Step through code
step
next

# Examine memory
x/16x 0x40000000

# Print variables
print current_task
print *current_task

# Backtrace
backtrace

# Registers
info registers
```

### Code Style Guidelines

- **Naming:**
  - Functions: `snake_case`
  - Structures: `struct snake_case`
  - Macros/Constants: `UPPER_CASE`
  - Global variables: `snake_case` with static

- **Comments:**
  - Function headers explaining purpose
  - Complex logic explained inline
  - TODO markers for future work

- **Formatting:**
  - 4-space indentation
  - Braces on same line
  - Max 100 characters per line

### Common Issues and Solutions

**Issue: MMU fault on boot**
- **Cause:** Incorrect page table mappings
- **Solution:** Verify identity mappings for DRAM and devices
- **Debug:** Check TTBR0_EL1, TTBR1_EL1, TCR_EL1 values

**Issue: Timer not firing**
- **Cause:** IRQs not enabled or GIC misconfigured
- **Solution:** Verify `gic_enable_irq(30)` called
- **Debug:** Check DAIF register, GIC registers

**Issue: User task crashes**
- **Cause:** Incorrect page table or SPSR
- **Solution:** Verify user mappings (0x400000, 0x7FFFF000)
- **Debug:** Check ESR_EL2 for exception cause

**Issue: Context switch fails**
- **Cause:** Stack corruption or misaligned SP
- **Solution:** Verify 16-byte stack alignment
- **Debug:** Check SP values in exception frame

---

## Appendix

### Register Reference

**System Registers:**

| Register | Purpose | Access |
|----------|---------|--------|
| CurrentEL | Current Exception Level | Read |
| SCTLR_EL1 | System Control (MMU enable) | Read/Write |
| TCR_EL1 | Translation Control | Read/Write |
| TTBR0_EL1 | User page table base | Read/Write |
| TTBR1_EL1 | Kernel page table base | Read/Write |
| MAIR_EL1 | Memory attributes | Read/Write |
| VBAR_EL2 | Vector base address | Read/Write |
| HCR_EL2 | Hypervisor configuration | Read/Write |
| ESR_EL2 | Exception syndrome | Read |
| ELR_EL2 | Exception return address | Read/Write |
| SPSR_EL2 | Saved program status | Read/Write |

**Timer Registers:**

| Register | Purpose |
|----------|---------|
| CNTFRQ_EL0 | Counter frequency |
| CNTP_TVAL_EL0 | Timer value |
| CNTP_CTL_EL0 | Timer control |
| CNTPCT_EL0 | Physical count |

### Memory Map

```
Physical Address Space:
0x00000000-0x3FFFFFFF   Not used
0x40000000-0x43FFFFFF   DRAM (64MB)
0x44000000-0x07FFFFFF   Not used
0x08000000-0x08000FFF   GIC Distributor
0x08010000-0x08010FFF   GIC CPU Interface
0x09000000-0x09000FFF   UART (PL011)
0x09010000-0xFFFFFFFF   Not used
```

### Useful Links

- **ARM Architecture Reference Manual:**
  https://developer.arm.com/documentation/ddi0487/latest

- **ARM GICv2 Specification:**
  https://developer.arm.com/documentation/ihi0048/latest

- **QEMU Documentation:**
  https://www.qemu.org/docs/master/

- **AArch64 Instruction Set:**
  https://developer.arm.com/architectures/instruction-sets/base-isas/a64

---

## Conclusion

This manual covers all major components of the AArch64 Bare Metal OS. The system demonstrates fundamental OS concepts in a clean, minimal implementation suitable for learning and experimentation.

**Key Achievements:**
- Complete boot from EL2
- Virtual memory with MMU
- Preemptive multitasking
- User mode support with syscalls
- Interrupt handling with GIC
- Timer-based scheduling

**Future Enhancements:**
- Multi-core support (SMP)
- Proper blocking (sleep queue)
- More system calls (fork, exec, wait)
- Filesystem support
- Network stack
- Full hypervisor with VM isolation

For questions or contributions, please refer to the project repository.

**Happy Hacking! 🚀**
