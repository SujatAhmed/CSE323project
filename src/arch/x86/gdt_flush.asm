bits 64
global gdt_flush

; gdt_flush(uint64_t gdt_ptr_addr)
; RDI = address of GDT pointer struct
gdt_flush:
    lgdt [rdi]
    ; Far return to reload CS with kernel code segment
    push qword 0x08         ; kernel code selector
    lea  rax, [rel .reload]
    push rax
    retfq
.reload:
    mov ax, 0x10            ; kernel data selector
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax
    ret
