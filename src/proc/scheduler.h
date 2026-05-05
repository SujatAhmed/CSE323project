#pragma once
#include "process.h"

void scheduler_init(void);
process_t *scheduler_create(const char *name, void (*entry)(void));
void       scheduler_yield(void);
void       scheduler_exit(void);
process_t *scheduler_current(void);
void       scheduler_print_all(void);
