#pragma once
#include "idt.h"

typedef void (*isr_handler_t)(registers_t *regs);

void isr_init(void);
void isr_register(uint8_t n, isr_handler_t handler);
void isr_handler(registers_t *regs);
