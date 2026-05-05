#include "scheduler.h"
#include "../mm/kmalloc.h"
#include "../lib/string.h"
#include "../lib/kprintf.h"
#include "../lib/panic.h"

/* Assembly: switch_to(&old_rsp, new_rsp) */
extern void switch_to(uint64_t *old_rsp, uint64_t new_rsp);

static process_t  proc_table[MAX_PROCESSES];
static int        n_procs;
static int        current_idx;
static uint32_t   next_pid;

void scheduler_init(void) {
    n_procs = 0;
    current_idx = 0;
    next_pid = 0;

    /* Create idle process representing the boot thread */
    process_t *idle = &proc_table[0];
    idle->pid    = next_pid++;
    idle->state  = PROC_RUNNING;
    idle->stack  = 0;  /* uses the boot stack */
    strncpy(idle->name, "idle", PROC_NAME_LEN);
    n_procs = 1;
}

process_t *scheduler_current(void) {
    return &proc_table[current_idx];
}

/*
 * Layout of a newly-created process's stack (grows downward):
 *   [rbx=0, rbp=0, r12=0, r13=0, r14=0, r15=0, RIP=entry]
 * switch_to() will pop these and ret to entry.
 */
static void setup_stack(process_t *p, void (*entry)(void)) {
    uint64_t *sp = (uint64_t *)(p->stack_top);
    *--sp = (uint64_t)scheduler_exit; /* return address if entry() returns */
    *--sp = (uint64_t)entry;  /* RIP restored by switch_to's ret */
    *--sp = 0;  /* r15 */
    *--sp = 0;  /* r14 */
    *--sp = 0;  /* r13 */
    *--sp = 0;  /* r12 */
    *--sp = 0;  /* rbp */
    *--sp = 0;  /* rbx */
    p->ctx.rsp = (uint64_t)sp;
}

process_t *scheduler_create(const char *name, void (*entry)(void)) {
    if (n_procs >= MAX_PROCESSES) PANIC("Too many processes");

    process_t *p = &proc_table[n_procs++];
    p->pid   = next_pid++;
    p->state = PROC_READY;
    p->stack = (uint8_t *)kmalloc(STACK_SIZE);
    if (!p->stack) PANIC("Cannot allocate process stack");

    p->stack_top = (uint64_t)(p->stack + STACK_SIZE);
    strncpy(p->name, name, PROC_NAME_LEN);
    setup_stack(p, entry);
    return p;
}

void scheduler_yield(void) {
    int old_idx = current_idx;
    /* Round-robin: find next READY process */
    int next = (current_idx + 1) % n_procs;
    while (next != old_idx) {
        if (proc_table[next].state == PROC_READY ||
            proc_table[next].state == PROC_RUNNING) break;
        next = (next + 1) % n_procs;
    }
    if (next == old_idx) return; /* nothing else to run */

    proc_table[old_idx].state = PROC_READY;
    proc_table[next].state    = PROC_RUNNING;
    current_idx = next;

    switch_to(&proc_table[old_idx].ctx.rsp,
               proc_table[next].ctx.rsp);
}

void scheduler_exit(void) {
    proc_table[current_idx].state = PROC_ZOMBIE;
    kprintf("[scheduler] process '%s' exited\n",
            proc_table[current_idx].name);
    /* Switch to next alive process */
    int next = (current_idx + 1) % n_procs;
    while (proc_table[next].state == PROC_ZOMBIE)
        next = (next + 1) % n_procs;
    int old = current_idx;
    current_idx = next;
    proc_table[next].state = PROC_RUNNING;
    switch_to(&proc_table[old].ctx.rsp, proc_table[next].ctx.rsp);
}

void scheduler_print_all(void) {
    static const char *state_names[] = {"READY", "RUNNING", "BLOCKED", "ZOMBIE"};
    kprintf("PID  STATE    NAME\n");
    kprintf("---  -------  ----\n");
    for (int i = 0; i < n_procs; i++) {
        process_t *p = &proc_table[i];
        kprintf("%-4u %-8s %s\n", p->pid, state_names[p->state], p->name);
    }
}
