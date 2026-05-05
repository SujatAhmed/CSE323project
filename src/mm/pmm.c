#include "pmm.h"
#include "../lib/kprintf.h"
#include "../lib/panic.h"
#include "../lib/string.h"

/* Bitmap: 1 bit per 4KB page, 0=free 1=used */
#define MAX_FRAMES (256 * 1024)   /* support up to 1GB RAM */
static uint8_t bitmap[MAX_FRAMES / 8];
static uint64_t total_frames;
static uint64_t free_count;

static void bitmap_set(uint64_t frame) {
    bitmap[frame / 8] |= (1 << (frame % 8));
}
static void bitmap_clear(uint64_t frame) {
    bitmap[frame / 8] &= ~(1 << (frame % 8));
}
static int bitmap_test(uint64_t frame) {
    return (bitmap[frame / 8] >> (frame % 8)) & 1;
}

/* Multiboot2 tag header */
typedef struct { uint32_t type; uint32_t size; } mb2_tag_t;

void pmm_init(uint64_t mb2_info) {
    memset(bitmap, 0xFF, sizeof(bitmap)); /* all used initially */
    total_frames = 0;
    free_count   = 0;

    /* Multiboot2 info starts with a u32 total_size, u32 reserved */
    uint64_t offset = mb2_info + 8;
    uint64_t end    = mb2_info + *(uint32_t *)mb2_info;

    while (offset < end) {
        mb2_tag_t *tag = (mb2_tag_t *)offset;
        if (tag->type == 0) break;  /* end tag */

        if (tag->type == 6) {  /* memory map */
            mb2_mmap_tag_t *mmap = (mb2_mmap_tag_t *)tag;
            uint64_t n_entries = (tag->size - 16) / mmap->entry_size;
            for (uint64_t i = 0; i < n_entries; i++) {
                mb2_mmap_entry_t *e = (mb2_mmap_entry_t *)
                    ((uint8_t *)mmap->entries + i * mmap->entry_size);
                if (e->type != MB2_MMAP_AVAILABLE) continue;

                uint64_t start_frame = (e->addr + PAGE_SIZE - 1) / PAGE_SIZE;
                uint64_t end_frame   = (e->addr + e->len) / PAGE_SIZE;
                for (uint64_t f = start_frame; f < end_frame && f < MAX_FRAMES; f++) {
                    bitmap_clear(f);
                    total_frames++;
                    free_count++;
                }
            }
        }
        offset += (tag->size + 7) & ~7UL;
    }

    /* Mark frame 0 as reserved (null pointer protection) */
    if (!bitmap_test(0)) { bitmap_set(0); free_count--; }

    /* Mark kernel frames as used (loaded at 1MB = frame 256, ~512KB) */
    extern uint8_t _kernel_start[], _kernel_end[];
    uint64_t ks = (uint64_t)_kernel_start / PAGE_SIZE;
    uint64_t ke = ((uint64_t)_kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t f = ks; f < ke && f < MAX_FRAMES; f++) {
        if (!bitmap_test(f)) { bitmap_set(f); free_count--; }
    }

    kprintf("[PMM] %llu MB total, %llu MB free\n",
            total_frames * PAGE_SIZE / (1024*1024),
            free_count   * PAGE_SIZE / (1024*1024));
}

void *pmm_alloc(void) {
    for (uint64_t i = 1; i < MAX_FRAMES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_count--;
            return (void *)(i * PAGE_SIZE);
        }
    }
    PANIC("Out of physical memory");
    return 0;
}

void pmm_free(void *frame) {
    uint64_t f = (uint64_t)frame / PAGE_SIZE;
    if (f >= MAX_FRAMES || !bitmap_test(f)) return;
    bitmap_clear(f);
    free_count++;
}

uint64_t pmm_free_frames(void)  { return free_count; }
uint64_t pmm_total_frames(void) { return total_frames; }
