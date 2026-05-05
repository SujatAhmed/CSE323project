#include "pit.h"
#include "../arch/x86/irq.h"
#include "../proc/scheduler.h"
#include "../lib/kprintf.h"

#define PIT_CHANNEL0  0x40
#define PIT_CMD       0x43
#define PIT_BASE_HZ   1193182

static volatile uint64_t tick_count;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void timer_handler(registers_t *regs) {
    (void)regs;
    tick_count++;
    scheduler_yield();
}

void pit_init(void) {
    tick_count = 0;
    uint16_t divisor = (uint16_t)(PIT_BASE_HZ / PIT_FREQ_HZ);
    /* channel 0, lobyte/hibyte access, mode 3 (square wave), binary */
    outb(PIT_CMD,      0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)(divisor >> 8));
    irq_register(0, timer_handler);
    kprintf("[PIT] Timer at %u Hz (divisor %u)\n", PIT_FREQ_HZ, divisor);
}

uint64_t pit_ticks(void) { return tick_count; }
