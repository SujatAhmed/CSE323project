# ElemOS — An Elementary Kernel (CSE323)

ElemOS is a 64-bit hobby kernel written in C and x86-64 assembly, built for the CSE323 Operating Systems course. It boots via GRUB2 Multiboot2, initializes core OS subsystems, and drops into an interactive shell.

---

## Project Structure

```
src/
├── arch/x86/       # GDT, IDT, ISR, IRQ, PIC
├── boot/           # Multiboot2 entry, long-mode setup (boot.asm)
├── drivers/        # VGA, keyboard (PS/2), serial (COM1), PIT
├── kernel/         # kernel_main — boot sequence
├── lib/            # kprintf, string utils, panic
├── mm/             # PMM (bitmap), VMM (4-level paging), kmalloc (free-list)
├── proc/           # Round-robin scheduler, context switch
└── shell/          # Interactive shell
linker.ld           # Kernel linked at 1MB physical
grub.cfg            # GRUB2 Multiboot2 menu entry
Makefile
```

---

## Prerequisites

Install the following tools before building:

```bash
# On Debian/Ubuntu
sudo apt install gcc-x86-64-linux-gnu nasm binutils grub-pc-bin grub-common xorriso qemu-system-x86 gdb

# On Arch Linux
sudo pacman -S x86_64-elf-gcc nasm binutils grub libisoburn xorriso qemu gdb
```

Verify each tool is available:

```bash
x86_64-linux-gnu-gcc --version
nasm --version
ld --version
grub-mkrescue --version
qemu-system-x86_64 --version
```

> **Note:** On Arch Linux the cross-compiler may be named `x86_64-elf-gcc`. If so, edit `CC` in the `Makefile` to match.

---

## Build

### Build the bootable ISO

```bash
make
```

This compiles all C and assembly sources, links `build/kernel.elf`, and packages it into `build/elemOS.iso` via `grub-mkrescue`.

### Clean build artifacts

```bash
make clean
```

---

## Running

### Normal run (QEMU with VGA display)

```bash
make run
```

QEMU launches with:
- 256 MB RAM
- Serial output mirrored to the terminal (`-serial stdio`)
- No automatic reboot or shutdown on triple-fault

You will see the ElemOS banner and boot log in the QEMU window, then the shell prompt in a moment.

### Run without a graphical window (serial-only)

```bash
make run-nographic
```

All output goes to the terminal via the emulated serial port (COM1). Useful on headless systems or inside SSH sessions. Press `Ctrl+A` then `X` to quit QEMU.

### Debug with GDB

```bash
make debug
```

This starts QEMU paused and waiting for GDB on port 1234, then launches GDB with a breakpoint pre-set at `kernel_main`. Useful commands inside GDB:

```
(gdb) continue          # resume execution
(gdb) break vga_init    # set another breakpoint
(gdb) info registers    # inspect CPU registers
(gdb) x/10i $rip        # disassemble around the instruction pointer
(gdb) quit              # exit (also kills QEMU)
```

---

## Boot Sequence

When the kernel starts, the following initialization steps run in order and are printed to both VGA and the COM1 serial port:

| Step | Subsystem | What happens |
|------|-----------|--------------|
| 1 | Serial (COM1) | Debug output available before VGA is ready |
| 2 | VGA | Text-mode console initialized; ElemOS banner printed |
| 3 | GDT | Global Descriptor Table set up for 64-bit kernel mode |
| 4 | IDT + PIC | Interrupt Descriptor Table and 8259 PIC initialized |
| 5 | PMM | Physical Memory Manager reads Multiboot2 memory map; reports total/free RAM |
| 6 | VMM | Virtual Memory Manager ready (identity map already live from boot.asm) |
| 7 | kmalloc | 16 MB kernel heap initialized at virtual address 0x400000 |
| 8 | Keyboard | PS/2 IRQ1 handler registered |
| 9 | Scheduler | Round-robin scheduler initialized with an idle process |
| 10 | PIT | Timer fires at ~100 Hz to drive preemptive multitasking |
| 11 | Processes | `demo` and `shell` processes spawned |
| 12 | Interrupts | `sti` — system fully operational |

---

## Interactive Shell

After boot the shell prints:

```
ElemOS shell ready. Type 'help' for commands.

elemos>
```

The keyboard uses US QWERTY layout (scancode set 1). Shift is supported for uppercase and symbols.

### Shell Commands

#### `help`
Prints the list of all available commands.

```
elemos> help
ElemOS shell commands:
  help   - show this message
  clear  - clear the screen
  mem    - show memory statistics
  ps     - list processes
  yield  - voluntarily yield to next process
  hello  - print a greeting
```

