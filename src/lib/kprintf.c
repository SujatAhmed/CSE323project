#include "kprintf.h"
#include "string.h"
/* Forward declaration — vga and serial are the two sinks */
extern void vga_putchar(char c);
extern void serial_putchar(char c);

static void kputchar(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

static void kputs(const char *s) {
    while (*s) kputchar(*s++);
}

static void kpad(const char *s, int width, char pad) {
    int len = (int)strlen(s);
    for (int i = len; i < width; i++) kputchar(pad);
    kputs(s);
}

void kvprintf(const char *fmt, va_list args) {
    char buf[32];
    for (; *fmt; fmt++) {
        if (*fmt != '%') { kputchar(*fmt); continue; }
        fmt++;
        int width = 0;
        char pad = ' ';
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt++ - '0'); }

        int is_long = 0;
        if (*fmt == 'l') { is_long = 1; fmt++; }
        if (*fmt == 'l') { fmt++; } /* ll */

        switch (*fmt) {
        case 'd': {
            int64_t v = is_long ? va_arg(args, int64_t) : (int64_t)va_arg(args, int);
            itoa(v, buf, 10);
            kpad(buf, width, pad);
            break;
        }
        case 'u': {
            uint64_t v = is_long ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
            utoa(v, buf, 10);
            kpad(buf, width, pad);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v = is_long ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
            utoa(v, buf, 16);
            kpad(buf, width, pad);
            break;
        }
        case 'p': {
            uint64_t v = (uint64_t)(uintptr_t)va_arg(args, void *);
            kputs("0x");
            utoa(v, buf, 16);
            kputs(buf);
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            kputs(s);
            break;
        }
        case 'c':
            kputchar((char)va_arg(args, int));
            break;
        case '%':
            kputchar('%');
            break;
        default:
            kputchar('%');
            kputchar(*fmt);
            break;
        }
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}
