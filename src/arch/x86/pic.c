#include "pic.h"
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void io_wait(void) { outb(0x80, 0); }

void pic_init(void) {
    /* ICW1: init + expect ICW4 */
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();
    /* ICW2: vector offsets */
    outb(PIC1_DATA, PIC1_OFFSET); io_wait();
    outb(PIC2_DATA, PIC2_OFFSET); io_wait();
    /* ICW3: cascade */
    outb(PIC1_DATA, 0x04); io_wait(); /* PIC2 on IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait(); /* PIC2 is slave */
    /* ICW4: 8086 mode */
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();
    /* mask all IRQs; drivers unmask as needed */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = irq < 8 ? irq : irq - 8;
    outb(port, inb(port) | (1 << bit));
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = irq < 8 ? irq : irq - 8;
    outb(port, inb(port) & ~(1 << bit));
    if (irq >= 8) outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << 2)); /* unmask IRQ2 cascade */
}
