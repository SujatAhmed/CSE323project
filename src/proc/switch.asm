bits 64

; switch_to(uint64_t *old_rsp_ptr, uint64_t new_rsp)
; RDI = address of current process's saved RSP field
; RSI = next process's saved RSP value
global switch_to
switch_to:
    ; Save callee-saved registers of the outgoing process
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Persist current RSP into the PCB field pointed to by RDI
    mov [rdi], rsp

    ; Load the next process's RSP
    mov rsp, rsi

    ; Restore callee-saved registers of the incoming process
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Re-enable interrupts before entering the incoming process.
    ; switch_to may be called from inside a timer IRQ handler (IF=0);
    ; without sti the resumed process would run with interrupts off and
    ; any subsequent hlt would freeze the CPU permanently.
    sti

    ; The pushed return address (entry or yield caller) is now at TOS
    ret
