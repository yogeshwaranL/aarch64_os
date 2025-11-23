# Scheduler and Process Management

## AArch64 Scheduler Implementation

**Version:** 0.1.0
**Component:** Scheduler and Process Management
**Implementation Phase:** Phase 6

---

## Overview

The scheduler implements preemptive multi-tasking with:
- **Priority-based scheduling** with multiple priority levels
- **Round-robin** within same priority
- **SMP support** with load balancing across CPU cores
- **Real-time scheduling** classes
- **Context switching** with full register save/restore
- **Process and thread management**

---

## Task Control Block (TCB)

```c
/**
 * @brief Process/Thread states
 */
enum task_state {
    TASK_RUNNING,       // Currently running or runnable
    TASK_INTERRUPTIBLE, // Sleeping, can be woken by signal
    TASK_UNINTERRUPTIBLE, // Sleeping, cannot be interrupted
    TASK_STOPPED,       // Stopped by signal (SIGSTOP)
    TASK_ZOMBIE,        // Terminated, waiting for parent
    TASK_DEAD           // Being cleaned up
};

/**
 * @brief Task Control Block structure
 * Represents a process or thread in the system
 */
struct task_struct {
    // Scheduling
    volatile long state;        // Task state
    int prio;                   // Static priority
    int dynamic_prio;           // Dynamic priority
    unsigned long policy;       // Scheduling policy
    struct list_head run_list;  // Runqueue linkage
    unsigned long time_slice;   // Remaining time slice
    unsigned long runtime;      // Total runtime
    int on_cpu;                 // Currently on CPU (-1 if not)

    // CPU affinity and SMP
    unsigned long cpus_allowed; // CPU affinity mask
    int cpu;                    // Last/current CPU

    // Process management
    pid_t pid;                  // Process ID
    pid_t tgid;                 // Thread group ID
    struct task_struct *parent; // Parent process
    struct list_head children;  // Child processes
    struct list_head sibling;   // Sibling linkage

    // Memory management
    struct mm_struct *mm;       // Memory descriptor
    struct mm_struct *active_mm; // Active mm (for kernel threads)

    // Context (CPU registers)
    struct cpu_context cpu_context; // Callee-saved registers
    struct pt_regs *thread_info;    // Exception stack frame

    // File system (future)
    struct files_struct *files; // Open files

    // Signal handling (future)
    struct signal_struct *signal;
    sigset_t blocked;           // Blocked signals

    // Thread information
    unsigned long flags;        // Task flags (PF_*)
    void *stack;                // Kernel stack
    unsigned long stack_size;

    // Statistics
    unsigned long utime;        // User time
    unsigned long stime;        // System time
    unsigned long nvcsw;        // Voluntary context switches
    unsigned long nivcsw;       // Involuntary context switches

    // Name
    char comm[16];              // Task command name
};

/**
 * @brief CPU context (callee-saved registers per ARM AAPCS64)
 */
struct cpu_context {
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long fp;           // x29 - Frame pointer
    unsigned long sp;           // Stack pointer
    unsigned long pc;           // Program counter (return address)
};

/**
 * @brief Exception stack frame (pt_regs)
 * Saved on kernel stack when exception occurs
 */
struct pt_regs {
    unsigned long regs[31];     // x0-x30
    unsigned long sp;           // Stack pointer
    unsigned long pc;           // Program counter
    unsigned long pstate;       // Processor state (SPSR_EL1)
};
```

---

## Runqueue Structure

```c
/**
 * @brief Per-priority runqueue
 */
struct prio_array {
    unsigned long bitmap[BITS_TO_LONGS(MAX_PRIO)]; // Priority bitmap
    struct list_head queue[MAX_PRIO];              // Per-priority lists
    int nr_active;                                 // Number of tasks
};

/**
 * @brief Per-CPU runqueue
 */
struct rq {
    spinlock_t lock;            // Protects runqueue
    unsigned long nr_running;   // Number of runnable tasks
    unsigned long nr_switches;  // Context switch count

    struct task_struct *curr;   // Currently running task
    struct task_struct *idle;   // Idle task for this CPU

    struct prio_array *active;  // Active priority array
    struct prio_array *expired; // Expired priority array
    struct prio_array arrays[2];// Two arrays for active/expired

    // Load balancing
    unsigned long cpu_load;
    unsigned long nr_load_updates;

    // Statistics
    u64 clock;                  // Runqueue clock
    u64 prev_irq_time;          // Previous IRQ time
};

// Per-CPU runqueues
DEFINE_PER_CPU(struct rq, runqueues);
```

