# User Mode (EL0) Implementation Plan

## Status: Phase 1/6 Complete

### Completed ✅

**Phase 1: User Programs and Build Infrastructure**
- Created `kernel/user/user_hello.S` - Test program with syscalls
- Created `kernel/user/user_counter.S` - Counter program
- Created `kernel/user/Makefile` - Build system for user programs
- Created `kernel/user/user.ld` - Linker script (loads at 0x400000)
- **Commit**: `d423972` - "Add user mode programs and build infrastructure (Phase 1/6)"

### Next Steps 🔄

#### Phase 2: Implement `task_create_user()` Function

**Location**: `kernel/sched/sched.c`

**Function Signature**:
```c
task_id_t task_create_user(const char *name, uint64_t entry_point, uint32_t priority)
```

**Implementation Steps**:
1. Find free task slot (same as `task_create()`)
2. Allocate kernel stack (4KB) - for exception handling
3. Allocate user stack (4KB)
4. Create user page table: `page_table_t *user_pgd = mmu_create_user_table()`
5. Allocate physical pages for user program code/data
6. Map user program to user address space (0x400000)
7. Map user stack to high address (0x7FFFF000)
8. Initialize task structure:
   - `task->level = TASK_USER`
   - `task->page_table = user_pgd`
   - `task->user_stack_base` and `task->user_stack_top`
9. Set up exception frame:
   - `task->context.elr = entry_point` (0x400000)
   - `task->context.sp = 0x7FFFF000 + PAGE_SIZE` (user stack top)
   - `task->context.spsr = 0x3C0` (EL0t, IRQs enabled)
   - Clear all general purpose registers

**Key Constants**:
```c
#define USER_PROG_BASE    0x0000000000400000UL   /* 4MB in user space */
#define USER_STACK_BASE   0x0000007FFFF000UL    /* Near top of user space */
#define SPSR_EL0T         0x3C0                 /* EL0t, interrupts enabled */
```

#### Phase 3: Update Context Switching

**Location**: `kernel/sched/sched.c` - `schedule()` function

**Required Changes**:
```c
void schedule(void)
{
    // ... existing code ...

    /* Switch to next task */
    next_task->state = TASK_RUNNING;
    next_task->time_slice = DEFAULT_TIME_SLICE;
    next_task->run_count++;

    /* NEW: Switch user page table if needed */
    if (next_task->level == TASK_USER && next_task->page_table) {
        mmu_switch_user_table(next_task->page_table);
    } else if (next_task->level == TASK_KERNEL) {
        /* Switch back to no user table */
        mmu_switch_user_table(NULL);
    }

    /* Perform context switch */
    struct task *old_task = current_task;
    current_task = next_task;
    switch_context(&old_task->context, &next_task->context);
}
```

#### Phase 4: Build Integration

**Update**: `kernel/Makefile`

Add user program build step:
```makefile
# Add to dependencies
kernel.elf: ... user_programs

# Add user programs target
.PHONY: user_programs
user_programs:
	$(MAKE) -C user BUILD_DIR=../../$(BUILD_DIR)/kernel/user

# Link user program objects
KERNEL_OBJS += $(BUILD_DIR)/kernel/user/user_hello.o \
               $(BUILD_DIR)/kernel/user/user_counter.o
```

**Create**: `kernel/include/user_programs.h`
```c
#ifndef _USER_PROGRAMS_H
#define _USER_PROGRAMS_H

#include "kernel.h"

/* Embedded user programs (linked as binary data) */
extern uint8_t _binary_user_hello_bin_start[];
extern uint8_t _binary_user_hello_bin_end[];
extern uint8_t _binary_user_hello_bin_size[];

extern uint8_t _binary_user_counter_bin_start[];
extern uint8_t _binary_user_counter_bin_end[];
extern uint8_t _binary_user_counter_bin_size[];

#endif /* _USER_PROGRAMS_H */
```

#### Phase 5: Testing

**Update**: `kernel/core/main.c`

Add user task creation after kernel initialization:
```c
void kernel_main(void *dtb, uint64_t el)
{
    // ... existing initialization ...

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Creating User Mode Tasks\n");
    uart_puts("========================================\n");

    /* Create user tasks */
    task_create_user("user-hello", 0x400000, 10);
    task_create_user("user-counter", 0x400000, 10);

    uart_puts("User tasks created!\n");
    uart_puts("========================================\n");

    // ... rest of main ...
}
```

**Expected Output**:
```
[USER] Hello from user mode!
[USER] Hello from user mode!
[USER] Counter task starting...
[USER] Counting...
[USER] Goodbye from user mode!
[USER] Counting...
[USER] Counter task done!
```

#### Phase 6: Final Commit

Commit message template:
```
Implement user mode (EL0) task support (Phase 2-5/6)

- Implemented task_create_user() function
- Updated context switching for user page tables
- Integrated user program build into kernel
- Added test user tasks in main.c
- Verified syscalls work from EL0
- Tested memory isolation between tasks

User tasks now run at EL0 with their own address spaces,
making syscalls to the kernel at EL1 via SVC instruction.
```

## Technical Notes

### Exception Levels
- **Kernel**: Runs at EL1 (uses SPSR_EL1, ELR_EL1)
- **User Tasks**: Run at EL0 (SPSR = 0x3C0 = EL0t)
- **System Calls**: SVC from EL0 → EL1

### Memory Layout
```
User Space (TTBR0_EL1):
  0x0000000000000000 - 0x00000000003FFFFF   Reserved
  0x0000000000400000 - 0x00000000004FFFFF   User program code/data
  0x0000007FFFF000 - 0x0000007FFFFFFF       User stack (4KB)

Kernel Space (TTBR1_EL1):
  0xFFFFFF8000000000 - 0xFFFFFFFFFFFFFFFF   Kernel (identity mapped)
```

### SPSR Bits for EL0t
```
SPSR = 0x3C0 = 0b0000001111000000
  Bits [3:0] = 0b0000 = EL0t (EL0 with SP_EL0)
  Bit 6 (F) = 0 (FIQ not masked)
  Bit 7 (I) = 0 (IRQ not masked)
  Bit 8 (A) = 0 (SError not masked)
  Bit 9 (D) = 1 (Debug masked)
```

### User Program Binary Format
- Assembled as position-independent code
- Linked at 0x400000
- Converted to flat binary
- Embedded in kernel as `_binary_*_bin_start/end` symbols

## Issues to Watch

1. **Exception Level**: Kernel uses EL1 registers (SPSR_EL1, ELR_EL1) but might boot at EL2
2. **Memory Allocation**: Need physical pages for user code/data
3. **TLB Invalidation**: Must invalidate TLB when switching user tables
4. **Stack Alignment**: User stack must be 16-byte aligned
5. **Register State**: Clear all registers before first EL0 entry

## References
- User programs: `kernel/user/user_*.S`
- MMU functions: `kernel/include/mmu.h`, `kernel/mm/mmu.c`
- Task structure: `kernel/include/task.h`
- Scheduler: `kernel/sched/sched.c`
- Context switch: `kernel/sched/switch.S`
