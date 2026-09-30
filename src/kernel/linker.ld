ENTRY(_start)

SECTIONS
{
    . = 0x7E00;

    .text : { *(.text) }
    .data : { *(.data) }
    .bss  : { *(.bss) }

    . = ALIGN(4096);
    _kernel_end = .;

    /DISCARD/ : { *(.comment) *(.note*) }
}