---

## Scheduling Algorithm

### Priority Levels

```c
#define MAX_RT_PRIO     100     // Real-time priorities: 0-99
#define MAX_PRIO        140     // Total priorities: 0-139
#define DEFAULT_PRIO    120     // Default user priority

#define NICE_TO_PRIO(nice)  ((nice) + DEFAULT_PRIO)
#define PRIO_TO_NICE(prio)  ((prio) - DEFAULT_PRIO)
```

### Time Slice Calculation

```c
/**
 * @brief Calculate time slice based on priority
 * Higher priority tasks get longer time slices
 */
static unsigned long task_timeslice(struct task_struct *p)
{
    if (p->policy == SCHED_RR || p->policy == SCHED_FIFO) {
        // Real-time tasks: fixed time slice
        return RT_TIMESLICE;  // 100ms
    } else {
        // Normal tasks: priority-based
        int nice = PRIO_TO_NICE(p->prio);
        return (20 - nice) * 5 * HZ / 1000;  // 5-100ms range
    }
}
```

### Core Scheduler

```c
/**
 * @brief Main scheduler function
 * Called on timer interrupt or when task yields/blocks
 */
asmlinkage void schedule(void)
{
    struct rq *rq;
    struct task_struct *prev, *next;
    unsigned long flags;
    int cpu;

    cpu = smp_processor_id();
    rq = cpu_rq(cpu);

    spin_lock_irqsave(&rq->lock, flags);

    prev = rq->curr;
    prev->runtime += rq->clock - prev->timestamp;

    // If task used up time slice, move to expired array
    if (prev->state == TASK_RUNNING && prev->time_slice == 0) {
        dequeue_task(prev, rq->active);
        prev->prio = effective_prio(prev);  // Recalculate priority
        prev->time_slice = task_timeslice(prev);
        enqueue_task(prev, rq->expired);
    }

    // Pick next task
    next = pick_next_task(rq);

    if (next != prev) {
        rq->nr_switches++;
        rq->curr = next;
        next->timestamp = rq->clock;
        next->on_cpu = cpu;

        // Context switch
        context_switch(rq, prev, next);
    }

    spin_unlock_irqrestore(&rq->lock, flags);
}

/**
 * @brief Pick next task to run
 */
static struct task_struct *pick_next_task(struct rq *rq)
{
    struct prio_array *array = rq->active;
    struct task_struct *next;
    int idx;

    if (array->nr_active == 0) {
        // Active array empty, swap with expired
        array = rq->expired;
        rq->expired = rq->active;
        rq->active = array;

        if (array->nr_active == 0) {
            // No runnable tasks, return idle
            return rq->idle;
        }
    }

    // Find highest priority non-empty queue
    idx = find_first_bit(array->bitmap, MAX_PRIO);
    next = list_first_entry(&array->queue[idx], struct task_struct, run_list);

    return next;
}
```

---

## Context Switching

### High-Level Context Switch

```c
/**
 * @brief Context switch between tasks
 */
static inline void context_switch(struct rq *rq,
                                   struct task_struct *prev,
                                   struct task_struct *next)
{
    struct mm_struct *oldmm = prev->active_mm;
    struct mm_struct *mm = next->mm;

    // Switch address space (if needed)
    if (!mm) {
        // Kernel thread, use previous mm
        next->active_mm = oldmm;
        atomic_inc(&oldmm->mm_count);
    } else {
        // User task, switch page tables
        switch_mm(oldmm, mm, next);
    }

    // Switch CPU context (assembly)
    cpu_switch_to(prev, next);
}
```

### Low-Level Context Switch (Assembly)

