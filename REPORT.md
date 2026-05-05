# ElemOS — A Student's Development Report

**Course:** CSE323 — Operating Systems
**Author:** Sujat Ahmed
**Project:** ElemOS, a 64-bit hobby kernel

---

## 1. How I picked the project

When the OS course project was announced, most of my classmates went straight for "safe" picks — a userspace shell, a thread library on top of pthreads, a small file-system simulator. I looked at those and felt I'd be writing application code that *talked about* operating systems rather than actually being one. The lectures had just covered protected mode, paging, and interrupts, and I wanted to feel those things in my hands instead of just on slides.

So I committed to writing a real kernel. From scratch. Booted by GRUB, running on bare metal (well, QEMU pretending to be bare metal), with its own memory manager, scheduler, and shell. In hindsight this was a slightly reckless decision for a one-semester project, but I don't regret it.

I named it **ElemOS** — short for "elementary OS" — because every other name I tried sounded too grand for what it actually does.

---

## 2. The toolchain wall (week 1)

Before I wrote a single line of kernel code, I lost three full days to the toolchain.

I'm on Arch Linux. The OSDev wiki tells you, in many places, to build a GCC cross-compiler from source — `i686-elf-gcc` or `x86_64-elf-gcc`. I dutifully followed the instructions, watched binutils compile for forty minutes, and then watched GCC fail at the libgcc stage with an error I couldn't decode. Tried again with different flags. Failed differently. Tried the AUR package `x86_64-elf-gcc`. That actually built — but my linker was rejecting the object files with "incompatible target" because I had been mixing artifacts from the two attempts.

What finally worked was nuking everything and starting clean with:

```
sudo pacman -S x86_64-elf-gcc nasm binutils grub libisoburn xorriso qemu gdb
```

The lesson I wrote down in my notes: **don't compile your toolchain unless you have to.** Distro packages exist for a reason. I added the alternate `CC` note in the README so the next person doesn't lose a weekend like I did.

This was also when I learned that Ubuntu's cross-compiler is called `x86_64-linux-gnu-gcc`, which is *not* a freestanding compiler — it links against glibc by default and you have to fight it with `-ffreestanding -nostdlib`. The Makefile I eventually wrote pretends both names are interchangeable, which is technically a lie, but it works because we override every default with explicit flags anyway.

---

## 3. Getting GRUB to load *anything* (week 2)

The first milestone was: a black QEMU window prints the letter "H" and halts. That's it. That was the entire goal for week 2.

I wrote a Multiboot1 header, hex-edited it once because I miscounted the checksum, and got `grub-mkrescue` to produce an ISO. QEMU loaded it and… nothing. Black screen. No output. I assumed my code was wrong.

It wasn't. The header was at the wrong offset. Multiboot1 requires the magic number within the **first 8 KB** of the ELF, and my linker script had put `.text` first with `.multiboot` somewhere after, pushing the header past the cutoff. GRUB silently refused to load it. There's no error message — it just falls through to "no multiboot kernel found".

I learned three things from this:

1. The order of sections in a linker script matters enormously for early boot.
2. Switch to **Multiboot2**. It's better-specified, gives you a memory map for free (which I'd need later for the PMM), and the header is more flexible.
3. When something is silent, instrument the next thing down. I started writing to the VGA buffer (`0xB8000`) in my very first instruction so I could *see* that I'd at least made it into my own code.

After switching to Multiboot2 and reordering the linker script so `.multiboot2` was first, the "H" appeared. I literally took a photo of my screen.

---

## 4. The long jump to long mode (weeks 2–3)

Multiboot2 hands you control in **32-bit protected mode**. To run a 64-bit kernel you have to:

1. Verify CPUID is supported (push/pop EFLAGS bit 21).
2. Verify the long-mode bit in CPUID extended functions.
3. Build identity-mapped page tables in BSS — at least PML4, PDPT, PD.
4. Load `cr3`, set the PAE bit in `cr4`, set the LME bit in the EFER MSR, set PG in `cr0`.
5. Load a 64-bit GDT.
6. `jmp` to a 64-bit code segment.

Any one of those steps wrong and the CPU triple-faults silently. QEMU just resets. With `-no-reboot -no-shutdown` you at least get to *see* the hang, but you don't get a stack trace. You get nothing.

