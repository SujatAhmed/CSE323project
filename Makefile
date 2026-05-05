CC      := x86_64-linux-gnu-gcc
AS      := nasm
LD      := ld
GRUB    := grub-mkrescue

CFLAGS  := -std=c11 -ffreestanding -O2 -Wall -Wextra \
           -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
           -fno-stack-protector -fno-pic \
           -Isrc

ASFLAGS := -f elf64
LDFLAGS := -T linker.ld -nostdlib -z max-page-size=0x1000

SRCDIR  := src
OBJDIR  := build/obj
ISODIR  := build/iso

# Gather all sources
C_SRCS  := $(shell find $(SRCDIR) -name '*.c')
ASM_SRCS:= $(shell find $(SRCDIR) -name '*.asm')

C_OBJS  := $(C_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
ASM_OBJS:= $(ASM_SRCS:$(SRCDIR)/%.asm=$(OBJDIR)/%.asm.o)

OBJS    := $(ASM_OBJS) $(C_OBJS)
KERNEL  := build/kernel.elf
ISO     := build/elemOS.iso

.PHONY: all run debug clean iso

all: $(ISO)

# ─── Kernel ELF ──────────────────────────────────────────────────────────────
$(KERNEL): $(OBJS) linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "  LD  $@"

# ─── Bootable ISO ────────────────────────────────────────────────────────────
$(ISO): $(KERNEL)
	@mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/kernel.elf
	cp grub.cfg  $(ISODIR)/boot/grub/grub.cfg
	$(GRUB) -o $@ $(ISODIR) 2>/dev/null
	@echo "  ISO $@"

iso: $(ISO)

# ─── Compile C sources ────────────────────────────────────────────────────────
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "  CC  $<"

# ─── Assemble NASM sources ───────────────────────────────────────────────────
$(OBJDIR)/%.asm.o: $(SRCDIR)/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@
	@echo "  AS  $<"

# ─── Run in QEMU ─────────────────────────────────────────────────────────────
run: $(ISO)
	qemu-system-x86_64 \
	    -cdrom $(ISO) \
	    -m 256M \
	    -serial stdio \
	    -no-reboot \
	    -no-shutdown

# ─── Debug (QEMU + GDB) ──────────────────────────────────────────────────────
debug: $(ISO)
	qemu-system-x86_64 \
	    -cdrom $(ISO) \
	    -m 256M \
	    -serial stdio \
	    -no-reboot \
	    -no-shutdown \
	    -s -S &
	@sleep 0.5
	gdb $(KERNEL) \
	    -ex "target remote :1234" \
	    -ex "symbol-file $(KERNEL)" \
	    -ex "break kernel_main" \
	    -ex "continue"

# ─── QEMU serial only (no display) ───────────────────────────────────────────
run-nographic: $(ISO)
	qemu-system-x86_64 \
	    -cdrom $(ISO) \
	    -m 256M \
	    -nographic \
	    -no-reboot \
	    -no-shutdown

clean:
	rm -rf build
	@echo "  Cleaned."
