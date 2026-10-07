#include "mirror.h"
#include "string.h"
#include "fs.h"

#define ATTR_TEXT   USER_COLOR(C_BLACK, C_LIGHT_GREY)
#define ATTR_TITLE  USER_COLOR(C_BLUE, C_WHITE)
#define ATTR_STATUS USER_COLOR(C_BLUE, C_WHITE)
#define ATTR_FRAME  USER_COLOR(C_BLACK, C_CYAN)
#define ATTR_FIELD  USER_COLOR(C_DARK_GREY, C_WHITE)

#define TEXT_TOP  1
#define TEXT_BOT  23
#define TEXT_ROWS (TEXT_BOT - TEXT_TOP + 1)
#define BUF_MAX   65536
#define TAB_W     4
#define CMD_MAX   50

static mirror_t screen;
static char buf[BUF_MAX];
static int len;      /* bytes used in buf */
static int pos;      /* cursor offset */
static int top;      /* offset of the first visible line */
static int offx;     /* horizontal scroll, in expanded columns */
static int dirty;    /* changed since the last load or save */
static int cmd_mode; /* ':' command line is active */
static int cmd_len;
static int quit;
static char cmd[CMD_MAX + 1];
static char fname[FS_MAX_NAME];
static char msg[72];

static char *scat(char *d, const char *s)
{
    char *r = d;
    while (*d)
        d++;
    while ((*d++ = *s++))
        ;
    return r;
}

static char *scatn(char *d, int n)
{
    char t[12];
    itoa(n, t);
    return scat(d, t);
}

static void set_msg(const char *s)
{
    int i = 0;
    while (s[i] && i < (int)sizeof msg - 1)
    {
        msg[i] = s[i];
        i++;
    }
    msg[i] = '\0';
}

/* --- buffer line helpers ------------------------------------------------ */

static int line_begin(int p)
{
    while (p > 0 && buf[p - 1] != '\n')
        p--;
    return p;
}

static int line_end(int p)
{
    while (p < len && buf[p] != '\n')
        p++;
    return p;
}

/* cursor column counting tabs as tab stops, so the caret and the drawing
   agree on where characters land */
static int col_of(int p)
{
    int c = 0;
    for (int i = line_begin(p); i < p; i++)
    {
        if (buf[i] == '\t')
            c += TAB_W - (c % TAB_W);
        else
            c++;
    }
    return c;
}

static int pos_for_col(int start, int target)
{
    int e = line_end(start);
    int c = 0;
    int i = start;
    while (i < e && c < target)
    {
        c += (buf[i] == '\t') ? TAB_W - (c % TAB_W) : 1;
        i++;
    }
    if (c > target && i > start)
        i--; /* stepped over a wide tab: sit on it, not after it */
    return i;
}

static int line_of(int p)
{
    int n = 1;
    for (int i = 0; i < p; i++)
        if (buf[i] == '\n')
            n++;
    return n;
}

/* --- editing ------------------------------------------------------------ */

static void do_insert(char c, int n)
{
    if (len + n >= BUF_MAX)
    {
        set_msg("buffer full");
        return;
    }
    for (int i = len - 1; i >= pos; i--)
        buf[i + n] = buf[i];
    for (int i = 0; i < n; i++)
        buf[pos + i] = c;
    pos += n;
    len += n;
    dirty = 1;
}

static void do_backspace(void)
{
    if (pos <= 0)
        return;
    for (int i = pos; i < len; i++)
        buf[i - 1] = buf[i];
    pos--;
    len--;
    dirty = 1;
}

/* --- view --------------------------------------------------------------- */

/* Keeps the cursor line on screen, then keeps the cursor column on screen.
   Also repairs top after an edit that removed the newline it pointed at. */
static void scroll_fix(void)
{
    int lb = line_begin(pos);

    while (lb < top)
        top = (top > 0) ? line_begin(top - 1) : 0;

    for (;;)
    {
        int last = top;
        for (int i = 0; i < TEXT_ROWS - 1; i++)
        {
            int e = line_end(last);
            if (e >= len)
            {
                last = len + 1;
                break;
            }
            last = e + 1;
        }
        if (lb <= last)
            break;
        top = line_end(top) + 1;
    }

    int c = col_of(pos);
    if (c < offx)
        offx = c;
    else if (c >= offx + MIRROR_COLS)
        offx = c - (MIRROR_COLS - 1);
    if (offx < 0)
        offx = 0;
}

