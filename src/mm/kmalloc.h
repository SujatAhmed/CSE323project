#pragma once
#include <stddef.h>
#include <stdint.h>

void  kmalloc_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void  kfree(void *ptr);
uint64_t kmalloc_used(void);
