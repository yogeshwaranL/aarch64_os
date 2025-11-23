/*
 * AArch64 Bare Metal OS - Scheduler Header
 *
 * Round-robin preemptive scheduler
 */

#ifndef _SCHED_H
#define _SCHED_H

#include "kernel.h"
#include "task.h"

/* Default time slice (in timer ticks) */
#define DEFAULT_TIME_SLICE  10                  /* 10 ticks = 100ms at 100Hz */

/* Scheduler functions */
void sched_init(void);
task_id_t task_create(const char *name, task_func_t func, void *arg, uint32_t priority);
task_id_t task_create_user(const char *name, const void *binary, size_t binary_size, uint32_t priority);
void task_exit(void);
void task_yield(void);
void task_sleep(uint32_t ms);

/* Scheduler internal functions */
void schedule(void);                            /* Called from timer interrupt */
struct task *sched_get_current(void);
void sched_dump_tasks(void);

/* Context switching (implemented in assembly) */
void switch_context(struct exception_frame *old_ctx, struct exception_frame *new_ctx);

#endif /* _SCHED_H */
