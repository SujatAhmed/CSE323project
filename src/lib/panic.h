#pragma once

#define PANIC(msg) panic_at(msg, __FILE__, __LINE__)

void panic_at(const char *msg, const char *file, int line);
