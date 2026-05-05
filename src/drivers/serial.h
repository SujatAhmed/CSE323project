#pragma once
#include <stdint.h>

#define SERIAL_COM1 0x3F8

void serial_init(uint16_t port);
void serial_putchar(char c);
void serial_puts(const char *s);
char serial_getchar(void);
