#!/run/current-system/sw/bin/bash

PATHS=(
    "$HOME/data/Projects/c/OS/src/boot/boot.asm"
    "$HOME/data/Projects/c/OS/src/kernel/entry.asm"
    "$HOME/data/Projects/c/OS/src/kernel/kernel.c"
    "$HOME/data/Projects/c/OS/src/drivers/vga.c"
    "$HOME/data/Projects/c/OS/src/headers/vga.h"
    "$HOME/data/Projects/c/OS/src/headers/port.h"
    "$HOME/data/Projects/c/OS/src/drivers/keyboard.c"
    "$HOME/data/Projects/c/OS/src/headers/keyboard.h"
    "$HOME/data/Projects/c/OS/src/drivers/ata.c"
    "$HOME/data/Projects/c/OS/src/headers/ata.h"
    "$HOME/data/Projects/c/OS/src/kernel/isr.asm"
    "$HOME/data/Projects/c/OS/src/kernel/idt.c"
    "$HOME/data/Projects/c/OS/src/kernel/shell.c"
    "$HOME/data/Projects/c/OS/src/kernel/wm.c"
    "$HOME/data/Projects/c/OS/src/headers/wm.h"
    "$HOME/data/Projects/c/OS/Makefile"
    "$HOME/data/Projects/c/OS/todo.md"

)

for path in "${PATHS[@]}";do
    echo $path:
    cat $path
done
