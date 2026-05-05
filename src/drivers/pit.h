#pragma once
#include <stdint.h>

#define PIT_FREQ_HZ 100   /* 10ms time slice */

void     pit_init(void);
uint64_t pit_ticks(void);
