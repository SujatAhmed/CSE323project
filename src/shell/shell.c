#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"
#include "../mm/pmm.h"
#include "../mm/kmalloc.h"
#include "../proc/scheduler.h"

#define INPUT_MAX 256

static void shell_readline(char *buf, int max) {
    int i = 0;
    for (;;) {
        char c = serial_getchar();
        if (c == '\n' || c == '\r') {
            buf[i] = '\0';
            serial_putchar('\n');
            return;
        }
        if (c == '\b' || c == 0x7F) {   /* 0x7F = DEL, sent by most terminals */
            if (i > 0) {
                i--;
                serial_putchar('\b');
                serial_putchar(' ');
                serial_putchar('\b');
            }
            continue;
        }
        if (i < max - 1) {
            buf[i++] = c;
            serial_putchar(c);
        }
    }
}

/* ─── Built-in commands ────────────────────────────────────────────────────── */

static void cmd_help(void) {
    kprintf("ElemOS shell commands:\n");
    kprintf("  help   - show this message\n");
    kprintf("  clear  - clear the screen\n");
    kprintf("  mem    - show memory statistics\n");
    kprintf("  ps     - list processes\n");
    kprintf("  yield  - voluntarily yield to next process\n");
    kprintf("  hello  - print a greeting\n");
}

static void cmd_clear(void) {
    vga_clear();
}

static void cmd_mem(void) {
    uint64_t total = pmm_total_frames() * 4;
    uint64_t free  = pmm_free_frames()  * 4;
    uint64_t used  = total - free;
    kprintf("Physical memory:\n");
    kprintf("  Total : %llu KB (%llu MB)\n", total, total / 1024);
    kprintf("  Used  : %llu KB\n", used);
    kprintf("  Free  : %llu KB\n", free);
    kprintf("Kernel heap:\n");
    kprintf("  In use: %llu bytes\n", kmalloc_used());
}

static void cmd_ps(void) {
    scheduler_print_all();
}

static void cmd_yield(void) {
    scheduler_yield();
}

static void cmd_hello(void) {
    kprintf("Hello from ElemOS!\n");
}

static void dispatch(const char *line) {
    if (!*line) return;
    if (strcmp(line, "help")  == 0) { cmd_help();  return; }
    if (strcmp(line, "clear") == 0) { cmd_clear(); return; }
    if (strcmp(line, "mem")   == 0) { cmd_mem();   return; }
    if (strcmp(line, "ps")    == 0) { cmd_ps();    return; }
    if (strcmp(line, "yield") == 0) { cmd_yield(); return; }
    if (strcmp(line, "hello") == 0) { cmd_hello(); return; }
    kprintf("Unknown command: '%s' (type 'help' for list)\n", line);
}

void shell_run(void) {
    char line[INPUT_MAX];
    kprintf("\nElemOS shell ready. Type 'help' for commands.\n\n");
    for (;;) {
        kprintf("elemos> ");
        shell_readline(line, INPUT_MAX);
        dispatch(line);
    }
}