```asm
/**
 * @brief CPU context switch
 * x0 = previous task_struct
 * x1 = next task_struct
 */
.global cpu_switch_to
cpu_switch_to:
    // Save callee-saved registers of prev task
    mov     x9, sp
    stp     x19, x20, [x0, #CPU_CTX_X19]
    stp     x21, x22, [x0, #CPU_CTX_X21]
    stp     x23, x24, [x0, #CPU_CTX_X23]
    stp     x25, x26, [x0, #CPU_CTX_X25]
    stp     x27, x28, [x0, #CPU_CTX_X27]
    stp     x29, x9,  [x0, #CPU_CTX_FP]   // FP and SP
    str     x30,      [x0, #CPU_CTX_PC]   // LR (return address)

    // Restore callee-saved registers of next task
    ldp     x19, x20, [x1, #CPU_CTX_X19]
    ldp     x21, x22, [x1, #CPU_CTX_X21]
    ldp     x23, x24, [x1, #CPU_CTX_X23]
    ldp     x25, x26, [x1, #CPU_CTX_X25]
    ldp     x27, x28, [x1, #CPU_CTX_X27]
    ldp     x29, x9,  [x1, #CPU_CTX_FP]   // FP and SP
    ldr     x30,      [x1, #CPU_CTX_PC]   // LR
    mov     sp, x9

    // Return to next task (via x30/LR)
    ret
```

---

## Process Creation

```c
/**
 * @brief Create a new process (fork)
 */
int do_fork(unsigned long clone_flags, unsigned long stack_start,
            struct pt_regs *regs, unsigned long stack_size)
{
    struct task_struct *p;

    // Allocate new task struct
    p = alloc_task_struct();
    if (!p)
        return -ENOMEM;

    // Copy parent's task struct
    *p = *current;

    // Assign new PID
    p->pid = alloc_pid();
    p->state = TASK_RUNNING;
    p->time_slice = task_timeslice(p);

    // Copy or share memory
    if (clone_flags & CLONE_VM) {
        // Share memory (thread)
        p->mm = current->mm;
        atomic_inc(&current->mm->mm_users);
    } else {
        // Copy memory (process)
        p->mm = copy_mm(current->mm);
    }

    // Allocate kernel stack
    p->stack = alloc_pages(THREAD_SIZE_ORDER);
    if (!p->stack) {
        free_task_struct(p);
        return -ENOMEM;
    }

    // Setup child's registers (will return 0 from fork)
    struct pt_regs *childregs = task_pt_regs(p);
    *childregs = *regs;
    childregs->regs[0] = 0;  // Return value for child

    // Setup child's CPU context to return to ret_from_fork
    p->cpu_context.pc = (unsigned long)ret_from_fork;
    p->cpu_context.sp = (unsigned long)childregs;

    // Add to runqueue
    wake_up_new_task(p);

    return p->pid;  // Return child PID to parent
}
```

---

## SMP Load Balancing

```c
/**
 * @brief Periodic load balancing across CPUs
 */
static void load_balance(struct rq *this_rq)
{
    int this_cpu = smp_processor_id();
    int busiest_cpu = -1;
    unsigned long max_load = this_rq->cpu_load;
    struct rq *busiest_rq;
    struct task_struct *p;

    // Find busiest CPU
    for_each_online_cpu(cpu) {
        if (cpu == this_cpu)
            continue;

        struct rq *rq = cpu_rq(cpu);
        if (rq->nr_running > 1 && rq->cpu_load > max_load) {
            max_load = rq->cpu_load;
            busiest_cpu = cpu;
        }
    }

    if (busiest_cpu == -1)
        return;  // No imbalance

    busiest_rq = cpu_rq(busiest_cpu);

    // Try to pull a task
    double_rq_lock(this_rq, busiest_rq);

    p = pull_task(busiest_rq, this_rq);
    if (p) {
        p->cpu = this_cpu;
        enqueue_task(p, this_rq->active);
    }

    double_rq_unlock(this_rq, busiest_rq);
}
```

---

## Scheduling Policies

```c
#define SCHED_NORMAL    0  // Normal time-sharing
#define SCHED_FIFO      1  // Real-time FIFO
#define SCHED_RR        2  // Real-time round-robin
#define SCHED_BATCH     3  // Batch processing
#define SCHED_IDLE      5  // Very low priority

/**
 * @brief Set scheduling policy
 */
int sched_setscheduler(struct task_struct *p, int policy,
                       const struct sched_param *param);
```

---

**Next Document**: [Hypervisor](04-hypervisor.md)