The hardest one was step 3. I had identity-mapped the first 2 MB and assumed that was enough. It wasn't — my kernel image, loaded by GRUB at 1 MB, was about 800 KB compiled, but the BSS (page tables + 32 KB stack) pushed the working set over 2 MB. The first time the CPU tried to access something past 0x200000 in long mode, it page-faulted, but since the IDT wasn't installed yet, it triple-faulted. I extended the identity map to 8 MB and the boot got past it.

I also have a separate "high map" of the same 8 MB at a different virtual address, set up speculatively for when I'd later move the kernel to the higher half. I never actually finished that — the kernel still runs identity-mapped — but the page tables are there in `boot.asm`, ready, judging me.

The boot assembly file (`src/boot/boot.asm`) is the file I rewrote the most times. Probably eight or nine drafts.

---

## 5. The first triple-fault marathon (week 3–4)

Once I was in 64-bit mode and `kernel_main` was called, I thought the hard part was over. It was not.

Every new subsystem — GDT, IDT, PIC remap — is one line of code that's ten percent likely to triple-fault until you get it exactly right. Two memorable disasters:

**The GDT alignment bug.** My GDT was a `static struct` in C. When I loaded it with `lgdt`, the CPU triple-faulted on the very next instruction. After half a day of staring at hex, I added `__attribute__((aligned(8)))` to the descriptor struct. Fixed. The Intel SDM does say the GDT base should be aligned, but it's a "should" not a "must" — except apparently for the descriptor *itself*, the 10-byte limit/base structure passed to `lgdt`, where alignment matters because of how the CPU reads it. I'm still not 100% sure why this fixed it; I just know it did.

**The PIC remap that wasn't.** The 8259 PIC defaults to mapping IRQs 0–15 onto interrupt vectors 0x08–0x0F and 0x70–0x77. Vector 0x08 is the **double-fault vector** in long mode. So before my IDT was even installed, the timer IRQ was firing, hitting vector 0x08, the CPU treated it as a double-fault with no handler, and triple-faulted. The fix is to remap the PIC to vectors 0x20–0x2F before enabling interrupts. I knew this from the lectures. I had even written the remap code. The bug was that I'd put `sti` *before* `pic_remap()` in `kernel_main`, because I was tired and it looked nicer that way.

Reordering kernel initialization is the kind of bug where the fix is one line and the time-to-fix is six hours. I have a healthy fear of `sti` now.

---

## 6. Debugging without a debugger

For the first month I had no working GDB. I tried to attach early but my QEMU GDB stub kept disconnecting and I couldn't be bothered to fix it while I had so many other fires.

So I debugged the way kernel hackers in the 90s did: **`kprintf` everywhere.** Every function got a "I am here" line. Every function exit got "I left here". When the kernel hung, the last thing printed told me the approximate location of the crash.

Two rules I learned the hard way:

1. **Always flush serial output before doing anything risky.** The VGA buffer is memory-mapped and writes are immediate, but QEMU's serial port is buffered. If you write "about to switch CR3" to serial, then switch CR3, then crash — the message may never reach the terminal because the buffer flush gets eaten. I added a tight loop polling the serial line-status register after every important `kprintf`. Slow, but it never lied to me.

2. **Mirror to both VGA and serial.** I made `kprintf` write to both. Sometimes the VGA buffer would get scrolled away during a fast crash; the serial log on stdout was the source of truth. The `make run` target wires `-serial stdio` and I came to rely on it more than the QEMU window.

Eventually I did fix GDB (`make debug`), and it's wonderful — `info registers`, `x/10i $rip`, single-stepping through `switch_to` — but by then I'd already shipped most of the kernel. GDB became the tool for the *hard* bugs (context switching, page table walks); `kprintf` was still the tool for everything else.

---

## 7. The Physical Memory Manager (week 5)

The PMM was the first part where I felt like I was writing operating-system code rather than fighting the hardware. The design: a **bitmap**, one bit per 4 KB physical page. With 256 MB of RAM, that's 65,536 pages, or an 8 KB bitmap. Trivial.

The Multiboot2 memory map tag gives you a list of regions (`MULTIBOOT_MEMORY_AVAILABLE`, `MULTIBOOT_MEMORY_RESERVED`, etc.). I walk the list, mark every page in available regions as free, then mark every page occupied by the kernel image (between `_kernel_start` and `_kernel_end` from the linker script) as used.

