; boot.asm — Multiboot2 entry, long-mode setup, jump to kernel_main
; The CPU is in 32-bit protected mode when GRUB hands control here.

bits 32

; ─── Multiboot2 header ───────────────────────────────────────────────────────
MB2_MAGIC equ 0xE85250D6
MB2_ARCH  equ 0           ; i386 protected mode

section .multiboot2
align 8
mb2_header_start:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd mb2_header_end - mb2_header_start
    dd -(MB2_MAGIC + MB2_ARCH + (mb2_header_end - mb2_header_start))
    ; End tag
    dw 0
    dw 0
    dd 8
mb2_header_end:

; ─── BSS: page tables + initial stack ────────────────────────────────────────
section .bss
align 4096

global pml4_table
pml4_table: resb 4096

pdpt_lo:    resb 4096     ; identity map (first 8MB)
pd_lo:      resb 4096

pdpt_hi:    resb 4096     ; kernel high map (same 8MB, different VA)
pd_hi:      resb 4096

align 16
stack_bottom:
    resb 32768            ; 32KB boot stack
global stack_top
stack_top:

; ─── Saved multiboot info ────────────────────────────────────────────────────
section .data
align 4
mb_magic: dd 0
mb_info:  dd 0

; ─── 32-bit startup ──────────────────────────────────────────────────────────
section .text
global _start
extern kernel_main

_start:
    ; Save multiboot magic + info pointer ASAP
    mov [mb_magic], eax
    mov [mb_info],  ebx

    mov esp, stack_top

    ; ── Verify CPUID support ──────────────────────────────────────────────
    ; Try flipping ID bit in EFLAGS
    pushfd
    pop  eax
    mov  ecx, eax
    xor  eax, 0x200000
    push eax
    popfd
    pushfd
    pop  eax
    push ecx
    popfd
    cmp  eax, ecx
    je   .no_cpuid

    ; ── Verify long mode support via CPUID ───────────────────────────────
    mov  eax, 0x80000000
    cpuid
    cmp  eax, 0x80000001
    jb   .no_longmode
    mov  eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz   .no_longmode

    ; ── Zero page tables ─────────────────────────────────────────────────
    mov  edi, pml4_table
    xor  eax, eax
    mov  ecx, 5 * 1024      ; 5 pages × 1024 dwords = 5 × 4096 bytes
    rep  stosd

    ; ── Identity map: PML4[0] → pdpt_lo → pd_lo → 4× 2MB huge pages ─────
    mov  eax, pdpt_lo
    or   eax, 3                    ; P+W
    mov  [pml4_table], eax

    mov  eax, pd_lo
    or   eax, 3
    mov  [pdpt_lo], eax

    ; Map first 8MB (4 × 2MB pages) for identity
    mov  eax, 0x00000083           ; P+W+PS (huge page) at phys 0
    mov  [pd_lo + 0*8], eax
    mov  dword [pd_lo + 0*8 + 4], 0

    mov  eax, 0x00200083           ; phys 2MB
    mov  [pd_lo + 1*8], eax
    mov  dword [pd_lo + 1*8 + 4], 0

    mov  eax, 0x00400083           ; phys 4MB
    mov  [pd_lo + 2*8], eax
    mov  dword [pd_lo + 2*8 + 4], 0

    mov  eax, 0x00600083           ; phys 6MB
    mov  [pd_lo + 3*8], eax
    mov  dword [pd_lo + 3*8 + 4], 0

    ; ── Load CR3 ─────────────────────────────────────────────────────────
    mov  eax, pml4_table
    mov  cr3, eax

    ; ── Enable PAE ───────────────────────────────────────────────────────
    mov  eax, cr4
    or   eax, 1 << 5               ; CR4.PAE
    mov  cr4, eax

    ; ── Set EFER.LME ─────────────────────────────────────────────────────
    mov  ecx, 0xC0000080
    rdmsr
    or   eax, 1 << 8               ; LME
    wrmsr

    ; ── Enable paging (activates long mode) ──────────────────────────────
    mov  eax, cr0
    or   eax, (1 << 31) | (1 << 0) ; PG + PE
    mov  cr0, eax

    ; ── Load 64-bit GDT and far-jump into long mode ───────────────────────
    lgdt [gdt64_ptr]
    jmp  gdt64.code : long_mode_start

.no_cpuid:
.no_longmode:
    ; Print 'E' to port 0xE9 (QEMU debug port) and halt
    mov  al, 'E'
    out  0xE9, al
    cli
    hlt

; ─── 64-bit GDT (temporary; gdt.c will reload a proper one later) ────────────
section .rodata
align 8
gdt64:
    dq 0                            ; null
.code: equ $ - gdt64
    dq 0x00AF9A000000FFFF           ; 64-bit kernel code: L=1,P=1,DPL=0
.data: equ $ - gdt64
    dq 0x00CF92000000FFFF           ; 64-bit kernel data
gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64

; ─── 64-bit long-mode entry ──────────────────────────────────────────────────
section .text
bits 64
long_mode_start:
    mov  ax, gdt64.data
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    xor  ax, ax
    mov  fs, ax
    mov  gs, ax

    ; Set 64-bit stack (boot stack still valid — identity mapped)
    mov  rsp, stack_top

    ; Pass saved multiboot info to kernel_main
    mov  edi, dword [mb_magic]
    mov  esi, dword [mb_info]

    call kernel_main

.halt:
    cli
    hlt
    jmp .halt
