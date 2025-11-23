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
        task_pool[i].stack_base = NULL;
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

    /* Allocate stack (single page = 4KB) */
    struct page *stack_page = alloc_pages(0);
    if (!stack_page) {
        uart_puts("ERROR: Failed to allocate task stack\n");
        return 0;
    }
    task->stack_base = (void *)page_to_phys(stack_page);

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

    /* Initialize context */
    memset(&task->context, 0, sizeof(struct exception_frame));

    /* Set up initial stack frame */
    stack_ptr = (uint64_t *)((uint8_t *)task->stack_base + TASK_STACK_SIZE);
    stack_ptr = (uint64_t *)((uint64_t)stack_ptr & ~0xFUL);  /* 16-byte align */

    /* Set initial register values */
    task->context.elr = (uint64_t)func;                /* Entry point */
    task->context.sp = (uint64_t)stack_ptr;            /* Stack pointer */
    task->context.spsr = 0x3C5;                        /* EL1h, IRQs enabled */
    task->context.x0 = (uint64_t)arg;                  /* Argument */

    task->stack_top = (void *)stack_ptr;

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

    /* Free stack */
    if (current_task->stack_base) {
        struct page *stack_page = phys_to_page((uint64_t)current_task->stack_base);
        free_pages(stack_page, 0);
        current_task->stack_base = NULL;
    }

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
