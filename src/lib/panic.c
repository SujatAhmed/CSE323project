#include "panic.h"
#include "kprintf.h"

void panic_at(const char *msg, const char *file, int line) {
    kprintf("\n\n*** KERNEL PANIC ***\n");
    kprintf("  %s\n", msg);
    kprintf("  at %s:%d\n", file, line);
    kprintf("System halted.\n");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}
