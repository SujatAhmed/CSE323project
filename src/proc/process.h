#pragma once
#include <stdint.h>

#define MAX_PROCESSES 16
#define STACK_SIZE    16384  /* 16KB per process */
#define PROC_NAME_LEN 32

typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_BLOCKED = 2,
    PROC_ZOMBIE  = 3
} proc_state_t;

typedef struct {
    uint64_t rsp;    /* saved stack pointer (only callee-saved regs below) */
} proc_context_t;

typedef struct process {
    uint32_t     pid;
    char         name[PROC_NAME_LEN];
    proc_state_t state;
    proc_context_t ctx;
    uint8_t      *stack;     /* bottom of kernel stack */
    uint64_t      stack_top; /* top of stack (initial RSP) */
} process_t;
