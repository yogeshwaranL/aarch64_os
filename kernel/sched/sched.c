/*
 * AArch64 Bare Metal OS - Scheduler Implementation
 *
 * Round-robin preemptive scheduler
 */

#include "kernel.h"
#include "sched.h"
#include "task.h"
#include "kmalloc.h"
#include "pmm.h"
#include "mmu.h"
#include "uart.h"
#include "timer.h"

/* Task storage */
static struct task task_pool[MAX_TASKS];
static struct task *current_task = NULL;
static struct task *ready_queue_head = NULL;
static struct task *ready_queue_tail = NULL;

/* Next task ID */
static task_id_t next_task_id = 1;

/* Scheduling enabled flag */
static int sched_enabled = 0;

/* Forward declarations */
static void idle_task_func(void *arg);
static void add_to_ready_queue(struct task *task);
static void remove_from_ready_queue(struct task *task);
static struct task *get_next_ready_task(void);

/*
 * Initialize scheduler
 */
void sched_init(void)
{
    int i;

    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("Initializing Scheduler\n");
    uart_puts("========================================\n");

    /* Initialize task pool */
    for (i = 0; i < MAX_TASKS; i++) {
        task_pool[i].state = TASK_TERMINATED;
        task_pool[i].id = 0;
        task_pool[i].kernel_stack_base = NULL;
        task_pool[i].kernel_stack_top = NULL;
        task_pool[i].user_stack_base = NULL;
        task_pool[i].user_stack_top = NULL;
        task_pool[i].page_table = NULL;
        task_pool[i].level = TASK_KERNEL;
    }

    /* Create idle task (always runs when no other task is ready) */
    task_create("idle", idle_task_func, NULL, 255);

    uart_puts("Scheduler initialized\n");
    uart_puts("Time slice: ");
    uart_puthex(DEFAULT_TIME_SLICE);
    uart_puts(" ticks\n");
    uart_puts("========================================\n");
    uart_puts("\n");

    /* Enable scheduling */
    sched_enabled = 1;
}

/*
 * Idle task - runs when no other task is ready
 */
static void idle_task_func(void *arg)
{
    (void)arg;

    while (1) {
        /* Just wait for interrupts */
        __asm__ volatile("wfi");
    }
}

/*
 * Create a new task
 */
task_id_t task_create(const char *name, task_func_t func, void *arg, uint32_t priority)
{
    struct task *task = NULL;
    int i;
    uint64_t *stack_ptr;

    /* Find free task slot */
    for (i = 0; i < MAX_TASKS; i++) {
        if (task_pool[i].state == TASK_TERMINATED) {
            task = &task_pool[i];
            break;
        }
    }

    if (!task) {
        uart_puts("ERROR: No free task slots\n");
        return 0;
    }

    /* Allocate kernel stack (single page = 4KB) */
    struct page *stack_page = alloc_pages(0);
    if (!stack_page) {
        uart_puts("ERROR: Failed to allocate task stack\n");
        return 0;
    }
    task->kernel_stack_base = (void *)page_to_phys(stack_page);

    /* Initialize task */
    task->id = next_task_id++;
    for (i = 0; i < 31 && name[i]; i++) {
        task->name[i] = name[i];
    }
    task->name[i] = '\0';

    task->priority = priority;
    task->time_slice = DEFAULT_TIME_SLICE;
    task->run_count = 0;
    task->total_runtime = 0;
    task->level = TASK_KERNEL;                         /* Kernel task by default */
    task->page_table = NULL;                           /* No user page table */
    task->user_stack_base = NULL;
    task->user_stack_top = NULL;

    /* Initialize context */
    memset(&task->context, 0, sizeof(struct exception_frame));

    /* Set up initial stack frame */
    stack_ptr = (uint64_t *)((uint8_t *)task->kernel_stack_base + TASK_STACK_SIZE);
    stack_ptr = (uint64_t *)((uint64_t)stack_ptr & ~0xFUL);  /* 16-byte align */

    /* Set initial register values */
    task->context.elr = (uint64_t)func;                /* Entry point */
    task->context.sp = (uint64_t)stack_ptr;            /* Stack pointer */
    task->context.spsr = 0x3C5;                        /* EL1h, IRQs enabled */
    task->context.x0 = (uint64_t)arg;                  /* Argument */

    task->kernel_stack_top = (void *)stack_ptr;

    /* Set state and add to ready queue */
    task->state = TASK_READY;

    if (sched_enabled) {
        add_to_ready_queue(task);
    } else {
        /* First task (idle) becomes current */
        current_task = task;
        task->state = TASK_RUNNING;
    }

    uart_puts("Created task: ");
    uart_puts(task->name);
    uart_puts(" (ID ");
    uart_puthex(task->id);
    uart_puts(", priority ");
    uart_puthex(task->priority);
    uart_puts(")\n");

    return task->id;
}

/*
 * Add task to ready queue
 */
