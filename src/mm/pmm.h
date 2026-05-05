#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096UL

/* Multiboot2 memory-map entry (type field values) */
#define MB2_MMAP_AVAILABLE  1
#define MB2_MMAP_RESERVED   2
#define MB2_MMAP_ACPI       3
#define MB2_MMAP_NVS        4
#define MB2_MMAP_BADRAM     5

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    mb2_mmap_entry_t entries[];
} __attribute__((packed)) mb2_mmap_tag_t;

void     pmm_init(uint64_t mb2_info);
void    *pmm_alloc(void);        /* allocate one 4KB frame -> physical addr */
void     pmm_free(void *frame);
uint64_t pmm_free_frames(void);
uint64_t pmm_total_frames(void);
