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

#ifdef PAGER_TEST
static unsigned int firstfix[8];   /* snapshot of the first repaired fault */
static unsigned int firstfix2[8];
static unsigned int raww[8];
/* every fault seen this boot, in order: eip, cs, cr2, err */
static unsigned int flog[48];
static int flogn;
#endif

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
        case 11:
            /* grow the heap; the pages behind it are demand paged, and a
               failed grow comes back as the unchanged old break */
            r->eax = vm_sbrk(r->ebx);
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
    (void)r;

#ifdef PAGER_TEST
    {
        /* snapshot the CPU frame *before* anything else can fault */
        volatile unsigned int *raw = (volatile unsigned int *)0x6000;
        if (flogn < 40)
        {
            flog[flogn++] = raw[3];   /* eip */
            flog[flogn++] = raw[4];   /* cs  */
            flog[flogn++] = cr2;
            flog[flogn++] = err;
        }
    }
#endif

    /* A missing page inside the ring-3 window is not an error: hand it a
       zeroed page and let the CPU retry the instruction. A fault on an
       already-present page is a real protection violation, so it falls
       through to the report below. */
    if (vector == 0x0E && vm_fault(cr2))
    {
#ifdef PAGER_TEST
        if (!firstfix[0])
        {
            unsigned int cr2now;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2now));
            firstfix[0] = 1;
            firstfix[1] = flog[flogn - 4];  /* eip */
            firstfix[2] = flog[flogn - 3];  /* cs  */
            firstfix[3] = flog[flogn - 2];  /* cr2 as passed */
            firstfix[4] = r->eax;
            firstfix[5] = r->ecx;
            firstfix[6] = r->edx;
            firstfix[7] = cr2now;
            firstfix2[0] = r->ebp;
            firstfix2[1] = *(volatile unsigned int *)(r->ebp + 4);
            firstfix2[2] = r->edi;
            firstfix2[3] = r->ebx;
            firstfix2[4] = r->esi;
            /* raw pushad words, so the layout can be checked by hand */
            firstfix2[5] = r->eax;
            firstfix2[6] = ((volatile unsigned int *)r)[2];
            firstfix2[7] = ((volatile unsigned int *)r)[4];
            raww[0] = ((volatile unsigned int *)r)[0];
            raww[1] = ((volatile unsigned int *)r)[1];
            raww[2] = ((volatile unsigned int *)r)[2];
            raww[3] = ((volatile unsigned int *)r)[3];
            raww[4] = ((volatile unsigned int *)r)[4];
            raww[5] = ((volatile unsigned int *)r)[5];
            raww[6] = ((volatile unsigned int *)r)[6];
            raww[7] = ((volatile unsigned int *)r)[7];
        }
#endif
        return 1;
    }

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
        char msg[1400];
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
        {   /* first repaired fault, to see what the retry re-ran */
            static const char *labels[7] = { " f_eip=", " f_cs=", " f_cr2=",
                                             " f_eax=", " f_ecx=", " f_edx=",
                                             " f_cr2b=" };
            for (int k = 0; k < 7; k++)
            {
                for (m = labels[k]; *m; m++) msg[o++] = *m;
                uhex(firstfix[1 + k], num);
                for (char *p = num; *p; p++) msg[o++] = *p;
            }
            static const char *labels2[5] = { " f_ebp=", " f_ret=", " f_edi=",
                                              " f_ebx=", " f_esi=" };
            for (int k = 0; k < 5; k++)
            {
                for (m = labels2[k]; *m; m++) msg[o++] = *m;
                uhex(firstfix2[k], num);
                for (char *p = num; *p; p++) msg[o++] = *p;
            }
            for (int k = 0; k < 4; k++)
            {
                for (m = " vmd="; *m; m++) msg[o++] = *m;
                uhex(vm_dbg[k], num);
                for (char *p = num; *p; p++) msg[o++] = *p;
            }
            for (int k = 0; k < 8; k++)
            {
                for (m = " w="; *m; m++) msg[o++] = *m;
                uhex(raww[k], num);
                for (char *p = num; *p; p++) msg[o++] = *p;
            }
        }
        {   /* full fault sequence this boot */
            for (int k = 0; k + 3 < flogn; k += 4)
            {
                for (m = " |e="; *m; m++) msg[o++] = *m;
                uhex(flog[k], num);
                for (char *p = num; *p; p++) msg[o++] = *p;
                for (m = " c="; *m; m++) msg[o++] = *m;
                uhex(flog[k + 1], num);
                for (char *p = num; *p; p++) msg[o++] = *p;
                for (m = " a="; *m; m++) msg[o++] = *m;
                uhex(flog[k + 2], num);
                for (char *p = num; *p; p++) msg[o++] = *p;
                for (m = " r="; *m; m++) msg[o++] = *m;
                uhex(flog[k + 3], num);
                for (char *p = num; *p; p++) msg[o++] = *p;
            }
        }
        msg[o++] = '\n';
        msg[o] = '\0';
        fs_write("FAULT.LOG", FS_FILE, msg, o);
    }
#endif

    user_halt();
    return 0;      /* unreachable: user_halt() never returns */
}

