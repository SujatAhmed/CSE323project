# ElemOS — Technical Report

**Course:** CSE323 — Operating Systems  
**Author:** Sujat Ahmed  
**Project:** ElemOS, a 64-bit hobby kernel

---

## 1. Project Overview

ElemOS is a 64-bit monolithic kernel written from scratch in C and x86-64 assembly. It boots via GRUB using the Multiboot2 protocol, runs under QEMU, and implements the core subsystems of a basic operating system: physical and virtual memory management, interrupt handling, preemptive multitasking, and an interactive shell.

**Codebase:** ~3,000 lines of C, ~400 lines of NASM assembly  
**Target:** x86-64, freestanding, no libc  
**Toolchain:** `x86_64-elf-gcc`, `nasm`, `grub-mkrescue`, `xorriso`, `qemu-system-x86_64`

Implemented subsystems, in initialization order:

1. Multiboot2 boot header and GRUB handoff
2. 32-bit to 64-bit long mode transition (page tables, GDT, EFER MSR)
3. Global Descriptor Table (GDT)
4. Interrupt Descriptor Table (IDT) + ISR stubs
5. 8259 PIC remapping
6. VGA and serial (`kprintf`)
7. Physical Memory Manager (PMM)
8. Virtual Memory Manager (VMM) / paging
9. Heap allocator (`kmalloc`/`kfree`)
10. PS/2 keyboard driver
11. PIT-driven preemptive scheduler
12. Interactive shell process

---

## 2. Boot Sequence

### 2.1 Multiboot2 and GRUB

The kernel image begins with a Multiboot2 header in a dedicated `.multiboot2` section, placed first in the linker script (`linker.ld`). GRUB requires the magic bytes within the first 8 KB of the image; any other ordering silently causes GRUB to skip the kernel without error.

GRUB loads the kernel in 32-bit protected mode and passes a pointer to the Multiboot2 info structure in `ebx`. The boot code validates the magic number (`0x36d76289`) and stores the info pointer for later use by the PMM (to read the memory map tag).

### 2.2 Long Mode Transition

The entry point (`boot.asm`) performs the transition from 32-bit protected mode to 64-bit long mode before calling `kernel_main`. Steps, in order:

1. Verify CPUID support by testing EFLAGS bit 21.
2. Check for long-mode capability via `CPUID EAX=0x80000001` (LM bit in EDX).
3. Build identity-mapped page tables covering the first 8 MB: PML4 → PDPT → PD → PT chain allocated in BSS.
4. Load `cr3` with the PML4 address.
5. Set PAE in `cr4`, set LME in `IA32_EFER` MSR, set PG in `cr0`.
6. Load a 64-bit GDT.
7. Far-jump to a 64-bit code segment, entering long mode.
8. Set up a 32 KB stack in BSS, then `call kernel_main`.

The kernel image links at physical address `0x100000` (1 MB, above BIOS/EBDA). The identity map covers 8 MB because the kernel image plus BSS (stack, page tables) exceeds 2 MB.

---

## 3. GDT and IDT

### 3.1 GDT

The GDT contains four descriptors: null, 64-bit code, 64-bit data, and a TSS entry. The TSS is used only to define a valid RSP0 for future privilege-level transitions; no userspace exists yet.

The GDT struct is declared `__attribute__((aligned(8)))`. The 10-byte `GDTR` passed to `lgdt` must also be aligned; misalignment causes a silent triple-fault on some CPU implementations.

### 3.2 IDT and ISR Stubs

