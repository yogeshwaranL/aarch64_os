/*
 * AArch64 Bare Metal OS - Task Management Header
 *
 * Task Control Block and task state definitions
 */

#ifndef _TASK_H
#define _TASK_H

#include "kernel.h"
#include "exception.h"

/* Task states */
typedef enum {
    TASK_READY = 0,                             /* Ready to run */
    TASK_RUNNING,                               /* Currently running */
    TASK_BLOCKED,                               /* Waiting for event */
    TASK_TERMINATED                             /* Finished execution */
} task_state_t;

/* Task stack size (4KB per task - single page) */
#define TASK_STACK_SIZE     (4 * 1024)

/* Maximum number of tasks */
#define MAX_TASKS           32

/* Task ID type */
typedef uint32_t task_id_t;

/* Task execution level */
typedef enum {
    TASK_KERNEL = 0,                            /* Kernel task (EL1) */
    TASK_USER                                   /* User task (EL0) */
} task_level_t;

/* Task Control Block */
struct task {
    /* Task identification */
    task_id_t id;
    char name[32];

    /* Scheduling info */
    task_state_t state;
    uint32_t priority;                          /* 0 = highest */
    uint64_t time_slice;                        /* Remaining time slice */

    /* Context (saved registers) */
    struct exception_frame context;

    /* Stack - kernel and user */
    void *kernel_stack_base;                    /* Bottom of kernel stack */
    void *kernel_stack_top;                     /* Top of kernel stack */
    void *user_stack_base;                      /* Bottom of user stack (if user task) */
    void *user_stack_top;                       /* Top of user stack (if user task) */

    /* Memory management (for user tasks) */
    void *page_table;                           /* User page table (NULL for kernel tasks) */

    /* Execution level */
    task_level_t level;                         /* EL0 (user) or EL1 (kernel) */

    /* Statistics */
    uint64_t run_count;                         /* Number of times scheduled */
    uint64_t total_runtime;                     /* Total CPU time in ticks */

    /* List linkage */
    struct task *next;
    struct task *prev;
};

/* Task function prototype */
typedef void (*task_func_t)(void *arg);

#endif /* _TASK_H */
