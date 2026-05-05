#include "vmm.h"
#include "pmm.h"
#include "../lib/panic.h"
#include "../lib/string.h"

/* Read CR3 to get PML4 physical address */
static uint64_t *get_pml4(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return (uint64_t *)(cr3 & PAGE_MASK);
}

static uint64_t *get_or_alloc_table(uint64_t *parent, int idx, uint64_t flags) {
    if (!(parent[idx] & PTE_PRESENT)) {
        void *page = pmm_alloc();
        memset(page, 0, PAGE_SIZE);
        parent[idx] = (uint64_t)page | flags | PTE_PRESENT;
    }
    return (uint64_t *)(parent[idx] & PAGE_MASK);
}

void vmm_init(void) {
    /* The boot code already set up an identity map for the first 2MB.
       Nothing more to do until we start mapping new regions. */
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pml4 = get_pml4();
    int i4 = (virt >> 39) & 0x1FF;
    int i3 = (virt >> 30) & 0x1FF;
    int i2 = (virt >> 21) & 0x1FF;
    int i1 = (virt >> 12) & 0x1FF;

    uint64_t *pdpt = get_or_alloc_table(pml4, i4, PTE_WRITE);
    uint64_t *pd   = get_or_alloc_table(pdpt, i3, PTE_WRITE);
    uint64_t *pt   = get_or_alloc_table(pd,   i2, PTE_WRITE);

    pt[i1] = (phys & PAGE_MASK) | flags | PTE_PRESENT;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap(uint64_t virt) {
    uint64_t *pml4 = get_pml4();
    int i4 = (virt >> 39) & 0x1FF;
    int i3 = (virt >> 30) & 0x1FF;
    int i2 = (virt >> 21) & 0x1FF;
    int i1 = (virt >> 12) & 0x1FF;

    if (!(pml4[i4] & PTE_PRESENT)) return;
    uint64_t *pdpt = (uint64_t *)(pml4[i4] & PAGE_MASK);
    if (!(pdpt[i3] & PTE_PRESENT)) return;
    uint64_t *pd = (uint64_t *)(pdpt[i3] & PAGE_MASK);
    if (!(pd[i2] & PTE_PRESENT)) return;
    uint64_t *pt = (uint64_t *)(pd[i2] & PAGE_MASK);

    pt[i1] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t *pml4 = get_pml4();
    int i4 = (virt >> 39) & 0x1FF;
    int i3 = (virt >> 30) & 0x1FF;
    int i2 = (virt >> 21) & 0x1FF;
    int i1 = (virt >> 12) & 0x1FF;

    if (!(pml4[i4] & PTE_PRESENT)) return 0;
    uint64_t *pdpt = (uint64_t *)(pml4[i4] & PAGE_MASK);
    if (!(pdpt[i3] & PTE_PRESENT)) return 0;
    uint64_t *pd = (uint64_t *)(pdpt[i3] & PAGE_MASK);
    if (!(pd[i2] & PTE_PRESENT)) return 0;
    uint64_t *pt = (uint64_t *)(pd[i2] & PAGE_MASK);
    if (!(pt[i1] & PTE_PRESENT)) return 0;
    return (pt[i1] & PAGE_MASK) | (virt & (PAGE_SIZE - 1));
}
