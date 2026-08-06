#include "vga.h"
#include "wm.h"
#include "user.h"
#include "keyboard.h"
#include "fs.h"
#include "string.h"
#include "port.h"

struct regs
{
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

static void user_halt(void)
{
    __asm__ volatile("cli");
    for (;;)
        __asm__ volatile("hlt");
}

static void user_power_off(void)
{
    __asm__ volatile("cli");
    outw(0x604, 0x2000);
    for (;;)
        __asm__ volatile("hlt");
}

static void user_reboot(void)
{
    __asm__ volatile("cli");
    outb(0x64, 0xFE);
    for (;;)
        __asm__ volatile("hlt");
}

static void (*exit_target)(void) = exit_to_shell;

static void sys_write(const char *buf, unsigned int len)
{
    terminal_t *t = wm_current();
    for (unsigned int i = 0; i < len; i++)
        term_putchar(t, buf[i], VGA_COLOR(BLACK, WHITE));
    wm_blit(t);
}

void syscall_handler(struct regs *r, unsigned int *frame)
{
    switch (r->eax)
    {
        case 1:
            sys_write((const char *)r->ebx, r->ecx);
            break;
        case 2:
            frame[0] = (unsigned int)exit_target;
            frame[1] = 0x08;
            break;
        case 3: {
            int c;
            do {
                c = kbd_getc();
            } while (c == 0);
            r->eax = c;
            break;
        }
        case 4:
            r->eax = fs_read((const char *)r->ebx, (char *)r->ecx, r->edx);
            break;
        case 5:
            r->eax = fs_write((const char *)r->ebx, FS_FILE, (const char *)r->ecx, r->edx);
            break;
        case 6:
            r->eax = fs_ls_buf((const char *)r->ebx, (char *)r->ecx, r->edx);
            break;
        case 7:
            switch (r->ebx)
            {
                case 0: exit_target = exit_to_shell; break;
                case 1: exit_target = user_halt; break;
                case 2: exit_target = user_power_off; break;
                case 3: exit_target = user_reboot; break;
                default: break;
            }
            break;
        case 8:
            term_clear(wm_current());
            wm_blit(wm_current());
            break;
        case 9:
            if (r->ecx < 80 && r->edx < 25)
            {
                term_putchar_at(wm_current(), (char)r->ebx, r->ecx, r->edx, (color_t)r->esi);
                wm_blit(wm_current());
            }
            break;
        case 10:
            if (r->ebx < 80 && r->ecx < 25)
            {
                terminal_t *t = wm_current();
                t->cursor_x = r->ebx;
                t->cursor_y = r->ecx;
                wm_blit(t);
            }
            break;
        default:
            break;
    }
}

void fault_handler(int vector)
{
    char buf[12];
    terminal_t *t = wm_current();
    term_print_color(t, "FAULT ", VGA_COLOR(BLACK, RED));
    itoa(vector, buf);
    term_print_color(t, buf, VGA_COLOR(BLACK, RED));
    term_print_color(t, "\n", VGA_COLOR(BLACK, RED));
    wm_blit(t);
}
