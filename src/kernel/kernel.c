#include "vga.h"
#include "keyboard.h"
#include "ata.h"
#include "string.h"
#include "fs.h"
#include "wm.h"

extern void idt_init(void);
extern void gdt_init(void);
extern void shell_run(void);

void kmain(void)
{
    gdt_init();
    idt_init();
    __asm__ volatile("sti");

    vga_init();
    wm_init();

    term_print_color(wm_current(), "LunariaOS V-0\n", VGA_COLOR(BLACK, GREEN));
    term_print_color(wm_current(), "Booted\n", VGA_COLOR(BLACK, GREEN));

    if (fs_mount() == 0)
        term_print_color(wm_current(), "[FS]: Mounted\n", VGA_COLOR(BLACK, BLUE));
    else
        term_print_color(wm_current(), "[FS]: BAD MAGIC\n", VGA_COLOR(BLACK, RED));

    term_print(wm_current(), "\n");
    wm_blit(wm_current());

    shell_run();
}
