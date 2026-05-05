#include "serial.h"
#include <stdint.h>

static uint16_t g_port;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

void serial_init(uint16_t port) {
    g_port = port;
    outb(port + 1, 0x00); /* disable interrupts */
    outb(port + 3, 0x80); /* DLAB on */
    outb(port + 0, 0x03); /* divisor lo: 38400 baud */
    outb(port + 1, 0x00); /* divisor hi */
    outb(port + 3, 0x03); /* 8N1, DLAB off */
    outb(port + 2, 0xC7); /* enable+clear FIFO, 14-byte threshold */
    outb(port + 4, 0x0B); /* DTR+RTS+IRQ enabled */
}

static int serial_tx_empty(void) {
    return inb(g_port + 5) & 0x20;
}

void serial_putchar(char c) {
    while (!serial_tx_empty());
    outb(g_port, (uint8_t)c);
}

void serial_puts(const char *s) {
    while (*s) serial_putchar(*s++);
}

static int serial_rx_ready(void) {
    return inb(g_port + 5) & 0x01;
}

char serial_getchar(void) {
    while (!serial_rx_ready());
    return (char)inb(g_port);
}
