/* paint.c — a text-mode drawing app for LunariaOS.
 *
 * An 80x24 canvas driven straight from the keyboard:
 *
 *   arrows       the pen is always down while moving, so dragging draws
 *   letters etc. pick a new brush (and stamp it under the cursor)
 *   space        stamp the current brush
 *   [ and ]      cycle the 16 foreground colours
 *   backspace    toggle the eraser (drag with it to scrub a line)
 *   enter        clear the canvas
 *   esc          leave
 *
 * Drawn through the plain syscall API. The artwork lives in pix[][]; a
 * second view[][] remembers what the screen already shows, so each frame
 * only the cells that changed are sent to the kernel. There are no libc
 * calls beyond the header-only wrappers, so the file also compiles cleanly
 * on-device with `run tcc paint.c -o out`. */

#include "user_api.h"
#include "string.h"

#define CANV_W 80
#define CANV_H 24            /* rows 0..23 are the canvas */
#define STATUS_ROW 24        /* bottom row: status + help */
#define STATUS_W 80

#define K_ESC      0x1B
#define K_BACKSPACE '\b'
#define K_ENTER    '\n'
#define K_UP       0xE0
#define K_DOWN     0xE1
#define K_LEFT     0xE2
#define K_RIGHT    0xE3

#define ATTR_STATUS USER_COLOR(C_BLUE, C_WHITE)
#define ATTR_ERASE  USER_COLOR(C_RED, C_WHITE)

#define HELP_END (STATUS_W - 9)   /* the right edge is reserved for " esc quit" */

typedef struct
{
    char ch;
    unsigned char fg;        /* foreground colour; the background stays black */
} cell_t;

static cell_t pix[CANV_H][CANV_W];   /* the artwork */
static cell_t view[CANV_H][CANV_W];  /* what the screen currently shows */

static char stateline[STATUS_W];
static const char *const cname[16] = {
    "black", "blue", "green", "cyan", "red", "magenta", "brown", "grey",
    "dkgrey", "lblue", "lgreen", "lcyan", "lred", "lmagenta", "yellow", "white"
};

static int cx, cy;           /* pen position */
static char brush;           /* glyph the pen stamps */
static int fgcol;            /* 0..15 */
static int erasing;          /* backspace toggles this */

/* --- helpers ------------------------------------------------------------ */

static int status_put(int i, const char *s, int end)
{
    for (; *s && i < end; i++)
        stateline[i] = *s++;
    return i;
}

/* Rebuild the status bar. Content is clamped to HELP_END so the quit hint
   at the right edge is always visible. */
static void status_build(void)
{
    char num[8];
    int i = 0;

    for (i = 0; i < STATUS_W; i++)
        stateline[i] = ' ';
    i = 0;

    i = status_put(i, erasing ? "ERASE " : "paint ", HELP_END);
    i = status_put(i, "brush:", HELP_END);
    if (i < HELP_END)
        stateline[i++] = brush;
    i = status_put(i, "  col:", HELP_END);
    itoa(fgcol, num);
    i = status_put(i, num, HELP_END);
    i = status_put(i, " ", HELP_END);
    i = status_put(i, cname[fgcol], HELP_END);
    if (erasing)
        i = status_put(i, "  [] col  arrows erase  bs paint  enter new", HELP_END);
    else
        i = status_put(i, "  [] col  arrows draw  bs erase  enter new", HELP_END);
    status_put(HELP_END, " esc quit", STATUS_W);
}

static void status_render(void)
{
    unsigned int a = erasing ? ATTR_ERASE : ATTR_STATUS;
    for (int x = 0; x < STATUS_W; x++)
        sys_putchar_at(stateline[x], (unsigned int)x, STATUS_ROW, a);
}

/* --- canvas -------------------------------------------------------------- */

/* Make cell (x, y) match pix[][] on the real screen, sending a syscall only
   when the cell changed. The pen cell is inverted so it is always visible,
   even over an empty (black-on-black) cell. */
static void sync_cell(int x, int y)
{
    char c = pix[y][x].ch;
    unsigned int a = USER_COLOR(C_BLACK, pix[y][x].fg);

    if (x == cx && y == cy)
        a = USER_COLOR(pix[y][x].fg ? pix[y][x].fg : C_WHITE, C_BLACK);

    if (c == view[y][x].ch && (unsigned char)a == view[y][x].fg)
        return;
    sys_putchar_at(c, (unsigned int)x, (unsigned int)y, a);
    view[y][x].ch = c;
    view[y][x].fg = (unsigned char)a;
}

static void stamp(int x, int y)
{
    if (erasing)
    {
        pix[y][x].ch = ' ';
        pix[y][x].fg = 0;
    }
    else
    {
        pix[y][x].ch = brush;
        pix[y][x].fg = (unsigned char)fgcol;
    }
    sync_cell(x, y);
}

/* Move the pen, painting the destination cell on the way (etch-a-sketch). */
static void step(int dx, int dy)
{
    int nx = cx + dx;
    int ny = cy + dy;

    if (nx < 0 || nx >= CANV_W || ny < 0 || ny >= CANV_H)
        return;
    sync_cell(cx, cy);       /* drop the pen highlight from the old cell */
    cx = nx;
    cy = ny;
    stamp(cx, cy);
}

static void clear_canvas(void)
{
    cx = 0;
    cy = 0;
    for (int y = 0; y < CANV_H; y++)
        for (int x = 0; x < CANV_W; x++)
        {
            pix[y][x].ch = ' ';
            pix[y][x].fg = 0;
            sync_cell(x, y); /* only previously-painted cells get sent */
        }
}

static void init_canvas(void)
{
    for (int y = 0; y < CANV_H; y++)
        for (int x = 0; x < CANV_W; x++)
        {
            pix[y][x].ch = ' ';
            pix[y][x].fg = 0;
            view[y][x].ch = ' ';
            view[y][x].fg = 0;
        }
    cx = 0;
    cy = 0;
    brush = '#';
    fgcol = C_LIGHT_GREY;
    erasing = 0;
}

/* --- main ---------------------------------------------------------------- */

void _start(void)
{
    init_canvas();
    sys_clear();             /* now view[][] correctly describes the screen */

    status_build();
    status_render();

    for (;;)
    {
        int k = sys_getc();

        switch (k)
        {
        case K_UP:
            step(0, -1);
            break;
        case K_DOWN:
            step(0, 1);
            break;
        case K_LEFT:
            step(-1, 0);
            break;
        case K_RIGHT:
            step(1, 0);
            break;

        case K_BACKSPACE:
            erasing = !erasing;
            if (erasing)
                stamp(cx, cy);   /* instant feedback under the pen */
            status_build();
            status_render();
            break;

        case K_ENTER:
            clear_canvas();
            status_build();
            status_render();
            break;

        case '[':
            fgcol = (fgcol + 15) % 16;   /* one back */
            status_build();
            status_render();
            break;

        case ']':
            fgcol = (fgcol + 1) % 16;    /* one forward */
            status_build();
            status_render();
            break;

        case K_ESC:
            sys_clear();
            sys_exit();
            break;

        default:
            if (k >= 32 && k <= 126)
            {
                erasing = 0;
                brush = (char)k;
                stamp(cx, cy);
                status_build();
                status_render();
            }
            break;
        }

        sys_goto_xy((unsigned int)cx, (unsigned int)cy);   /* park the caret on the pen */
    }
}