The bug here was sneaky. I was correctly marking the kernel as used, but **not the bitmap itself**. So the PMM would happily allocate the page that contained its own bookkeeping. The first allocation would corrupt the bitmap, and the second would return a page that was already in use. I caught this when `kmalloc` started handing out the same pointer twice.

I now reserve the bitmap's pages explicitly, after marking the kernel. Feels obvious in retrospect; wasn't at the time.

---

## 8. Paging and the VMM

The VMM was the part of the project where I was most thankful for the lectures. Four-level paging on x86-64 is genuinely complex — PML4 → PDPT → PD → PT, each level 512 entries of 8 bytes — and the only way to keep it straight in your head is to draw a diagram.

I drew the diagram. It's in my notebook, taped to the wall above my desk. It saved me probably ten hours.

The VMM ended up smaller than I expected, because the boot assembly has already done the hard work of building an identity map for the first 8 MB. The C-side VMM only has to: (a) walk page tables to translate virtual → physical, and (b) map new pages on demand for the heap. Most of the kernel lives inside the identity map and never needs the VMM at all.

I never implemented page-fault recovery. If you fault, you panic. This is fine for a single-address-space kernel where every fault is a bug; it would not be fine for a real OS with userspace.

---

## 9. kmalloc — the part everyone underestimates

I budgeted three days for the heap allocator. It took eight.

The design is a **free-list of variable-size blocks** with a header carrying the size and a "free" flag. Allocation walks the list, finds a block big enough, splits it if there's leftover space. Freeing marks the block free and (in theory) coalesces with neighbors.

Three subtle bugs cost me three nights:

1. **Header alignment.** My header was 16 bytes, but I wasn't aligning the *user pointer* to 16. Strings worked fine; the moment I tried to allocate something containing a `uint64_t` field, on some calls the field would straddle a cache line and reads would return garbage. (Actually — they wouldn't. x86 handles unaligned access fine. The real bug was different. I'm leaving this paragraph in because I spent a full evening *believing* it was alignment before finding the actual cause.)

2. **The actual alignment-related bug** was that I was returning `header + 1` as the user pointer, but `sizeof(header)` was 24 bytes due to padding inside the struct, not 16. So my "allocate 32 bytes" call was returning a pointer 24 bytes into a 32-byte block, leaving only 8 bytes usable. Fixed by hand-packing the header with `__attribute__((packed))` and pinning it to exactly 16 bytes.

3. **Coalescing was off-by-one.** When freeing, I checked the next block's "free" flag — but I was reading from the wrong offset, getting effectively random data, and sometimes coalescing with non-free blocks. The heap would slowly corrupt itself over many alloc/free cycles. I caught this by adding a `kmalloc_verify()` walk that you can call to scan the whole heap for consistency. Now I call it in tests, never in production.

The current `kmalloc` is good enough. It is not fast. It is not fragmentation-resistant. If I had another month I'd replace it with a slab allocator. I do not have another month.

---

## 10. Interrupts and the keyboard (week 6)

Writing `isr_stubs.asm` was tedious — 256 nearly-identical stubs, each pushing its vector number and jumping to a common handler. I generated them with a NASM macro instead of typing them out, which is a flex I'm proud of.

The keyboard driver was the first thing in the project that *felt good* to use. After weeks of staring at boot logs, typing a key and seeing it appear on screen felt magical. The first time I held Shift and typed "HELLO" and it actually came out uppercase, I called my roommate over to look.

The PS/2 protocol is mercifully simple. Each keypress generates one or two scancode bytes from set 1 (the legacy AT set, which IBM has been promising to deprecate since approximately 1986). The keyboard IRQ (IRQ1) fires, you read port `0x60`, you decode it. Shift, Caps Lock, and modifier handling is bookkeeping.

The bug here: I was sending the End-of-Interrupt signal (EOI) to the PIC **before** reading the scancode, instead of after. This caused the PIC to immediately re-fire the same IRQ before I'd cleared the scancode out of the keyboard's output buffer, leading to an interrupt storm where the kernel did nothing but service IRQ1. The fix is a one-line reorder. The diagnosis took an hour.

---

## 11. The scheduler and context switching (week 7–8)

This was the most intellectually exciting part of the project.

A round-robin scheduler is conceptually simple — a circular list of processes, each tick you pick the next ready one and switch to it. The implementation hinges on one function: `switch_to(uint64_t *old_rsp_ptr, uint64_t new_rsp)`. You save the outgoing process's callee-saved registers on its stack, write its current RSP into its PCB, load the next process's RSP, pop its callee-saved registers, and `ret` — which transfers control to wherever the next process last paused.

