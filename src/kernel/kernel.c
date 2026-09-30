#include "vga.h"
#include "keyboard.h"
#include "ata.h"
#include "fs.h"
#include "wm.h"
#include "pit.h"
#include "memory.h"
#include "string.h"

extern void idt_init(void);
extern void gdt_init(void);
extern void shell_run(void);

void kmain(void)
{
    gdt_init();
    idt_init();
    memory_init();
    paging_init();
    pit_init(100);
    __asm__ volatile("sti");

    vga_init();
    wm_init();

    term_print_color(wm_current(), "LunariaOS V-0\n", VGA_COLOR(BLACK, GREEN));
    term_print_color(wm_current(), "Booted\n", VGA_COLOR(BLACK, GREEN));
    print_banner(BLUE);

    char kb[12];
    term_print_color(wm_current(), "[MEM]: ", VGA_COLOR(BLACK, BLUE));
    itoa((int)memory_total_kb(), kb);
    term_print_color(wm_current(), kb, VGA_COLOR(BLACK, BLUE));
    term_print_color(wm_current(), " KB total, ", VGA_COLOR(BLACK, BLUE));
    itoa((int)memory_free_frames(), kb);
    term_print_color(wm_current(), kb, VGA_COLOR(BLACK, BLUE));
    term_print_color(wm_current(), " free frames (paging ON)\n", VGA_COLOR(BLACK, BLUE));

    if (fs_mount() == 0)
        term_print_color(wm_current(), "[FS]: Mounted\n", VGA_COLOR(BLACK, BLUE));
    else
        term_print_color(wm_current(), "[FS]: BAD MAGIC\n", VGA_COLOR(BLACK, RED));

    term_print(wm_current(), "\n");
    wm_blit(wm_current());

#ifdef PAGER_TEST
    {
        extern void shell_run_program(const char *name);
        char log[192];
        char num[12];
        unsigned int cr0, cr3;
        unsigned int o = 0;
        const char *hdr = "PAGER_TEST boot: mem_kb=";
        for (const char *p = hdr; *p; p++)
            log[o++] = *p;
        itoa((int)memory_total_kb(), num);
        for (char *p = num; *p; p++)
            log[o++] = *p;
        const char *m1 = " free=";
        for (const char *p = m1; *p; p++)
            log[o++] = *p;
        itoa((int)memory_free_frames(), num);
        for (char *p = num; *p; p++)
            log[o++] = *p;
        __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        const char *m2 = " cr0=";
        for (const char *p = m2; *p; p++)
            log[o++] = *p;
        itoa((int)cr0, num);
        for (char *p = num; *p; p++)
            log[o++] = *p;
        const char *m3 = " cr3=";
        for (const char *p = m3; *p; p++)
            log[o++] = *p;
        itoa((int)cr3, num);
        for (char *p = num; *p; p++)
            log[o++] = *p;
        const char *m4 = " pg=";
        for (const char *p = m4; *p; p++)
            log[o++] = *p;
        itoa((int)((cr0 >> 31) & 1), num);
        for (char *p = num; *p; p++)
            log[o++] = *p;
        log[o++] = '\n';
        log[o] = '\0';
        fs_write("TEST.LOG", FS_FILE, log, o);
#ifdef PAGER_PROBE
        /* faultprobe deliberately reads a kernel page to prove ring-3
           isolation faults, so it only runs when explicitly asked for. */
        shell_run_program("faultprobe");
#endif
    }
#endif

    shell_run();
}
