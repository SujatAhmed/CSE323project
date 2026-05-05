ENTRY(_start)

SECTIONS {
    /* Load kernel at 1MB physical = 1MB virtual (identity mapped) */
    . = 1M;

    _kernel_start = .;

    .multiboot2 : {
        KEEP(*(.multiboot2))
    }

    .text ALIGN(4K) : {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) : {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    _kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note.GNU-stack)
    }
}