static void draw_line(int row, int s)
{
    mirror_fill(&screen, 0, row, MIRROR_COLS, 1, ' ', ATTR_TEXT);
    if (s > len)
        return;

    int e = line_end(s);
    int col = 0;
    for (int i = s; i < e; i++)
    {
        char c = buf[i];
        int w = 1;

        if (c == '\t')
        {
            w = TAB_W - (col % TAB_W);
            for (int j = 0; j < w; j++)
            {
                int sx = col + j - offx;
                if (sx >= 0 && sx < MIRROR_COLS)
                    mirror_putc(&screen, sx, row, ' ', ATTR_TEXT);
            }
        }
        else
        {
            if (c < ' ' || c > '~')
                c = '.';
            int sx = col - offx;
            if (sx >= 0 && sx < MIRROR_COLS)
                mirror_putc(&screen, sx, row, c, ATTR_TEXT);
        }

        col += w;
        if (col - offx >= MIRROR_COLS)
            break;
    }
}

static void draw(void)
{
    char left[76];
    char right[26];

    strcpy(left, "lunaria edit - ");
    scat(left, fname[0] ? fname : "(untitled)");
    mirror_status(&screen, 0, left, dirty ? "modified" : "", ATTR_TITLE);

    int s = top;
    for (int row = TEXT_TOP; row <= TEXT_BOT; row++)
    {
        draw_line(row, s);
        if (s <= len)
        {
            int e = line_end(s);
            s = (e < len) ? e + 1 : len + 1;
        }
    }

    if (cmd_mode)
    {
        strcpy(left, ":");
        scat(left, cmd);
        mirror_status(&screen, 24, left, "enter run - esc cancel", ATTR_STATUS);
        mirror_cursor(2 + cmd_len, 24);
        return;
    }

    strcpy(right, "L");
    scatn(right, line_of(pos));
    scat(right, " C");
    scatn(right, col_of(pos) + 1);

    if (msg[0])
        strcpy(left, msg);
    else
        strcpy(left, "esc commands | arrows move | type to edit");

    mirror_status(&screen, 24, left, right, ATTR_STATUS);

    int row = 0;
    for (int i = top; i < pos; i++)
        if (buf[i] == '\n')
            row++;
    mirror_cursor(col_of(pos) - offx, TEXT_TOP + row);
}

/* --- files -------------------------------------------------------------- */

/* Reads name into the buffer and resets the view; returns the size or -1
   (the buffer is untouched on failure). fname is set on success only. */
static int load_into(const char *name)
{
    int n = sys_read_file(name, buf, (unsigned int)(BUF_MAX - 1));
    if (n < 0)
        return -1;
    len = n;
    pos = 0;
    top = 0;
    offx = 0;
    dirty = 0;
    strcpy(fname, name);
    return n;
}

static int save_file(const char *name)
{
    if (!name[0])
    {
        set_msg("no file name - :w <name>");
        return 0;
    }
    if ((int)strlen(name) >= FS_MAX_NAME)
    {
        set_msg("name too long (max 22)");
        return 0;
    }
    if (sys_write_file(name, buf, (unsigned int)len) != 0)
    {
        set_msg("save failed");
        return 0;
    }
    strcpy(fname, name);
    dirty = 0;
    strcpy(msg, "saved ");
    scat(msg, name);
    scat(msg, " (");
    scatn(msg, len);
    scat(msg, " bytes)");
    return 1;
}

/* --- ':' command line --------------------------------------------------- */

static void run_command(void)
{
    char *c = cmd;
    while (*c == ' ')
        c++;
    int n = (int)strlen(c);
    while (n > 0 && c[n - 1] == ' ')
        c[--n] = '\0';

    if (!*c)
        return;

    if (strcmp(c, "q") == 0)
    {
        if (dirty)
            set_msg("modified - :w saves, :q! discards");
        else
            quit = 1;
    }
    else if (strcmp(c, "q!") == 0)
        quit = 1;
    else if (strcmp(c, "w") == 0)
        save_file(fname);
    else if (strcmp(c, "wq") == 0)
    {
        if (save_file(fname))
            quit = 1;
    }
    else if (strcmp(c, "n") == 0)
    {
        if (dirty)
            set_msg("modified - :w saves first");
        else
        {
            len = 0;
            pos = 0;
            top = 0;
            offx = 0;
            dirty = 0;
            fname[0] = '\0';
            set_msg("new empty buffer");
        }
    }
    else if (c[0] == 'e' && c[1] == ' ')
    {
        char *arg = c + 1;
        while (*arg == ' ')
            arg++;
        if (dirty)
            set_msg("modified - :w saves first");
        else if ((int)strlen(arg) >= FS_MAX_NAME)
            set_msg("name too long (max 22)");
        else if (load_into(arg) < 0)
            set_msg("no such file");
        else
        {
            strcpy(msg, "loaded ");
            scatn(msg, len);
            scat(msg, " bytes");
        }
    }
    else if (c[0] == 'w' && c[1] == ' ')
    {
        char *arg = c + 1;
        while (*arg == ' ')
            arg++;
        save_file(arg);
    }
    else
        set_msg("cmds: w [file] wq q q! e <file> n");
}

