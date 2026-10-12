/* paint.c — text-mode drawing app for LunariaOS (improved) */

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

typedef struct
{
    unsigned char ch;
    unsigned char fg;
} cell_t;

static cell_t pix[CANV_H][CANV_W];
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

static int status_put(int i, const char *s, int end)
{
    for (; *s && i < end; i++)
        stateline[i] = *s++;
    return i;
}

static void status_build(void)
{
    char num[8];
    int i = 0;
    for (i = 0; i < STATUS_W; i++)
        stateline[i] = ' ';
    i = 0;
    i = status_put(i, drawing ? "DRAW" : "MOVE", HELP_END);
    i = status_put(i, "  col:", HELP_END);
    itoa(fgcol, num);
    i = status_put(i, num, HELP_END);
    i = status_put(i, " ", HELP_END);
    i = status_put(i, cname[fgcol], HELP_END);
    i = status_put(i, "  [] col  spc toggle  s save  enter new", HELP_END);
    status_put(HELP_END, " esc quit", STATUS_W);
}

static void draw_cell_content(int x, int y)
{
    if (y < 0 || y >= CANV_H || x < 0 || x >= CANV_W)
        return;
    char ch = (char)pix[y][x].ch;
    unsigned char attr = (unsigned char)USER_COLOR(C_BLACK, pix[y][x].fg);
    if (view_ch[y][x] == ch && view_attr[y][x] == attr)
        return;
    sys_putchar_at(ch, (unsigned int)x, (unsigned int)y, (unsigned int)attr);
    view_ch[y][x] = ch;
    view_attr[y][x] = attr;
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
            draw_cell_content(x, y);
    flush_status();
    sys_putchar_at('*', (unsigned int)cx, (unsigned int)cy, (unsigned int)ATTR_CURSOR);
    sys_goto_xy((unsigned int)cx, (unsigned int)cy);
}

static void step(int dx, int dy)
{
    int nx = cx + dx;
    int ny = cy + dy;
    if (nx < 0 || nx >= CANV_W || ny < 0 || ny >= CANV_H)
        return;
    draw_cell_content(cx, cy);
    cx = nx;
    cy = ny;
    if (drawing)
    {
        pix[cy][cx].ch = ' ';
        pix[cy][cx].fg = (unsigned char)fgcol;
    }
    draw_cell_content(cx, cy);
    sys_putchar_at('*', (unsigned int)cx, (unsigned int)cy, (unsigned int)ATTR_CURSOR);
    sys_goto_xy((unsigned int)cx, (unsigned int)cy);
}

static void clear_canvas(void)
{
    cx = 0;
    cy = 0;
    for (int y = 0; y < CANV_H; y++)
    {
        for (int x = 0; x < CANV_W; x++)
        {
            pix[y][x].ch = ' ';
            pix[y][x].fg = 0;
        }
    }
    redraw_all();
}

static void init_canvas(void)
{
    for (int y = 0; y < CANV_H + 1; y++)
    {
        for (int x = 0; x < CANV_W; x++)
        {
            view_ch[y][x] = ' ';
            view_attr[y][x] = 0;
        }
    }
    for (int y = 0; y < CANV_H; y++)
    {
        for (int x = 0; x < CANV_W; x++)
        {
            pix[y][x].ch = ' ';
            pix[y][x].fg = 0;
        }
    }
    cx = 0; cy = 0; fgcol = C_LIGHT_GREY; drawing = 0;
}

static void save_bit(void)
{
    unsigned char buf[4 + 2 + 2 + CANV_W * CANV_H * 2];
    int p = 0;
    buf[p++] = 'B'; buf[p++] = 'I'; buf[p++] = 'T'; buf[p++] = 1;
    buf[p++] = (unsigned char)(CANV_W & 0xFF); buf[p++] = (unsigned char)((CANV_W >> 8) & 0xFF);
    buf[p++] = (unsigned char)(CANV_H & 0xFF); buf[p++] = (unsigned char)((CANV_H >> 8) & 0xFF);
    for (int y = 0; y < CANV_H; y++)
    {
        for (int x = 0; x < CANV_W; x++)
        {
            buf[p++] = pix[y][x].ch;
            buf[p++] = pix[y][x].fg;
        }
    }
    sys_write_file("paint.bit", (const char *)buf, (unsigned int)p);
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
        case K_SPACE:
            drawing = !drawing;
            status_build();
            flush_status();
            sys_putchar_at('*', (unsigned int)cx, (unsigned int)cy, (unsigned int)ATTR_CURSOR);
            sys_goto_xy((unsigned int)cx, (unsigned int)cy);
            break;
        case '[':
            fgcol = (fgcol + 15) % 16;
            status_build();
            flush_status();
            sys_putchar_at('*', (unsigned int)cx, (unsigned int)cy, (unsigned int)ATTR_CURSOR);
            sys_goto_xy((unsigned int)cx, (unsigned int)cy);
            break;
        case ']':
            fgcol = (fgcol + 1) % 16;
            status_build();
            flush_status();
            sys_putchar_at('*', (unsigned int)cx, (unsigned int)cy, (unsigned int)ATTR_CURSOR);
            sys_goto_xy((unsigned int)cx, (unsigned int)cy);
            break;
        case 's':
        case 'S':
            save_bit();
            sys_putchar_at('*', (unsigned int)cx, (unsigned int)cy, (unsigned int)ATTR_CURSOR);
            sys_goto_xy((unsigned int)cx, (unsigned int)cy);
            break;
        case K_ENTER:
            clear_canvas();
            break;
        case K_ESC:
            sys_clear();
            sys_exit();
            break;
        default:
            sys_putchar_at('*', (unsigned int)cx, (unsigned int)cy, (unsigned int)ATTR_CURSOR);
            sys_goto_xy((unsigned int)cx, (unsigned int)cy);
            break;
        }
    }
}
