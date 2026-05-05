#include "gdt.h"

/* 3-entry GDT: null, 64-bit kernel code, 64-bit kernel data */
static gdt_entry_t gdt[3];
static gdt_ptr_t   gdt_ptr;

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[i].base_low  = base & 0xFFFF;
    gdt[i].base_mid  = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;
    gdt[i].limit_low = limit & 0xFFFF;
    gdt[i].granularity = (gran & 0xF0) | ((limit >> 16) & 0x0F);
    gdt[i].access    = access;
}

/* Reload segment registers after loading a new GDT */
extern void gdt_flush(uint64_t ptr);

void gdt_init(void) {
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    gdt_set_entry(0, 0, 0, 0, 0);            /* null */
    /* 64-bit code: P=1, DPL=0, S=1, type=0xA, L=1, G=1 */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xAF);
    /* 64-bit data: P=1, DPL=0, S=1, type=0x2, D/B=1, G=1 */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);

    gdt_flush((uint64_t)&gdt_ptr);
}
