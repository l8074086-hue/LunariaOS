ENTRY(_start)

SECTIONS
{
    . = 0x7E00;

    .text : { *(.text) }
    .data : { *(.data) }
    .bss  : { *(.bss) }

    /DISCARD/ : { *(.comment) *(.note*) }
}