static void edit_key(int k)
{
    if (k == MIRROR_KEY_ESC)
    {
        cmd_mode = 1;
        cmd_len = 0;
        cmd[0] = '\0';
    }
    else if (k == MIRROR_KEY_LEFT)
    {
        if (pos > 0)
            pos--;
    }
    else if (k == MIRROR_KEY_RIGHT)
    {
        if (pos < len)
            pos++;
    }
    else if (k == MIRROR_KEY_UP)
    {
        int lb = line_begin(pos);
        if (lb > 0)
            pos = pos_for_col(line_begin(lb - 1), col_of(pos));
    }
    else if (k == MIRROR_KEY_DOWN)
    {
        int e = line_end(pos);
        if (e < len)
            pos = pos_for_col(e + 1, col_of(pos));
    }
    else if (k == MIRROR_KEY_BACKSPACE)
        do_backspace();
    else if (k == '\t')
        do_insert(' ', TAB_W - col_of(pos) % TAB_W);
    else if (k == '\n')
        do_insert('\n', 1);
    else if (k >= 32 && k <= 126)
        do_insert((char)k, 1);
}

static void cmd_key(int k)
{
    if (k == MIRROR_KEY_ESC)
        cmd_mode = 0;
    else if (k == MIRROR_KEY_ENTER)
    {
        cmd_mode = 0;
        run_command();
    }
    else if (k == MIRROR_KEY_BACKSPACE)
    {
        if (cmd_len > 0)
            cmd[--cmd_len] = '\0';
        else
            cmd_mode = 0;
    }
    else if (k >= 32 && k <= 126 && cmd_len < CMD_MAX)
    {
        cmd[cmd_len++] = (char)k;
        cmd[cmd_len] = '\0';
    }
}

/* --- startup ------------------------------------------------------------ */

static void startup(void)
{
    char name[FS_MAX_NAME];
    int n;

    mirror_status(&screen, 0, "lunaria edit", "v0.1", ATTR_TITLE);
    mirror_box(&screen, 8, 8, 64, 7, ATTR_FRAME, " open file ");
    mirror_puts(&screen, 12, 11, "file name:", ATTR_TEXT);
    mirror_puts(&screen, 12, 16, "enter opens it; a new name starts an empty buffer", ATTR_TEXT);
    mirror_puts(&screen, 12, 17, "esc starts with an empty unnamed buffer", ATTR_TEXT);
    mirror_status(&screen, 24, "a tiny text editor for LunariaOS", "built on mirror", ATTR_STATUS);

    name[0] = '\0';
    if (mirror_input(&screen, 26, 11, 44, name, FS_MAX_NAME, ATTR_FIELD) < 0)
        name[0] = '\0';

    if (!name[0])
    {
        set_msg("new buffer - :w <name> saves it");
        return;
    }

    n = load_into(name);
    if (n < 0)
    {
        /* the name was typed on purpose: keep it as the save target */
        strcpy(fname, name);
        strcpy(msg, "new file - ");
        scat(msg, fname);
    }
    else
    {
        strcpy(msg, "loaded ");
        scatn(msg, n);
        scat(msg, " bytes");
    }
}

void _start(void)
{
    len = 0;
    pos = 0;
    top = 0;
    offx = 0;
    dirty = 0;
    cmd_mode = 0;
    cmd_len = 0;
    quit = 0;
    cmd[0] = '\0';
    fname[0] = '\0';
    msg[0] = '\0';

    mirror_init(&screen, ATTR_TEXT);
    startup();

    for (;;)
    {
        scroll_fix();
        draw();
        mirror_flush(&screen);

        int k = mirror_getkey();
        msg[0] = '\0';
        if (cmd_mode)
            cmd_key(k);
        else
            edit_key(k);
        if (quit)
            break;
    }

    mirror_clear(&screen, ATTR_TEXT);
    mirror_flush(&screen);
    sys_exit();
}