`isr_stubs.asm` defines 256 ISR entry points using a NASM macro. Each stub pushes a vector number (and a dummy error code for vectors that don't push one) and jumps to a common C handler, `isr_handler`. The handler dispatches to registered function pointers stored in a 256-entry table.

All ISRs save and restore the full register set (`rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`, `rbp`, `r8`–`r15`) on the kernel stack before entering C.

### 3.3 PIC Remap

The 8259A PIC defaults to mapping IRQ0–IRQ7 to interrupt vectors 0x08–0x0F. In 64-bit mode, vectors 0x08 and 0x0D are the double-fault and general-protection-fault handlers. To prevent hardware IRQs from triggering CPU exception handlers, the PIC is remapped to vectors 0x20–0x2F before `sti` is executed.

---

## 4. Physical Memory Manager

The PMM uses a **bitmap allocator**: one bit per 4 KB physical page. For 256 MB of RAM, the bitmap is 8 KB.

**Initialization:**  
The PMM walks the Multiboot2 memory map tag. It marks all pages in `MULTIBOOT_MEMORY_AVAILABLE` regions as free, then marks pages occupied by the kernel image (`_kernel_start`–`_kernel_end`, from the linker script) as used, then marks the bitmap's own pages as used.

**Allocation:** `pmm_alloc()` finds the first free bit, sets it, and returns the physical address (`bit_index * 4096`).

**Deallocation:** `pmm_free()` clears the bit. No coalescing — physical pages are fixed-size.

---

## 5. Virtual Memory Manager

The VMM operates on top of the identity-mapped 8 MB region established at boot. Its two responsibilities are:

1. **Page table walk:** translate a virtual address to a physical address by traversing PML4 → PDPT → PD → PT.
2. **Map pages:** install a virtual→physical mapping by allocating intermediate tables from the PMM as needed and writing page table entries with the appropriate flags (present, writable, etc.).

The kernel runs entirely within the identity-mapped region. The VMM's map function is used only by `kmalloc` when expanding the heap past its initial committed range.

Page faults are handled by a registered ISR that calls `kpanic()`. There is no demand paging or page-fault recovery; any fault is a kernel bug.

---

## 6. Heap Allocator (`kmalloc` / `kfree`)

The heap allocator uses a **free-list of variable-size blocks**. Each block has a 16-byte header (size, flags, forward and backward pointers), followed by the user payload.

**Allocation:** Walk the free list for a block of sufficient size. If the remaining space after allocation exceeds the minimum block size, split. Return a pointer to the payload.

**Deallocation:** Mark the block free and coalesce with adjacent free blocks.

**Integrity check:** `kmalloc_verify()` walks the entire heap validating header consistency. Called in test scenarios only; not active at runtime.

The heap starts at a fixed virtual address and grows via `vmm_map()` + `pmm_alloc()` in 4 KB increments.

---

## 7. Interrupt Handling and Drivers

### 7.1 PIT (Preemptive Timer)

The PIT (8253/8254) is configured in mode 3 (square wave) with a divisor that produces approximately 100 Hz. IRQ0 fires the timer ISR, which increments a tick counter and calls `scheduler_yield()`.

### 7.2 PS/2 Keyboard

The keyboard driver handles IRQ1 (PS/2 port). The ISR reads one scancode byte from port `0x60`, decodes it against a scancode set 1 lookup table, tracks modifier state (Shift, Caps Lock), and enqueues the resulting ASCII character in a small ring buffer. `keyboard_getchar()` blocks by spinning on the buffer.

EOI is sent to the PIC **after** reading the scancode. Sending EOI before reading leaves the byte in the keyboard's output buffer, which re-asserts IRQ1 on the next PIC cycle, producing an interrupt storm.

---

## 8. Scheduler and Context Switching

The scheduler is a **round-robin** over a fixed-size process table. Each process has a PCB (`struct process`) holding PID, state (READY/RUNNING/BLOCKED), a kernel stack, and a saved RSP.

### 8.1 Context Switch

`switch_to(uint64_t *old_rsp_ptr, uint64_t new_rsp)` in `switch.asm`:

1. Push all callee-saved registers (`rbp`, `rbx`, `r12`–`r15`) on the outgoing process's stack.
2. Write current RSP into `*old_rsp_ptr`.
3. Load `new_rsp` into RSP.
4. Pop the incoming process's callee-saved registers.
5. `ret` — transfers control to wherever the incoming process last called `switch_to` (or to the entry function for a new process).

### 8.2 New Process Stack Initialization

A process that has never run has no saved context. Its stack is fabricated to look like it was switched away from: six zeros (for the six callee-saved registers) followed by the process's entry function address. The first `switch_to` call pops the zeros, hits `ret`, and enters the entry function.

### 8.3 Interrupts and Scheduling

`switch_to` re-enables interrupts (`sti`) immediately before `ret`. When called from the timer ISR (IF=0), the incoming process would otherwise run with interrupts permanently disabled, causing `hlt` in the idle loop to freeze.

---

## 9. Shell

The shell runs as an ordinary kernel process. It loops: read a line from the keyboard buffer, tokenize on whitespace, dispatch on the first token.

| Command | Action |
|---------|--------|
| `help`  | Print available commands |
| `clear` | Clear the VGA screen |
| `mem`   | Print PMM statistics (free/used pages) |
| `ps`    | Print process table (PID, state, name) |
| `yield` | Explicitly yield CPU to scheduler |
| `hello` | Print a test string |

---

## 10. Problems Encountered and Solutions

### 10.1 Toolchain Setup

**Problem:** Building a cross-compiler (`x86_64-elf-gcc`) from source via the OSDev wiki instructions failed during the libgcc build step. A subsequent attempt using the AUR package produced incompatible object files when mixed with artifacts from the failed build.

**Solution:** Cleared all build artifacts and installed the full toolchain from Arch Linux repos: `x86_64-elf-gcc`, `nasm`, `binutils`, `grub`, `libisoburn`, `xorriso`, `qemu`, `gdb`. Distro-packaged cross-compilers are sufficient for a freestanding kernel target; the README documents the exact package list so this does not need to be repeated.

### 10.2 Multiboot2 Header Placement

**Problem:** GRUB silently refused to load the kernel. No error message — it fell through to "no bootable kernel". Root cause: the Multiboot1 header was placed after `.text` in the linker script, pushing it past the 8 KB cutoff GRUB scans.

**Solution:** Switched to Multiboot2 and placed `.multiboot2` as the first section in `linker.ld`. Added an immediate VGA write (`0xB8000`) as the first instruction in the kernel to confirm GRUB handoff.

### 10.3 Identity Map Too Small

**Problem:** Triple-fault on the first memory access past 0x200000 after entering long mode. The initial identity map covered only 2 MB; the kernel image plus BSS (stack and page tables) exceeded that boundary. With no IDT installed, the page fault escalated to a triple-fault with no diagnostic output.

**Solution:** Extended the identity map to 8 MB by adding extra PT entries in `boot.asm`.

### 10.4 GDT Alignment Triple-Fault

**Problem:** `lgdt` succeeded but the CPU triple-faulted on the next instruction. No register corruption visible prior.

**Solution:** Adding `__attribute__((aligned(8)))` to the GDT descriptor struct fixed it. The Intel SDM recommends GDT base alignment; in practice the CPU is strict about the alignment of the 10-byte `GDTR` operand itself when loading from memory.

### 10.5 `sti` Before PIC Remap

**Problem:** Kernel triple-faulted immediately after enabling interrupts. The PIC default IRQ mapping (IRQ0 → vector 0x08) overlaps the double-fault handler vector, so the first timer tick triggered a spurious double-fault before the IDT was populated.

**Solution:** Call `pic_remap()` before `sti`. The bug was that `sti` appeared before `pic_remap()` in `kernel_main` initialization order. Reordering the two calls resolved it.

### 10.6 PMM Allocating Its Own Bitmap

**Problem:** The first `pmm_alloc()` call returned a page inside the bitmap itself. Subsequent allocations corrupted bookkeeping and eventually returned duplicate pointers.

**Solution:** After marking kernel pages as used, explicitly mark the bitmap's own pages as used before opening the allocator to callers.

### 10.7 `kmalloc` Header Size Mismatch

**Problem:** `kmalloc(32)` returned only 8 usable bytes. The allocator computed `header + 1` as the user pointer, but `sizeof(header)` was 24 bytes due to implicit struct padding, not the intended 16.

**Solution:** Declared the header struct `__attribute__((packed))` with explicit fixed-width fields to guarantee a 16-byte layout.

### 10.8 Heap Coalescing Off-By-One

**Problem:** The heap slowly corrupted over many alloc/free cycles. `kfree` read the "free" flag of the next block from the wrong offset, occasionally coalescing into a block that was still in use.

**Solution:** Fixed the offset arithmetic in the coalescing code. Added `kmalloc_verify()` — a full heap walk that validates header consistency — to detect this class of error in testing.

### 10.9 New Process Stack Register Order

**Problem:** A newly created process triple-faulted on its first execution. The fabricated stack had the six callee-saved registers pushed in the wrong order; `switch_to` popped them in reverse push order, loading garbage into the registers and jumping to address 0x0.

**Solution:** Matched the push order in the new-process stack setup to the pop order in `switch_to`. The correct order (push then pop in reverse) is documented with a comment in `switch.asm`.

### 10.10 Keyboard EOI Ordering

**Problem:** After the first keypress, the kernel entered an interrupt storm servicing IRQ1 continuously with no other work being done.

**Solution:** Moved the EOI write to the PIC to after the scancode read from port `0x60`. Sending EOI before reading the scancode left the byte in the keyboard output buffer, which re-asserted the IRQ signal on the next cycle.
