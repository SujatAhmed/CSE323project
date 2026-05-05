#include "../arch/x86/gdt.h"
#include "../arch/x86/idt.h"
#include "../arch/x86/isr.h"
#include "../arch/x86/irq.h"
#include "../arch/x86/pic.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../drivers/keyboard.h"
#include "../drivers/pit.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/kmalloc.h"
#include "../proc/scheduler.h"
#include "../shell/shell.h"
#include "../lib/kprintf.h"

/* Linker-defined symbols marking kernel image boundaries */
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

/* Demo process that runs alongside the shell */
static void demo_process(void) {
    for (int i = 0; i < 3; i++) {
        kprintf("[demo] iteration %d — yielding\n", i + 1);
        scheduler_yield();
    }
    kprintf("[demo] process finished\n");
}

void kernel_main(uint32_t mb2_magic, uint32_t mb2_info_phys) {
    /* 1. Serial first so we have debug output even before VGA */
    serial_init(SERIAL_COM1);

    /* 2. VGA text console */
    vga_init();
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("  ______ _           ____  ____\n");
    kprintf(" |  ____| |         / __ \\/ ___|\n");
    kprintf(" | |__  | | ___ _ | |  | \\___ \\\n");
    kprintf(" |  __| | |/ _ \\ '_| |  | |___) |\n");
    kprintf(" | |____| |  __/ |  | |__| /___/ /\n");
    kprintf(" |______|_|\\___|_|   \\____/_____/\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("\n  ElemOS — An Elementary Kernel (CSE323)\n");
    kprintf("  ----------------------------------------\n\n");

    /* 3. GDT */
    kprintf("[boot] Initializing GDT...\n");
    gdt_init();

    /* 4. IDT + PIC */
    kprintf("[boot] Initializing IDT & PIC...\n");
    pic_init();
    isr_init();
    irq_init();
    idt_init();

    /* 5. Physical memory manager */
    kprintf("[boot] Initializing PMM (mb2_info=0x%x)...\n", mb2_info_phys);
    if (mb2_magic != 0x36D76289)
        kprintf("[boot] WARNING: not booted via Multiboot2!\n");
    pmm_init((uint64_t)mb2_info_phys);

    /* 6. Virtual memory manager */
    kprintf("[boot] Initializing VMM...\n");
    vmm_init();

    /* 7. Kernel heap */
    kprintf("[boot] Initializing kmalloc heap...\n");
    kmalloc_init();

    /* 8. Keyboard (enables IRQ1) */
    kprintf("[boot] Initializing keyboard driver...\n");
    keyboard_init();

    /* 9. Scheduler */
    kprintf("[boot] Initializing scheduler...\n");
    scheduler_init();

    /* 10. PIT — must come after scheduler_init */
    kprintf("[boot] Initializing PIT...\n");
    pit_init();

    /* 12. Spawn demo process and a shell process */
    scheduler_create("demo",  demo_process);
    scheduler_create("shell", shell_run);

    /* Enable interrupts — PIT will drive preemption from here */
    kprintf("[boot] All systems GO — enabling interrupts\n\n");
    __asm__ volatile("sti");

    /* Boot thread becomes the idle loop; yield to shell immediately */
    scheduler_yield();

    /* Idle loop */
    for (;;) {
        __asm__ volatile("sti; hlt");
        scheduler_yield();
    }
}
