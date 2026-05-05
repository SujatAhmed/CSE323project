#include "isr.h"
#include "../../lib/kprintf.h"
#include "../../lib/panic.h"

static isr_handler_t isr_handlers[32];

void isr_init(void) {
    for (int i = 0; i < 32; i++) isr_handlers[i] = 0;
}

static const char *exception_names[] = {
    "Division By Zero",        "Debug",                   "NMI",
    "Breakpoint",              "Into Overflow",            "Out of Bounds",
    "Invalid Opcode",          "No Coprocessor",          "Double Fault",
    "Coprocessor Overrun",     "Bad TSS",                 "Segment Not Present",
    "Stack Fault",             "General Protection Fault", "Page Fault",
    "Unknown Interrupt",       "x87 FPU Fault",           "Alignment Check",
    "Machine Check",           "SIMD FPU Fault",          "Virtualization Fault",
    "Control Protection",      "Reserved",                "Reserved",
    "Reserved",                "Reserved",                "Reserved",
    "Hypervisor Injection",    "VMM Communication",       "Security Exception",
    "Reserved",                "Triple Fault"
};

void isr_register(uint8_t n, isr_handler_t handler) {
    if (n < 32) isr_handlers[n] = handler;
}

void isr_handler(registers_t *regs) {
    uint64_t n = regs->int_no;
    if (n < 32 && isr_handlers[n]) {
        isr_handlers[n](regs);
        return;
    }
    kprintf("\n*** EXCEPTION #%llu: %s ***\n", n,
            n < 32 ? exception_names[n] : "Unknown");
    kprintf("  err_code=0x%llx  rip=0x%llx  cs=0x%llx\n",
            regs->err_code, regs->rip, regs->cs);
    kprintf("  rsp=0x%llx  rflags=0x%llx\n", regs->rsp, regs->rflags);
    kprintf("  rax=0x%llx  rbx=0x%llx  rcx=0x%llx  rdx=0x%llx\n",
            regs->rax, regs->rbx, regs->rcx, regs->rdx);
    PANIC("Unhandled CPU exception");
}
