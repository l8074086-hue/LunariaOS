#include "vga.h"
#include "keyboard.h"
#include "ata.h"
#include "port.h"
#include "string.h"
#include "fs.h"
#include "wm.h"
#include "user.h"
#include "pit.h"
#include "memory.h"

#define CMD_BUF_SIZE 128
#define MAX_ARGS 16

struct command
{
    const char *name;
    void (*fn)(int argc, char **argv);
};

static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_read(int argc, char **argv);
static void cmd_exit(int argc, char **argv);
static void cmd_halt(int argc, char **argv);
static void cmd_ls(int argc, char **argv);
static void cmd_cat(int argc, char **argv);
static void cmd_rm(int argc, char **argv);
static void cmd_mkdir(int argc, char **argv);
static void cmd_run(int argc, char **argv);
static void cmd_uptime(int argc, char **argv);
static void cmd_sleep(int argc, char **argv);
static void cmd_banner(int argc, char **argv);

void shell_run_program(const char *name);

static const struct command commands[] = {
    { "help", cmd_help },
    { "clear", cmd_clear },
    { "echo", cmd_echo },
    { "read", cmd_read },
    { "exit", cmd_exit },
    { "halt", cmd_halt },
    { "ls", cmd_ls },
    { "cat", cmd_cat },
    { "rm", cmd_rm },
    { "mkdir", cmd_mkdir },
    { "run", cmd_run },
    { "uptime", cmd_uptime },
    { "sleep", cmd_sleep },
    { "banner", cmd_banner},
};

static const int command_count = sizeof(commands) / sizeof(commands[0]);

static void cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    term_print_color(wm_current(), "Available Commands:\n  help\n  clear\n  echo\n  read\n  ls\n  cat\n  rm\n  mkdir\n  run\n  uptime\n  sleep\n  halt\n  exit\n", VGA_COLOR(BLACK, WHITE));

}

static void cmd_clear(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    term_clear(wm_current());
    wm_current()->prompt = 0;
}

static void cmd_echo(int argc, char **argv)
{
    if (argc >= 2) {
        for (int i = 1; i < argc; i++) {
            if (i > 1)
                term_print_color(wm_current(), " ", VGA_COLOR(BLACK, WHITE));
            term_print_color(wm_current(), argv[i], VGA_COLOR(BLACK, WHITE));
        }
        term_print_color(wm_current(), "\n", VGA_COLOR(BLACK, WHITE));
    }
}

static void cmd_mkdir(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (argc >= 2)
    {
        for (int i = 1; i < argc; i++)
        {
            if (fs_mkdir(argv[i]) != 0)
            {
                term_print_color(wm_current(), "mkdir: failed:", VGA_COLOR(BLACK, RED));
                term_print_color(wm_current(), argv[i], VGA_COLOR(BLACK, RED));
                term_print_color(wm_current(), "\n", VGA_COLOR(BLACK, RED));
            }
        }
    }
}

static void cmd_run(int argc, char **argv)
{
    if (argc < 2)
    {
        term_print_color(wm_current(), "usage: run <file>\n", VGA_COLOR(BLACK, RED));
        return;
    }
    shell_run_program(argv[1]);
}

