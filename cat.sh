#!/run/current-system/sw/bin/bash

PATHS=(
    "$HOME/Projects/c/OS/src/boot/boot.asm"
    "$HOME/Projects/c/OS/src/kernel/entry.asm"
    "$HOME/Projects/c/OS/src/kernel/kernel.c"
    "$HOME/Projects/c/OS/src/drivers/vga.c"
    "$HOME/Projects/c/OS/src/headers/vga.h"
    "$HOME/Projects/c/OS/src/headers/port.h"
    "$HOME/Projects/c/OS/src/drivers/keyboard.c"
    "$HOME/Projects/c/OS/src/headers/keyboard.h"
    "$HOME/Projects/c/OS/src/drivers/ata.c"
    "$HOME/Projects/c/OS/src/headers/ata.h"
    "$HOME/Projects/c/OS/src/kernel/isr.asm"
    "$HOME/Projects/c/OS/src/kernel/idt.c"
    "$HOME/Projects/c/OS/src/kernel/shell.c"
    "$HOME/Projects/c/OS/src/kernel/wm.c"
    "$HOME/Projects/c/OS/src/headers/wm.h"
    "$HOME/Projects/c/OS/Makefile"
    "$HOME/Projects/c/OS/todo.md"

)

for path in "${PATHS[@]}";do
    echo $path:
    cat $path
done
