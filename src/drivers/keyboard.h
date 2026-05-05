#pragma once
#include <stdint.h>

#define KB_BUFFER_SIZE 256

void keyboard_init(void);
char keyboard_getchar(void);  /* blocking read from ring buffer */
int  keyboard_available(void);
