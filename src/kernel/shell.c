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
static void cmd_free(int argc, char **argv);

void shell_run_program(const char *name, int argc, char **argv);

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
    {"free", cmd_free},
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
        term_print_color(wm_current(), "usage: run <file> [args...]\n", VGA_COLOR(BLACK, RED));
        return;
    }
    shell_run_program(argv[1], argc - 1, &argv[1]);
}

/* Build the ring-3 argument block on the new process stack and return the
   initial esp. Runs with the process page directory live (after vm_attach):
   the writes land in the user window and are demand-paged by the same
   vm_fault path that serves the image load. Layout, high to low:

       argument strings
       char **argv (NULL-terminated)
       char **envp = { NULL }
       dummy return address, argc, argv    <- initial esp (cdecl entry)

   Everything sits above the initial esp, so the program's own stack grows
   down away from it. */
static unsigned int build_user_args(int argc, char **argv)
{
    unsigned int ptrs[MAX_ARGS + 1];
    unsigned int argv_ptr;
    unsigned int sp = USER_STACK;

    if (argc > MAX_ARGS)
        argc = MAX_ARGS;

    /* strings, from the top of the stack down */
    for (int i = 0; i < argc; i++)
    {
        unsigned int len = (unsigned int)strlen(argv[i]) + 1;
        sp -= len;
        for (unsigned int j = 0; j < len; j++)
            ((char *)sp)[j] = argv[i][j];
        ptrs[i] = sp;
    }
    ptrs[argc] = 0;

    sp &= ~3u;                       /* align down to 4 bytes */

    /* char **argv, NULL-terminated */
    sp -= (unsigned int)(argc + 1) * 4;
    argv_ptr = sp;
    for (int i = 0; i <= argc; i++)
        ((unsigned int *)sp)[i] = ptrs[i];

    /* char **envp = { NULL } */
    sp -= 4;
    ((unsigned int *)sp)[0] = 0;

    /* cdecl entry frame: a C _start(int argc, char **argv) finds argc at
       [esp+4] and argv at [esp+8]; [esp] is a throwaway return address. */
    sp -= 12;
    ((unsigned int *)sp)[0] = 0;
    ((unsigned int *)sp)[1] = (unsigned int)argc;
    ((unsigned int *)sp)[2] = argv_ptr;
    return sp;
}

/* Little-endian u32 read (used by the flat-binary header parse). */
static unsigned int rd32_le(const char *p)
{
    return (unsigned int)(unsigned char)p[0]
         | ((unsigned int)(unsigned char)p[1] << 8)
         | ((unsigned int)(unsigned char)p[2] << 16)
         | ((unsigned int)(unsigned char)p[3] << 24);
}

void shell_run_program(const char *name, int argc, char **argv)
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

    int size = fs_read(name, (char *)USER_BASE, USER_PROG_MAX);
    if (size < 0)
    {
        term_print_color(wm_current(), "run: not found or too big\n", VGA_COLOR(BLACK, RED));
        vm_detach();
        return;
    }

    /* tcc flat binary? 12-byte header: "LUNB" + u32 image size + u32 entry
       offset. tcc baked the image for TCC_FLAT_BASE, so copy it there and
       enter it instead of the normal USER_BASE image. */
    {
        char *img = (char *)USER_BASE;
        if (size >= 12
            && img[0] == TCC_FLAT_MAGIC0 && img[1] == TCC_FLAT_MAGIC1
            && img[2] == TCC_FLAT_MAGIC2 && img[3] == TCC_FLAT_MAGIC3)
        {
            unsigned int fsize = rd32_le(img + 4);
            unsigned int fentry = rd32_le(img + 8);
            if (fsize > (unsigned int)size - 12
                || fsize + 12u > TCC_FLAT_MAX
                || TCC_FLAT_BASE + fsize > USER_STACK
                || fentry >= fsize)
            {
                term_print_color(wm_current(), "run: bad flat binary\n", VGA_COLOR(BLACK, RED));
                vm_detach();
                return;
            }

            memcpy((void *)TCC_FLAT_BASE, img + 12, fsize);
            /* Images linked against libc.a need their heap just past the
               image: tcc anchors __heap_start at TCC_FLAT_BASE, so the
               break must sit above that (hence the USER_BASE offset). */
            vm_note_load((TCC_FLAT_BASE - USER_BASE) + fsize);
            wm_current()->prompt = 0;
            kbd_flush();
            enter_user(TCC_FLAT_BASE + fentry, build_user_args(argc, argv));
            return;
        }
    }

    vm_note_load((unsigned int)size);
    wm_current()->prompt = 0;
    kbd_flush();
    enter_user(USER_BASE, build_user_args(argc, argv));
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

/* Reports the physical frame allocator and the pager. Hand-rolled instead of
   printf since there is no formatting in the kernel yet. */
static void cmd_free(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    terminal_t *t = wm_current();
    color_t k = VGA_COLOR(BLACK, WHITE);
    color_t v = VGA_COLOR(BLACK, YELLOW);
    color_t l = VGA_COLOR(BLACK, CYAN);
    char buf[12];

    uint32_t total_f = memory_total_frames();
    uint32_t free_f = memory_free_frames();

    term_print_color(t, "  total  ", k);
    itoa(memory_total_kb(), buf);
    term_print_color(t, buf, v);
    term_print_color(t, " KB\n", k);

    term_print_color(t, "  frames ", k);
    itoa(total_f, buf);
    term_print_color(t, buf, v);
    term_print_color(t, " total, ", k);
    itoa(free_f, buf);
    term_print_color(t, buf, v);
    term_print_color(t, " free, ", k);
    itoa(total_f - free_f, buf);
    term_print_color(t, buf, v);
    term_print_color(t, " used\n", k);

    /* frames -> KB, rounding down */
    itoa((free_f * 4) / 1024, buf);
    term_print_color(t, "  heap   ", k);
    term_print_color(t, buf, v);
    term_print_color(t, " KB free\n", k);

    term_print_color(t, "  ring3  ", k);
    term_print_color(t, "0x400000-0x800000 on demand, ", l);
    itoa(vm_faults(), buf);
    term_print_color(t, buf, v);
    term_print_color(t, " faults served\n", k);
}

static void cmd_sleep(int argc, char **argv)
{
    if (argc > 1)
    {
        pit_sleep(atoi(argv[1]));
    }
}

static void cmd_df(int argc, char **argv)
{

}

void shell_run(void);

void exit_to_shell(void)
{
    /* reached from the iret in isr0x80, so the process directory is still
       the active one: hand the CPU back to the kernel map and release it */
    vm_detach();
    /* flush and drop any file handles the program left open */
    fs_close_all();
    /* a `run` command never returns to execute(), so its t->len = 0 was
       skipped: drop the consumed line before the shell reads the next one,
       or everything typed now lands after the old command's embedded NUL */
    wm_current()->len = 0;
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