static void add_to_ready_queue(struct task *task)
{
    task->next = NULL;
    task->prev = ready_queue_tail;

    if (ready_queue_tail) {
        ready_queue_tail->next = task;
    }

    ready_queue_tail = task;

    if (!ready_queue_head) {
        ready_queue_head = task;
    }
}

/*
 * Remove task from ready queue
 */
static void remove_from_ready_queue(struct task *task)
{
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        ready_queue_head = task->next;
    }

    if (task->next) {
        task->next->prev = task->prev;
    } else {
        ready_queue_tail = task->prev;
    }

    task->next = NULL;
    task->prev = NULL;
}

/*
 * Get next ready task (round-robin)
 */
static struct task *get_next_ready_task(void)
{
    struct task *task;

    if (!ready_queue_head) {
        return NULL;
    }

    /* Get task from head of queue */
    task = ready_queue_head;
    remove_from_ready_queue(task);

    return task;
}

/*
 * Schedule next task
 * Called from timer interrupt with IRQs disabled
 */
void schedule(void)
{
    struct task *next_task;

    if (!sched_enabled || !current_task) {
        return;
    }

    /* Decrement current task's time slice */
    if (current_task->time_slice > 0) {
        current_task->time_slice--;
    }

    /* If time slice expired or task yielded/blocked, switch */
    if (current_task->time_slice == 0 || current_task->state != TASK_RUNNING) {
        /* Save current state if still runnable */
        if (current_task->state == TASK_RUNNING) {
            current_task->state = TASK_READY;
            current_task->time_slice = DEFAULT_TIME_SLICE;
            add_to_ready_queue(current_task);
        }

        /* Get next task */
        next_task = get_next_ready_task();
        if (!next_task) {
            return;  /* Stay on current task */
        }

        /* Switch to next task */
        next_task->state = TASK_RUNNING;
        next_task->time_slice = DEFAULT_TIME_SLICE;
        next_task->run_count++;

        /* Switch user page table if needed */
        if (next_task->level == TASK_USER && next_task->page_table) {
            mmu_switch_user_table((page_table_t *)next_task->page_table);
        } else if (next_task->level == TASK_KERNEL) {
            /* Switch back to no user table */
            mmu_switch_user_table(NULL);
        }

        /* Perform context switch */
        struct task *old_task = current_task;
        current_task = next_task;

        /* Context switch happens in assembly */
        switch_context(&old_task->context, &next_task->context);
    }
}

/*
 * Get current task
 */
struct task *sched_get_current(void)
{
    return current_task;
}

/*
 * Yield CPU to next task
 */
void task_yield(void)
{
    if (!current_task) return;

    /* Force reschedule */
    current_task->time_slice = 0;
}

/*
 * Create a new user mode task
 */
