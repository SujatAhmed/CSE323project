#pragma once
#include <stdint.h>
#include <stddef.h>

#define PTE_PRESENT  (1UL << 0)
#define PTE_WRITE    (1UL << 1)
#define PTE_USER     (1UL << 2)
#define PTE_HUGE     (1UL << 7)

#define PAGE_SIZE    4096UL
#define PAGE_MASK    (~(PAGE_SIZE - 1))

void  vmm_init(void);
void  vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void  vmm_unmap(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);
