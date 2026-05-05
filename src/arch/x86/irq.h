#pragma once
#include "idt.h"

typedef void (*irq_handler_t)(registers_t *regs);

void irq_init(void);
void irq_register(uint8_t irq, irq_handler_t handler);
void irq_unregister(uint8_t irq);
void irq_handler(registers_t *regs);
