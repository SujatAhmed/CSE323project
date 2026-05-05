#include "irq.h"
#include "pic.h"
#include "../../lib/kprintf.h"

static irq_handler_t irq_handlers[16];

void irq_init(void) {
    for (int i = 0; i < 16; i++) irq_handlers[i] = 0;
}

void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
        pic_unmask_irq(irq);
    }
}

void irq_unregister(uint8_t irq) {
    if (irq < 16) {
        irq_handlers[irq] = 0;
        pic_mask_irq(irq);
    }
}

void irq_handler(registers_t *regs) {
    uint8_t irq = (uint8_t)(regs->int_no - 32);
    /* EOI before handler: lets the PIC deliver new interrupts even if the
       handler performs a context switch and never returns to this frame. */
    pic_send_eoi(irq);
    if (irq < 16 && irq_handlers[irq])
        irq_handlers[irq](regs);
}