That last sentence took me a week to internalize. The trick is that `ret` doesn't know or care that it's "switching processes" — it just pops a return address and jumps. If you've set up the new process's stack to look like a function that called `switch_to` and is now returning, the CPU will happily resume it.

Two bugs lived in `switch.asm` for days:

1. **First entry of a brand-new process.** A new process has never been "switched away from", so its stack is fake — I have to fabricate it to look like one that was switched away from. I push a return address (the process's entry function), then six zeros for the callee-saved registers, then point the PCB's `rsp` at the top. The first time `switch_to` resumes this process, it pops the zeros, hits `ret`, and jumps to the entry function. I got the *order* of those six zeros wrong on my first try (they're popped in reverse order from how they're pushed), and the brand-new process would jump to address `0x0` and triple-fault.

2. **The interrupts-disabled trap.** `switch_to` is sometimes called from the timer IRQ handler, where IF=0. If I switched to a new process and didn't re-enable interrupts, the new process would run with interrupts permanently off, and any `hlt` (e.g. in the idle loop) would freeze the CPU forever. I added an `sti` at the end of `switch_to`, just before the `ret`. There's a comment in the file explaining this because future-me will absolutely not remember why it's there.

The PIT (Programmable Interval Timer) drives preemption. I configured it for ~100 Hz; each tick the IRQ0 handler calls `scheduler_yield()`, and processes get preempted whether they cooperate or not. The first time I saw `demo` and `shell` interleaving their output without any explicit yields, that was the moment ElemOS felt like a real (tiny) operating system.

---

## 12. The shell (week 9)

After all that low-level work, the shell was almost embarrassingly easy. A loop: read line, tokenize on spaces, dispatch on the first token. Six commands: `help`, `clear`, `mem`, `ps`, `yield`, `hello`. The shell is just an ordinary kernel process that happens to call `keyboard_getchar()` and `kprintf()`.

The most satisfying command to write was `ps`. It walks the process list, prints PID/state/name, and demonstrates that the scheduler bookkeeping is real. The first time I ran `ps` and saw three processes — `idle`, `demo`, `shell` — with the right states, I closed my laptop and went to get a coffee. That felt like the project was done.

It wasn't done. But it felt like it.

---

## 13. Things I would do differently

- **I'd write `make debug` on day one.** I let GDB stay broken for a month because I was scared of the configuration. When I finally fixed it, every category of bug got cheaper to find.
- **I'd commit more often.** My git history has one commit. (The current one. "made the project".) This is a confession. I would lose my mind if I had to bisect anything in this repo. Future projects: I commit per subsystem at minimum.
- **I'd write tests for the PMM and kmalloc.** Both were bug-rich, and both are pure functions that could've been tested in isolation. I kept thinking "I'll add tests once it's stable" and then it was stable so I didn't.
- **I'd skip the high-half mapping** in `boot.asm` until I actually used it. It's dead code that confused me twice when I forgot it was there.

---

## 14. What I learned

Aside from the obvious — paging, interrupts, scheduling — I learned three meta-lessons that I think generalize beyond OS work:

1. **When a system is silent on failure, instrument it loudly.** Triple faults, missing Multiboot headers, IRQ storms — they all looked the same to me at first (a hang). Learning to differentiate by adding output at every layer is the single most useful debugging skill I picked up.

2. **The hardware is not your friend, but it is honest.** Every bug I had was eventually explainable by going back to the Intel SDM or the OSDev wiki. The CPU never lies. If your code is doing something weird, you are doing something weird.

3. **Big projects are an aggregation of small wins.** I could not have planned this project in advance and predicted how to write it. Every week I knew what the next milestone was, but I rarely knew what the one after looked like until I got there. The first time `mem` printed correct numbers, I had no idea how I'd write the scheduler. The first time `ps` worked, I had no idea how I'd write the shell. You build the next thing on top of the last thing, and eventually you have a kernel.

---

## 15. Final state

ElemOS boots cleanly under QEMU, initializes twelve subsystems, runs two user processes preemptively, and exposes an interactive shell. It is roughly 3,000 lines of C and 400 lines of assembly. It does not have a filesystem, userspace separation, or system calls. It will absolutely panic if you breathe on it wrong. But it is mine, and it is real, and I am proud of it.

— Sujat
