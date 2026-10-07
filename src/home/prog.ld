ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text : { *(.text._start) *(.text*) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) *(COMMON) }

    /* First byte past the image and its .bss. The userspace allocator grows
       the program break up to here so the heap cannot overlap either. */
    . = ALIGN(8);
    __heap_start = .;

    /DISCARD/ : { *(.comment) *(.note*) }
}
