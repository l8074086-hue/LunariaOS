#include "vga.h"
#include "wm.h"
#include "user.h"
#include "keyboard.h"
#include "fs.h"
#include "string.h"
#include "port.h"
#include "memory.h"

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
                terminal_t *t = wm_current();
                term_putchar_at(t, (char)r->ebx, r->ecx, r->edx, (color_t)r->esi);
                wm_blit_row(t, r->edx);
            }
            break;
        case 10:
            if (r->ebx < 80 && r->ecx < 25)
            {
                terminal_t *t = wm_current();
                int oy = t->cursor_y;
                t->cursor_x = r->ebx;
                t->cursor_y = r->ecx;
                /* the caret can be on either row; refresh both so the old
                   one is un-inverted and the new one drawn */
                wm_blit_row(t, oy);
                if (oy != t->cursor_y)
                    wm_blit_row(t, t->cursor_y);
            }
            break;
        case 11:
            /* grow the heap; the pages behind it are demand paged, and a
               failed grow comes back as the unchanged old break */
            r->eax = vm_sbrk(r->ebx);
            break;
        case 12:
            r->eax = fs_open((const char *)r->ebx, (int)r->ecx);
            break;
        case 13:
            r->eax = fs_read_fd((int)r->ebx, (char *)r->ecx, r->edx);
            break;
        case 14:
            r->eax = fs_write_fd((int)r->ebx, (const char *)r->ecx, r->edx);
            break;
        case 15:
            r->eax = fs_seek((int)r->ebx, (int)r->ecx, (int)r->edx);
            break;
        case 16:
            r->eax = fs_close((int)r->ebx);
            break;
        case 17:
            r->eax = fs_unlink((const char *)r->ebx);
            break;
        default:
            break;
    }
}

static void uhex(unsigned int v, char *buf)
{
    for (int i = 7; i >= 0; i--)
    {
        unsigned int d = v & 0xF;
        buf[i] = d < 10 ? (char)('0' + d) : (char)('A' + d - 10);
        v >>= 4;
    }
    buf[8] = '\0';
}

int fault_handler(struct regs *r, unsigned int cr2, unsigned int err, int vector)
{
    (void)r;   /* only the PAGER_TEST log below reads the saved registers */

    /* A missing page inside the ring-3 window is not an error: hand it a
       zeroed page and let the CPU retry the instruction. A fault on an
       already-present page is a real protection violation, so it falls
       through to the report below. */
    if (vector == 0x0E && vm_fault(cr2))
        return 1;

    char vbuf[12];
    char abuf[9];
    terminal_t *t = wm_current();
    color_t c = VGA_COLOR(BLACK, RED);
    itoa(vector, vbuf);
    term_print_color(t, "\nFAULT vec=", c);
    term_print_color(t, vbuf, c);
    uhex(cr2, abuf);
    term_print_color(t, " cr2=0x", c);
    term_print_color(t, abuf, c);
    itoa((int)err, vbuf);
    term_print_color(t, " err=", c);
    term_print_color(t, vbuf, c);
    if (vector == 0x0E)
    {
        term_print_color(t, (err & 1) ? " present" : " not-present", c);
        term_print_color(t, (err & 2) ? " write" : " read", c);
        term_print_color(t, (err & 4) ? " user" : " kernel", c);
    }
    term_print_color(t, "\n", c);
    wm_blit(t);

#ifdef PAGER_TEST
    {
        char msg[448];
        unsigned int o = 0;
        const char *m;
        volatile unsigned int *raw = (volatile unsigned int *)0x6000;
        unsigned int rvec = raw[0];
        unsigned int rcr2 = raw[1];
        unsigned int rerr = raw[2];
        unsigned int reip = raw[3];
        unsigned int rcs = raw[4];
        unsigned int cr3reg, pde_usr = 0, pte_usr = 0;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3reg));
        {
            unsigned int *pd = (unsigned int *)(cr3reg & ~0xFFFu);
            unsigned int pdi = USER_BASE >> 22;
            if (pd[pdi] & 1)
            {
                unsigned int *pt = (unsigned int *)(pd[pdi] & 0xFFFFF000);
                pte_usr = pt[(USER_BASE >> 12) & 0x3FF];
                pde_usr = pd[pdi];
            }
            raw[5] = cr3reg;
            raw[6] = pde_usr;
            raw[7] = pte_usr;
        }
        char num[12];
        m = "FAULT vec=";
        for (; *m; m++) msg[o++] = *m;
        itoa((int)rvec, num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " cs=";
        for (; *m; m++) msg[o++] = *m;
        uhex(rcs, num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " eip=";
        for (; *m; m++) msg[o++] = *m;
        uhex(reip, num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " cr2=";
        for (; *m; m++) msg[o++] = *m;
        uhex(rcr2, num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " err=";
        for (; *m; m++) msg[o++] = *m;
        uhex(rerr, num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " present=";
        for (; *m; m++) msg[o++] = *m;
        itoa((int)((rerr & 1) ? 1 : 0), num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " user=";
        for (; *m; m++) msg[o++] = *m;
        itoa((int)((rerr & 4) ? 1 : 0), num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " write=";
        for (; *m; m++) msg[o++] = *m;
        itoa((int)((rerr & 2) ? 1 : 0), num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " cr3=";
        for (; *m; m++) msg[o++] = *m;
        itoa((int)raw[5], num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " pde_usr=";
        for (; *m; m++) msg[o++] = *m;
        uhex(raw[6], num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        m = " pte_usr=";
        for (; *m; m++) msg[o++] = *m;
        uhex(raw[7], num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        {   /* saved registers at fault time, to decode the instruction */
            const unsigned int regs[6] = { r->edi, r->esi, r->ebp, r->ebx, r->ecx, r->eax };
            const char *names[6] = { " edi=", " esi=", " ebp=", " ebx=", " ecx=", " eax=" };
            for (int k = 0; k < 6; k++)
            {
                for (m = names[k]; *m; m++) msg[o++] = *m;
                uhex(regs[k], num);
                for (char *p = num; *p; p++) msg[o++] = *p;
            }
        }
        m = " nfix=";
        for (; *m; m++) msg[o++] = *m;
        itoa((int)vm_faults(), num);
        for (char *p = num; *p; p++) msg[o++] = *p;
        msg[o++] = '\n';
        msg[o] = '\0';
        fs_write("FAULT.LOG", FS_FILE, msg, o);
    }
#endif

    user_halt();
    return 0;      /* unreachable: user_halt() never returns */
}

