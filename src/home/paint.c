/* paint.c — text-mode drawing app for LunariaOS.
 *
 * An 80x24 canvas of coloured blocks.
 *
 *   arrows       move the '*' cursor
 *   space        toggle draw mode (DRAW paints as you move, MOVE just moves)
 *   [ and ]      cycle the 16 colours
 *   s            save the canvas to paint.bit
 *   enter        clear the canvas
 *   esc          quit
 *
 * A pixel is a space whose *background* is the chosen colour, so it shows as
 * a solid block (a coloured foreground on a space would be invisible).
 * Every cell remembers what is really on screen in view_*; the '*' cursor is
 * written through the same bookkeeping, so moving it restores the cell
 * underneath instead of leaving a trail. */

#include "user_api.h"
#include "string.h"
#include "fs.h"

#define CANV_W 80
#define CANV_H 24
#define STATUS_ROW 24
#define STATUS_W 80

#define K_ESC      0x1B
#define K_ENTER    '\n'
#define K_SPACE    ' '
#define K_UP       0xE0
#define K_DOWN     0xE1
#define K_LEFT     0xE2
#define K_RIGHT    0xE3

#define ATTR_STATUS USER_COLOR(C_BLUE, C_WHITE)
#define ATTR_CURSOR USER_COLOR(C_BLACK, C_WHITE)

#define HELP_END (STATUS_W - 9)

/* One canvas pixel: the background colour index, or 0 for empty (black). */
static unsigned char pix[CANV_H][CANV_W];

/* What is actually on screen right now, so we only touch changed cells. */
static char view_ch[CANV_H + 1][CANV_W];
static unsigned char view_attr[CANV_H + 1][CANV_W];

static char stateline[STATUS_W];
static const char *const cname[16] = {
    "black", "blue", "green", "cyan", "red", "magenta", "brown", "grey",
    "dkgrey", "lblue", "lgreen", "lcyan", "lred", "lmagenta", "yellow", "white"
};

static int cx, cy;
static int fgcol;
static int drawing;

/* Put the real content of a canvas cell on screen (not the cursor). */
static void draw_content(int x, int y)
{
    if (y < 0 || y >= CANV_H || x < 0 || x >= CANV_W)
        return;
    unsigned char col = pix[y][x];
    char ch = ' ';
    /* space with a coloured background: a solid block. empty cells stay black */
    unsigned char attr = (unsigned char)USER_COLOR(col, col);
    if (view_ch[y][x] == ch && view_attr[y][x] == attr)
        return;
    sys_putchar_at(ch, (unsigned int)x, (unsigned int)y, (unsigned int)attr);
    view_ch[y][x] = ch;
    view_attr[y][x] = attr;
}

/* Put the '*' cursor on screen and remember that this cell shows it. */
static void draw_cursor(void)
{
    sys_putchar_at('*', (unsigned int)cx, (unsigned int)cy, (unsigned int)ATTR_CURSOR);
    view_ch[cy][cx] = '*';
    view_attr[cy][cx] = (unsigned char)ATTR_CURSOR;
    sys_goto_xy((unsigned int)cx, (unsigned int)cy);
}

static int status_put(int i, const char *s, int end)
{
    for (; *s && i < end; i++)
        stateline[i] = *s++;
    return i;
}

static void status_build(void)
{
    char num[8];
    int i;
    for (i = 0; i < STATUS_W; i++)
        stateline[i] = ' ';
    i = status_put(0, drawing ? "DRAW" : "MOVE", HELP_END);
    i = status_put(i, "  col:", HELP_END);
    itoa(fgcol, num);
    i = status_put(i, num, HELP_END);
    i = status_put(i, " ", HELP_END);
    i = status_put(i, cname[fgcol], HELP_END);
    i = status_put(i, "  [] col  spc toggle  s save  enter new", HELP_END);
    status_put(HELP_END, " esc quit", STATUS_W);
}

