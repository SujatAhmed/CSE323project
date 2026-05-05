#include "idt.h"
#include "isr.h"
#include "irq.h"

static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

/* Symbols from isr_stubs.asm */
extern void *isr_stub_table[];
extern void *irq_stub_table[];

void idt_set_gate(uint8_t n, uint64_t handler, uint8_t type_attr) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = 0x08; /* kernel code segment */
    idt[n].ist         = 0;
    idt[n].type_attr   = type_attr;
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

void idt_init(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    /* Exception stubs 0-31 */
    for (int i = 0; i < 32; i++)
        idt_set_gate(i, (uint64_t)isr_stub_table[i], 0x8E);

    /* IRQ stubs 32-47 */
    for (int i = 0; i < 16; i++)
        idt_set_gate(32 + i, (uint64_t)irq_stub_table[i], 0x8E);

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}