task_id_t task_create_user(const char *name, const void *binary, size_t binary_size, uint32_t priority)
{
    struct task *task = NULL;
    int i;
    uint64_t *stack_ptr;
    size_t num_pages;
    struct page *prog_pages;
    uint64_t prog_phys;
    const uint64_t USER_PROG_BASE = 0x0000000000400000UL;  /* 4MB in user space */
    const uint64_t USER_STACK_BASE = 0x0000007FFFF000UL;   /* Near top of user space */

    /* Find free task slot */
    for (i = 0; i < MAX_TASKS; i++) {
        if (task_pool[i].state == TASK_TERMINATED) {
            task = &task_pool[i];
            break;
        }
    }

    if (!task) {
        uart_puts("ERROR: No free task slots\n");
        return 0;
    }

    /* Allocate kernel stack (for exception handling) */
    struct page *kernel_stack_page = alloc_pages(0);
    if (!kernel_stack_page) {
        uart_puts("ERROR: Failed to allocate kernel stack\n");
        return 0;
    }
    task->kernel_stack_base = (void *)page_to_phys(kernel_stack_page);

    /* Allocate user stack */
    struct page *user_stack_page = alloc_pages(0);
    if (!user_stack_page) {
        uart_puts("ERROR: Failed to allocate user stack\n");
        free_pages(kernel_stack_page, 0);
        return 0;
    }
    task->user_stack_base = (void *)page_to_phys(user_stack_page);

    /* Allocate pages for user program code/data */
    num_pages = (binary_size + PAGE_SIZE - 1) / PAGE_SIZE;  /* Round up */
    if (num_pages == 0) num_pages = 1;

    /* For simplicity, allocate order based on num_pages (up to order 3 = 8 pages) */
    uint32_t order = 0;
    size_t order_pages = 1;
    while (order_pages < num_pages && order < 3) {
        order++;
        order_pages *= 2;
    }

    prog_pages = alloc_pages(order);
    if (!prog_pages) {
        uart_puts("ERROR: Failed to allocate user program pages\n");
        free_pages(kernel_stack_page, 0);
        free_pages(user_stack_page, 0);
        return 0;
    }
    prog_phys = page_to_phys(prog_pages);

    /* Copy user program binary to allocated pages */
    memcpy((void *)prog_phys, binary, binary_size);

    /* Create user page table */
    page_table_t *user_pgd = mmu_create_user_table();
    if (!user_pgd) {
        uart_puts("ERROR: Failed to create user page table\n");
        free_pages(kernel_stack_page, 0);
        free_pages(user_stack_page, 0);
        free_pages(prog_pages, order);
        return 0;
    }

    /* Map user program to user address space (0x400000) */
    mmu_map_user_range(user_pgd, USER_PROG_BASE, prog_phys,
                       order_pages * PAGE_SIZE, PAGE_USER_RX);

    /* Map user stack to high address (0x7FFFF000) */
    mmu_map_user_page(user_pgd, USER_STACK_BASE, (uint64_t)task->user_stack_base, PAGE_USER_RW);
    task->user_stack_top = (void *)(USER_STACK_BASE + PAGE_SIZE);

    /* Initialize task */
    task->id = next_task_id++;
    for (i = 0; i < 31 && name[i]; i++) {
        task->name[i] = name[i];
    }
    task->name[i] = '\0';

    task->priority = priority;
    task->time_slice = DEFAULT_TIME_SLICE;
    task->run_count = 0;
    task->total_runtime = 0;
    task->level = TASK_USER;                           /* User task */
    task->page_table = user_pgd;                       /* User page table */

    /* Initialize context */
    memset(&task->context, 0, sizeof(struct exception_frame));

    /* Set up kernel stack (for exception entry) */
    stack_ptr = (uint64_t *)((uint8_t *)task->kernel_stack_base + TASK_STACK_SIZE);
    stack_ptr = (uint64_t *)((uint64_t)stack_ptr & ~0xFUL);  /* 16-byte align */
    task->kernel_stack_top = (void *)stack_ptr;

    /* Set initial register values for EL0 entry */
    task->context.elr = USER_PROG_BASE;                /* Entry point (0x400000) */
    task->context.sp = (uint64_t)task->user_stack_top; /* User stack pointer */
    task->context.spsr = 0x3C0;                        /* EL0t, IRQs enabled */

    /* Clear all general purpose registers */
    task->context.x0 = 0;
    task->context.x1 = 0;
    /* x2-x30 already zeroed by memset */

    /* Set state and add to ready queue */
    task->state = TASK_READY;

    if (sched_enabled) {
        add_to_ready_queue(task);
    }

    uart_puts("Created user task: ");
    uart_puts(task->name);
    uart_puts(" (ID ");
    uart_puthex(task->id);
    uart_puts(", size ");
    uart_puthex(binary_size);
    uart_puts(" bytes)\n");

    return task->id;
}

/*
 * Exit current task
 */
void task_exit(void)
{
    if (!current_task) return;

    uart_puts("Task ");
    uart_puts(current_task->name);
    uart_puts(" exiting\n");

    /* Mark as terminated */
    current_task->state = TASK_TERMINATED;

    /* Free kernel stack */
    if (current_task->kernel_stack_base) {
        struct page *stack_page = phys_to_page((uint64_t)current_task->kernel_stack_base);
        free_pages(stack_page, 0);
        current_task->kernel_stack_base = NULL;
    }

    /* Free user stack if it exists */
    if (current_task->user_stack_base) {
        struct page *stack_page = phys_to_page((uint64_t)current_task->user_stack_base);
        free_pages(stack_page, 0);
        current_task->user_stack_base = NULL;
    }

    /* TODO: Free user page table if it exists */

    /* Force immediate reschedule */
    current_task->time_slice = 0;
    schedule();

    /* Should never reach here */
    while (1) __asm__ volatile("wfe");
}

/*
 * Sleep for specified milliseconds
 */
void task_sleep(uint32_t ms)
{
    uint64_t start = timer_get_uptime_ms();

    /* Simple busy-wait for now (Phase 8 will add proper blocking) */
    while ((timer_get_uptime_ms() - start) < ms) {
        task_yield();
    }
}

/*
 * Dump task list
 */
void sched_dump_tasks(void)
{
    int i;
    const char *state_names[] = {"READY", "RUNNING", "BLOCKED", "TERMINATED"};

    uart_puts("\nTask List:\n");
    uart_puts("==========\n");

    for (i = 0; i < MAX_TASKS; i++) {
        struct task *t = &task_pool[i];
        if (t->state != TASK_TERMINATED) {
            uart_puts("  [");
            uart_puthex(t->id);
            uart_puts("] ");
            uart_puts(t->name);
            uart_puts(" - ");
            uart_puts(state_names[t->state]);
            uart_puts(" (runs: ");
            uart_puthex(t->run_count);
            uart_puts(", pri: ");
            uart_puthex(t->priority);
            uart_puts(")\n");
        }
    }

    if (current_task) {
        uart_puts("\nCurrent: ");
        uart_puts(current_task->name);
        uart_puts("\n");
    }
}