#### `clear`
Clears the VGA text-mode screen.

```
elemos> clear
```

#### `mem`
Displays physical memory and kernel heap usage.

```
elemos> mem
Physical memory:
  Total : 262144 KB (256 MB)
  Used  :   4096 KB
  Free  : 258048 KB
Kernel heap:
  In use: 512 bytes
```

The PMM tracks pages (4 KB each) using a bitmap. Used pages include the kernel image and any allocated process stacks. The heap starts at virtual address 4 MB and can grow up to 16 MB.

#### `ps`
Lists all processes with their PID, state, and name.

```
elemos> ps
PID  STATE    NAME
---  -------  ----
0    READY    idle
1    ZOMBIE   demo
2    RUNNING  shell
```

| State | Meaning |
|-------|---------|
| `READY` | Eligible to run, waiting for its turn |
| `RUNNING` | Currently executing |
| `BLOCKED` | Waiting on I/O or an event |
| `ZOMBIE` | Finished, awaiting cleanup |

#### `yield`
Voluntarily gives up the CPU to the next runnable process. Useful for demonstrating cooperative scheduling.

```
elemos> yield
```

#### `hello`
Prints a greeting — a minimal smoke-test that the shell dispatch and `kprintf` are working.

```
elemos> hello
Hello from ElemOS!
```

---

## Testing Individual Subsystems

### Memory Management

1. Run `mem` immediately after boot to see the PMM baseline.
2. Run `mem` again after using other commands — heap usage (`In use`) should increase only if allocations occurred.
3. The `demo` process allocates a stack via `kmalloc` on creation. After it finishes (`ZOMBIE` in `ps`), the stack memory is not reclaimed (no `kfree` is called) — this is expected behaviour for this version of the kernel.

### Process Scheduler

1. Boot the kernel. The `demo` process runs first and prints:
   ```
   [demo] iteration 1 — yielding
   [demo] iteration 2 — yielding
   [demo] iteration 3 — yielding
   [demo] process finished
   [scheduler] process 'demo' exited
   ```
2. After `demo` finishes, the shell becomes the sole runnable process.
3. Run `ps` to confirm `demo` is `ZOMBIE` and `shell` is `RUNNING`.
4. Type `yield` in the shell to manually trigger a context switch. With only the idle thread and shell alive, control returns to the shell immediately.
5. The PIT (Programmable Interval Timer) fires at ~100 Hz. Each tick calls `scheduler_yield()` automatically, demonstrating preemptive multitasking even without typing `yield`.

### Keyboard Driver

1. Type any printable character — it should echo on screen.
2. Hold Shift and type letters — they should appear in uppercase.
3. Press Backspace — the last character should be erased.
4. Type a command longer than 255 characters — excess input is silently dropped (buffer limit).
5. Type an unknown command and press Enter:
   ```
   elemos> foobar
   Unknown command: 'foobar' (type 'help' for list)
   ```

### Serial Output

When running with `make run` or `make debug`, all `kprintf` output is mirrored to `stdout` via the emulated COM1 serial port. The full boot log and shell output will appear in the terminal alongside the QEMU window. This is useful for capturing logs without relying on the VGA buffer.

When running with `make run-nographic`, the serial port is the *only* output channel — the VGA framebuffer is not displayed.

### Interrupt Handling

- **IRQ0 (PIT):** fires continuously and drives preemption. A stall in the PIT handler would freeze the system.
- **IRQ1 (Keyboard):** fires on each keypress. Characters land in a 64-byte circular buffer; `keyboard_getchar()` blocks (via `hlt`) until a character is available.
- Unhandled interrupts reach the generic ISR handler, which calls `PANIC()` and halts the CPU with a message on screen.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `make` fails: `x86_64-linux-gnu-gcc: not found` | Cross-compiler not installed or named differently | Install `gcc-x86-64-linux-gnu` or update `CC` in `Makefile` |
| `make` fails: `grub-mkrescue: not found` | GRUB tools missing | Install `grub-pc-bin grub-common xorriso` |
| QEMU shows blank screen | VGA init failed or boot halted before kernel_main | Check serial output (`-serial stdio`) for error messages |
| Kernel prints `WARNING: not booted via Multiboot2!` | Wrong bootloader or ISO not used | Always boot via `make run`; do not load the ELF directly |
| `[boot] Initializing PMM...` then hang | Multiboot2 memory map tag missing | Ensure `grub.cfg` uses `multiboot2` (not `multiboot`) directive |
| No keyboard input in QEMU | IRQ1 not wired | Confirm QEMU is not in `-nographic` mode without serial input |