void shell_run_program(const char *name)
{
    /* Build a private address space and switch to it *before* loading, so
       the program image lands in pages this process will keep and a stale
       one from a previous run cannot leak in. */
    uint32_t dir = vm_create();
    if (!dir)
    {
        term_print_color(wm_current(), "run: out of memory\n", VGA_COLOR(BLACK, RED));
        return;
    }
    vm_attach(dir);

#ifdef PAGER_TEST
    {   /* TEMP: faulting copy, then show what vm_fault captured */
        char b[12];
        unsigned int vals[5];
        vm_dbg[4] = vm_dbg[5] = vm_dbg[6] = vm_dbg[7] = 0xAAAAAAAA;
        memcpy((void *)(USER_BASE + 0x2000), (void *)0x7E00, 512);
        vals[0] = vm_dbg[0];
        vals[1] = vm_dbg[1];
        vals[2] = vm_dbg[2];
        vals[3] = vm_dbg[3];
        vals[4] = *(volatile unsigned int *)(USER_BASE + 0x2000);
        terminal_t *t = wm_current();
        for (int k = 0; k < 5; k++)
        {
            for (int i = 7; i >= 0; i--)
            {
                unsigned int d = vals[k] & 0xF;
                b[i] = d < 10 ? (char)('0' + d) : (char)('A' + d - 10);
                vals[k] >>= 4;
            }
            b[8] = 0;
            term_print_color(t, k == 4 ? " dst0=" : " v=", VGA_COLOR(BLACK, CYAN));
            term_print_color(t, b, VGA_COLOR(BLACK, CYAN));
        }
        term_print_color(t, "\n", VGA_COLOR(BLACK, CYAN));
        wm_blit(t);
    }
#endif

    int size = fs_read(name, (char *)USER_BASE, USER_PROG_MAX);
    if (size < 0)
    {
        term_print_color(wm_current(), "run: not found or too big\n", VGA_COLOR(BLACK, RED));
        vm_detach();
        return;
    }

    vm_note_load((unsigned int)size);
    wm_current()->prompt = 0;
    kbd_flush();
    enter_user(USER_BASE, USER_STACK);
}

static void cmd_uptime(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char buf[12];
    itoa(pit_uptime_sec(), buf);
    term_print_color(wm_current(), "uptime: ", VGA_COLOR(BLACK,WHITE));
    term_print_color(wm_current(), buf, VGA_COLOR(BLACK, YELLOW));
    term_print_color(wm_current(), " seconds\n", VGA_COLOR(BLACK,WHITE));
}

static void cmd_sleep(int argc, char **argv)
{
    if (argc > 1)
    {
        pit_sleep(atoi(argv[1]));
    }
}

void shell_run(void);

void exit_to_shell(void)
{
    /* reached from the iret in isr0x80, so the process directory is still
       the active one: hand the CPU back to the kernel map and release it */
    vm_detach();
    shell_run();
}

static void cmd_read(int argc, char **argv)
{
    if (argc < 2)
    {
        term_print_color(wm_current(), "usage: read <lba>\n", VGA_COLOR(BLACK, RED));
        return;
    }
    char sector[513];
    int lba = atoi(argv[1]);
    if (ata_read_sectors(lba, 1, sector) == 0)
    {
        sector[512] = '\0';
        term_print_color(wm_current(), sector, VGA_COLOR(BLACK, WHITE));
        term_print_color(wm_current(), "\n", VGA_COLOR(BLACK, WHITE));
    }
    else
        term_print_color(wm_current(), "read failed\n", VGA_COLOR(BLACK, RED));
}

static void cmd_halt(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    __asm__ volatile("cli");
    for (;;)
        __asm__ volatile("hlt");
}

static void cmd_exit(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    __asm__ volatile("cli");
    outw(0x604, 0x2000);
    for (;;)
        __asm__ volatile("hlt");
}

static void cmd_ls(int argc, char **argv)
{
    if (argc >= 2)
    {
        if (fs_ls(argv[1]) != 0)
            term_print_color(wm_current(), "ls: not found\n", VGA_COLOR(BLACK, RED));
    }
    else
        fs_ls("");
}

static void cmd_banner(int argc, char **argv)
{
    if (argc == 1)
    {
        (void)argv;
        print_banner(GREEN);
    }
}

static void cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        term_print_color(wm_current(), "usage: cat <file>\n", VGA_COLOR(BLACK, RED));
        return;
    }
    if (fs_cat(argv[1]) != 0)
        term_print_color(wm_current(), "cat: not found\n", VGA_COLOR(BLACK, RED));
}

static void cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        term_print_color(wm_current(), "usage: rm <file>\n", VGA_COLOR(BLACK, RED));
        return;
    }
    if (fs_delete(argv[1]) != 0)
        term_print_color(wm_current(), "rm: not found\n", VGA_COLOR(BLACK, RED));
}

static int parse_line(char *line, char **argv)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < MAX_ARGS)
    {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ')
            p++;
        if (*p)
            *p++ = '\0';
    }

    return argc;
}