static void flush_status(void)
{
    unsigned int a = ATTR_STATUS;
    for (int x = 0; x < STATUS_W; x++)
    {
        if (view_ch[STATUS_ROW][x] == stateline[x] && view_attr[STATUS_ROW][x] == (unsigned char)a)
            continue;
        sys_putchar_at(stateline[x], (unsigned int)x, STATUS_ROW, a);
        view_ch[STATUS_ROW][x] = stateline[x];
        view_attr[STATUS_ROW][x] = (unsigned char)a;
    }
}

static void redraw_all(void)
{
    for (int y = 0; y < CANV_H; y++)
        for (int x = 0; x < CANV_W; x++)
            draw_content(x, y);
    flush_status();
    draw_cursor();
}

static void step(int dx, int dy)
{
    int nx = cx + dx;
    int ny = cy + dy;
    if (nx < 0 || nx >= CANV_W || ny < 0 || ny >= CANV_H)
        return;

    /* restore whatever the cursor was covering */
    draw_content(cx, cy);
    cx = nx;
    cy = ny;

    if (drawing)
        pix[cy][cx] = (unsigned char)fgcol;

    draw_content(cx, cy);   /* show the cell */
    draw_cursor();          /* then put the cursor back on top */
}

static void clear_canvas(void)
{
    for (int y = 0; y < CANV_H; y++)
        for (int x = 0; x < CANV_W; x++)
            pix[y][x] = 0;
    cx = 0;
    cy = 0;
    redraw_all();
}

static void init_canvas(void)
{
    for (int y = 0; y < CANV_H + 1; y++)
        for (int x = 0; x < CANV_W; x++)
        {
            view_ch[y][x] = ' ';
            view_attr[y][x] = 0;
        }
    for (int y = 0; y < CANV_H; y++)
        for (int x = 0; x < CANV_W; x++)
            pix[y][x] = 0;
    cx = 0;
    cy = 0;
    fgcol = C_LIGHT_GREY;
    drawing = 0;
}

/* Simple .bit format: "BIT\1", u16 width, u16 height, then one byte of
   colour per pixel (row-major). */
static void save_bit(void)
{
    unsigned char buf[8 + CANV_W * CANV_H];
    int p = 0;
    buf[p++] = 'B';
    buf[p++] = 'I';
    buf[p++] = 'T';
    buf[p++] = 1;
    buf[p++] = (unsigned char)(CANV_W & 0xFF);
    buf[p++] = (unsigned char)((CANV_W >> 8) & 0xFF);
    buf[p++] = (unsigned char)(CANV_H & 0xFF);
    buf[p++] = (unsigned char)((CANV_H >> 8) & 0xFF);
    for (int y = 0; y < CANV_H; y++)
        for (int x = 0; x < CANV_W; x++)
            buf[p++] = pix[y][x];
    sys_write_file("paint.bit", (const char *)buf, (unsigned int)p);
}

static void set_mode(void)
{
    status_build();
    flush_status();
    draw_cursor();
}

void _start(void)
{
    init_canvas();
    sys_clear();
    status_build();
    redraw_all();

    for (;;)
    {
        int k = sys_getc();
        switch (k)
        {
        case K_UP:    step(0, -1); break;
        case K_DOWN:  step(0, 1);  break;
        case K_LEFT:  step(-1, 0); break;
        case K_RIGHT: step(1, 0);  break;

        case K_SPACE:
            drawing = !drawing;
            set_mode();
            break;

        case '[':
            fgcol = (fgcol + 15) % 16;
            set_mode();
            break;

        case ']':
            fgcol = (fgcol + 1) % 16;
            set_mode();
            break;

        case 's':
        case 'S':
            save_bit();
            draw_cursor();
            break;

        case K_ENTER:
            clear_canvas();
            break;

        case K_ESC:
            sys_clear();
            sys_exit();
            break;

        default:
            draw_cursor();
            break;
        }
    }
}