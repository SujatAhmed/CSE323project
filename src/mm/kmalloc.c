#include "kmalloc.h"
#include "pmm.h"
#include "vmm.h"
#include "../lib/string.h"
#include "../lib/panic.h"

/*
 * Simple explicit free-list allocator.
 * Heap lives at HEAP_START and grows in 4KB increments from the PMM.
 */
#define HEAP_START  0x400000UL  /* 4MB virtual (above kernel) */
#define HEAP_MAX    0x1000000UL /* 16MB heap cap */
#define ALIGN8(x)   (((x) + 7) & ~7UL)

typedef struct block_hdr {
    size_t size;         /* payload size (not including header) */
    int    free;
    struct block_hdr *next;
} block_hdr_t;

#define HDR_SIZE ALIGN8(sizeof(block_hdr_t))

static block_hdr_t *heap_head;
static uint64_t heap_brk;  /* current end of committed heap */
static uint64_t used_bytes;

static void expand_heap(size_t needed) {
    while (heap_brk - HEAP_START < needed) {
        void *frame = pmm_alloc();
        vmm_map(heap_brk, (uint64_t)frame, PTE_PRESENT | PTE_WRITE);
        heap_brk += PAGE_SIZE;
        if (heap_brk > HEAP_START + HEAP_MAX) PANIC("Heap exhausted");
    }
}

void kmalloc_init(void) {
    heap_brk = HEAP_START;
    /* Commit initial page */
    expand_heap(PAGE_SIZE);
    heap_head = (block_hdr_t *)HEAP_START;
    heap_head->size = PAGE_SIZE - HDR_SIZE;
    heap_head->free = 1;
    heap_head->next = 0;
    used_bytes = 0;
}

void *kmalloc(size_t size) {
    if (!size) return 0;
    size = ALIGN8(size);

    block_hdr_t *b = heap_head;
    while (b) {
        if (b->free && b->size >= size) {
            /* Split if there's room for another block */
            if (b->size >= size + HDR_SIZE + 8) {
                block_hdr_t *nb = (block_hdr_t *)((uint8_t *)b + HDR_SIZE + size);
                nb->size = b->size - size - HDR_SIZE;
                nb->free = 1;
                nb->next = b->next;
                b->next  = nb;
                b->size  = size;
            }
            b->free = 0;
            used_bytes += b->size;
            return (uint8_t *)b + HDR_SIZE;
        }
        if (!b->next) {
            /* Expand heap */
            size_t grow = size + HDR_SIZE;
            grow = (grow + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            expand_heap((heap_brk - HEAP_START) + grow);
            block_hdr_t *nb = (block_hdr_t *)((uint8_t *)b + HDR_SIZE + b->size);
            nb->size = grow - HDR_SIZE;
            nb->free = 1;
            nb->next = 0;
            b->next  = nb;
        }
        b = b->next;
    }
    PANIC("kmalloc: no memory");
    return 0;
}

void *kzalloc(size_t size) {
    void *p = kmalloc(size);
    if (p) memset(p, 0, size);
    return p;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_hdr_t *b = (block_hdr_t *)((uint8_t *)ptr - HDR_SIZE);
    used_bytes -= b->size;
    b->free = 1;
    /* Coalesce adjacent free blocks */
    block_hdr_t *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += HDR_SIZE + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

uint64_t kmalloc_used(void) { return used_bytes; }