static void execute(char *line)
{
    char *argv[MAX_ARGS];
    int argc = parse_line(line, argv);

    if (argc == 0)
        return;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], ">") == 0)
        {
            if (i + 1 >= argc)
            {
                term_print_color(wm_current(), "usage: <cmd> ... > <file>\n", VGA_COLOR(BLACK, RED));
                return;
            }

            char content[CMD_BUF_SIZE];
            int len = 0;
            for (int j = 1; j < i; j++)
            {
                if (j > 1)
                    content[len++] = ' ';
                for (char *p = argv[j]; *p; p++)
                    if (*p == '\\' && p[1] != '\0')
                    {
                        p++;
                        switch (*p)
                        {
                            case 'n': content[len++] = '\n'; break;
                            case 't': content[len++] = '\t'; break;
                            case '\\': content[len++] = '\\'; break;
                            default: content[len++] = '\\'; content[len++] = *p; break;
                        }
                    }
                    else
                        content[len++] = *p;
            }
            content[len] = '\0';

            if (fs_write(argv[i + 1], FS_FILE, content, len) == 0)
            {
                term_print_color(wm_current(), "wrote ", VGA_COLOR(BLACK, GREEN));
                term_print_color(wm_current(), argv[i + 1], VGA_COLOR(BLACK, GREEN));
                term_print_color(wm_current(), "\n", VGA_COLOR(BLACK, GREEN));
            }
            else
                term_print_color(wm_current(), "write failed\n", VGA_COLOR(BLACK, RED));
            return;
        }
    }

    for (int i = 0; i < command_count; i++)
    {
        if (strcmp(argv[0], commands[i].name) == 0)
        {
            commands[i].fn(argc, argv);
            return;
        }
    }

    term_print_color(wm_current(), argv[0], VGA_COLOR(BLACK, WHITE));
    term_print_color(wm_current(), ": command not found\n", VGA_COLOR(BLACK, RED));
}

static void switch_workspace(int idx, terminal_t **tp)
{
    wm_switch(idx);
    *tp = wm_current();
    if (!(*tp)->prompt)
    {
        term_print_color(*tp, "sherenity > ", VGA_COLOR(BLACK, CYAN));
        (*tp)->prompt = 1;
    }
    wm_blit(*tp);
}

void shell_run(void)
{
    for (;;)
    {
        terminal_t *t = wm_current();

        if (!t->prompt)
        {
            term_print_color(t, "sherenity > ", VGA_COLOR(BLACK, CYAN));
            t->prompt = 1;
            wm_blit(t);
        }

        for (;;)
        {
            int c = kbd_getc();
            int handled = 0;

            if (kbd_mods() & MOD_CTRL)
            {
                if (c >= '1' && c <= '9')
                {
                    switch_workspace(c - '1', &t);
                    handled = 1;
                }
                else if (c == '0')
                {
                    switch_workspace(9, &t);
                    handled = 1;
                }
                else if (c >= 'a' && c <= 'z')
                {
                    int ws = -1;
                    switch (c)
                    {
                        case 'q': ws = 0; break;
                        case 'w': ws = 1; break;
                        case 'e': ws = 2; break;
                        case 'r': ws = 3; break;
                        case 't': ws = 4; break;
                        case 'y': ws = 5; break;
                        case 'u': ws = 6; break;
                        case 'o': ws = 7; break;
                        case 'p': ws = 8; break;
                        case 'a': ws = 9; break;
                    }
                    if (ws >= 0)
                    {
                        switch_workspace(ws, &t);
                        handled = 1;
                    }
                }
            }

            if (handled)
                continue;

            if (c == '\n')
            {
                term_putchar(t, '\n', VGA_COLOR(BLACK, WHITE));
                t->buf[t->len] = '\0';
                execute(t->buf);
                t->len = 0;
                t->prompt = 0;
                wm_blit(t);
                break;
            }
            else if (c == '\b')
            {
                if (t->len > 0)
                {
                    t->len--;
                    term_backspace(t);
                }
            }
            else if (c >= ' ' && c <= '~')
            {
                if (t->len < TERM_BUF_SIZE - 1)
                {
                    t->buf[t->len++] = (char)c;
                    term_putchar(t, c, VGA_COLOR(BLACK, WHITE));
                }
            }
            wm_blit(t);
        }
    }
}
