#include "mirror.h"
#include "string.h"
#include <stdarg.h>

/* CP437 single-line box drawing; these glyphs are in the VGA text font */
#define BOX_TL    '\xDA'
#define BOX_TR    '\xBF'
#define BOX_BL    '\xC0'
#define BOX_BR    '\xD9'
#define BOX_H     '\xC4'
#define BOX_V     '\xB3'
#define BOX_FILL  '\xDB'
#define BOX_SHADE '\xB0'

static void emit(mirror_t *m, int *cx, int *cy, int startx, char ch, mirror_attr_t attr)
{
    if (ch == '\n')
    {
        *cx = startx;
        (*cy)++;
        return;
    }
    mirror_putc(m, *cx, *cy, ch, attr);
    (*cx)++;
}

/* Writes s starting at (cx, cy). '\n' returns to startx on the next row.
   maxw limits how many columns are drawn (>= 0) or, when negative, only the
   screen edge clips. */
static void put_sn(mirror_t *m, int *cx, int *cy, int startx, int maxw,
                   const char *s, mirror_attr_t attr)
{
    if (!s)
        return;
    for (; *s; s++)
    {
        if (maxw >= 0 && *cx - startx >= maxw)
            return;
        emit(m, cx, cy, startx, *s, attr);
    }
}

static void hex(unsigned int v, char *buf)
{
    char tmp[8];
    int n = 0;

    do
    {
        int d = v & 0xF;
        tmp[n++] = d < 10 ? (char)('0' + d) : (char)('A' + d - 10);
        v >>= 4;
    } while (v);

    for (int i = 0; i < n; i++)
        buf[i] = tmp[n - 1 - i];
    buf[n] = '\0';
}

void mirror_init(mirror_t *m, mirror_attr_t attr)
{
    /* Wipe the terminal with one syscall instead of repainting it cell by
       cell. sys_clear fills with black-background spaces; when our blanks
       share that background they are already on screen (the foreground of
       a space is invisible), so the first flush only sends drawn cells. */
    int matches_clear = ((attr >> 4) == 0);

    for (int i = 0; i < MIRROR_COLS * MIRROR_ROWS; i++)
    {
        m->back[i].ch = ' ';
        m->back[i].attr = attr;
        if (matches_clear)
            m->front[i] = m->back[i];
        else
        {
            m->front[i].ch = 0;
            m->front[i].attr = 0;
        }
    }
    sys_clear();
    /* park the cursor in the corner so it never lands in the middle of a box */
    mirror_cursor(MIRROR_COLS - 1, MIRROR_ROWS - 1);
}

void mirror_clear(mirror_t *m, mirror_attr_t attr)
{
    for (int i = 0; i < MIRROR_COLS * MIRROR_ROWS; i++)
    {
        m->back[i].ch = ' ';
        m->back[i].attr = attr;
    }
}

void mirror_putc(mirror_t *m, int x, int y, char ch, mirror_attr_t attr)
{
    if (x < 0 || y < 0 || x >= MIRROR_COLS || y >= MIRROR_ROWS)
        return;
    int i = y * MIRROR_COLS + x;
    m->back[i].ch = ch;
    m->back[i].attr = attr;
}

void mirror_puts(mirror_t *m, int x, int y, const char *s, mirror_attr_t attr)
{
    int cx = x, cy = y;
    put_sn(m, &cx, &cy, x, -1, s, attr);
}

void mirror_printf(mirror_t *m, int x, int y, mirror_attr_t attr, const char *fmt, ...)
{
    va_list ap;
    char buf[16];
    int cx = x, cy = y;

    va_start(ap, fmt);
    for (const char *p = fmt; *p; p++)
    {
        if (*p != '%' || !p[1])
        {
            emit(m, &cx, &cy, x, *p, attr);
            continue;
        }
        p++;
        switch (*p)
        {
            case 's':
                put_sn(m, &cx, &cy, x, -1, va_arg(ap, const char *), attr);
                break;
            case 'c':
                emit(m, &cx, &cy, x, (char)va_arg(ap, int), attr);
                break;
            case 'd':
                itoa(va_arg(ap, int), buf);
                put_sn(m, &cx, &cy, x, -1, buf, attr);
                break;
            case 'x':
                hex(va_arg(ap, unsigned int), buf);
                put_sn(m, &cx, &cy, x, -1, buf, attr);
                break;
            case '%':
                emit(m, &cx, &cy, x, '%', attr);
                break;
            default:
                emit(m, &cx, &cy, x, '%', attr);
                emit(m, &cx, &cy, x, *p, attr);
                break;
        }
    }
    va_end(ap);
}

void mirror_fill(mirror_t *m, int x, int y, int w, int h, char ch, mirror_attr_t attr)
{
    if (w <= 0 || h <= 0)
        return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > MIRROR_COLS ? MIRROR_COLS : x + w;
    int y1 = y + h > MIRROR_ROWS ? MIRROR_ROWS : y + h;

    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            mirror_putc(m, xx, yy, ch, attr);
}

void mirror_box(mirror_t *m, int x, int y, int w, int h, mirror_attr_t attr, const char *title)
{
    if (w < 2 || h < 2)
        return;

    for (int i = 0; i < w; i++)
    {
        mirror_putc(m, x + i, y, BOX_H, attr);
        mirror_putc(m, x + i, y + h - 1, BOX_H, attr);
    }
    for (int i = 1; i < h - 1; i++)
    {
        mirror_putc(m, x, y + i, BOX_V, attr);
        mirror_putc(m, x + w - 1, y + i, BOX_V, attr);
    }
    mirror_putc(m, x, y, BOX_TL, attr);
    mirror_putc(m, x + w - 1, y, BOX_TR, attr);
    mirror_putc(m, x, y + h - 1, BOX_BL, attr);
    mirror_putc(m, x + w - 1, y + h - 1, BOX_BR, attr);

    if (title && w > 4)
    {
        int cx = x + 2, cy = y;
        put_sn(m, &cx, &cy, x + 2, w - 4, title, attr);
    }
}

void mirror_cursor(int x, int y)
{
    sys_goto_xy((unsigned int)x, (unsigned int)y);
}

int mirror_getkey(void)
{
    return sys_getc();
}

void mirror_flush(mirror_t *m)
{
    /* A fully blank screen with a black background is exactly what
       sys_clear produces: send it as one syscall instead of one per cell.
       The clear also homes the cursor. */
    int blank = 1;
    for (int i = 0; i < MIRROR_COLS * MIRROR_ROWS; i++)
    {
        if (m->back[i].ch != ' ' || (m->back[i].attr >> 4) != 0)
        {
            blank = 0;
            break;
        }
    }
    if (blank)
    {
        sys_clear();
        for (int i = 0; i < MIRROR_COLS * MIRROR_ROWS; i++)
            m->front[i] = m->back[i];
        return;
    }

    for (int y = 0; y < MIRROR_ROWS; y++)
    {
        for (int x = 0; x < MIRROR_COLS; x++)
        {
            int i = y * MIRROR_COLS + x;
            if (m->back[i].ch == m->front[i].ch && m->back[i].attr == m->front[i].attr)
                continue;
            sys_putchar_at(m->back[i].ch, (unsigned int)x, (unsigned int)y, m->back[i].attr);
            m->front[i] = m->back[i];
        }
    }
}

void mirror_status(mirror_t *m, int row, const char *left, const char *right, mirror_attr_t attr)
{
    if (row < 0 || row >= MIRROR_ROWS)
        return;
    mirror_fill(m, 0, row, MIRROR_COLS, 1, ' ', attr);

    int cx = 1, cy = row;
    put_sn(m, &cx, &cy, 1, -1, left, attr);

    if (right && *right)
    {
        int rx = MIRROR_COLS - 1 - strlen(right);
        if (rx < 1)
            rx = 1;
        cx = rx;
        cy = row;
        put_sn(m, &cx, &cy, rx, MIRROR_COLS - rx - 1, right, attr);
    }
}

void mirror_menu(mirror_t *m, int x, int y, int w, const char *const *items, int count,
                 int selected, mirror_attr_t attr, mirror_attr_t sel_attr)
{
    for (int i = 0; i < count; i++)
    {
        int ry = y + i;
        if (ry >= MIRROR_ROWS)
            break;
        mirror_attr_t ra = (i == selected) ? sel_attr : attr;
        mirror_fill(m, x, ry, w, 1, ' ', ra);
        mirror_putc(m, x, ry, (i == selected) ? '>' : ' ', ra);
        if (w >= 3)
        {
            int cx = x + 2, cy = ry;
            put_sn(m, &cx, &cy, x + 2, w - 2, items[i], ra);
        }
    }
}

int mirror_menu_select(mirror_t *m, int x, int y, int w, const char *const *items, int count,
                       int selected, mirror_attr_t attr, mirror_attr_t sel_attr)
{
    if (count <= 0)
        return -1;
    if (selected < 0 || selected >= count)
        selected = 0;

    for (;;)
    {
        mirror_menu(m, x, y, w, items, count, selected, attr, sel_attr);
        mirror_flush(m);

        int k = mirror_getkey();
        if (k == MIRROR_KEY_UP)
            selected = (selected + count - 1) % count;
        else if (k == MIRROR_KEY_DOWN)
            selected = (selected + 1) % count;
        else if (k == MIRROR_KEY_ENTER)
            return selected;
        else if (k == MIRROR_KEY_ESC)
            return -1;
    }
}

int mirror_input(mirror_t *m, int x, int y, int w, char *buf, int cap, mirror_attr_t attr)
{
    if (cap <= 0 || w <= 0)
        return -1;

    int len = (int)strlen(buf);
    if (len > cap - 1)
        len = cap - 1;
    int cur = len;

    for (;;)
    {
        /* horizontal scroll so the caret always stays inside the field */
        int off = cur >= w ? cur - w + 1 : 0;
        mirror_fill(m, x, y, w, 1, ' ', attr);
        for (int i = off; i < len && i - off < w; i++)
            mirror_putc(m, x + i - off, y, buf[i], attr);
        mirror_cursor(x + cur - off, y);
        mirror_flush(m);

        int k = mirror_getkey();
        if (k == MIRROR_KEY_ENTER)
            break;
        if (k == MIRROR_KEY_ESC)
        {
            mirror_cursor(MIRROR_COLS - 1, MIRROR_ROWS - 1);
            return -1;
        }
        if (k == MIRROR_KEY_BACKSPACE)
        {
            if (cur > 0)
            {
                for (int i = cur - 1; i < len - 1; i++)
                    buf[i] = buf[i + 1];
                cur--;
                len--;
                buf[len] = '\0';
            }
        }
        else if (k == MIRROR_KEY_LEFT)
        {
            if (cur > 0)
                cur--;
        }
        else if (k == MIRROR_KEY_RIGHT)
        {
            if (cur < len)
                cur++;
        }
        else if (k >= 32 && k <= 126 && len < cap - 1)
        {
            for (int i = len; i > cur; i--)
                buf[i] = buf[i - 1];
            buf[cur] = (char)k;
            cur++;
            len++;
            buf[len] = '\0';
        }
    }

    mirror_cursor(MIRROR_COLS - 1, MIRROR_ROWS - 1);
    return 0;
}

void mirror_progress(mirror_t *m, int x, int y, int w, int percent,
                     mirror_attr_t fill, mirror_attr_t attr)
{
    if (w <= 0)
        return;
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;

    int n = (percent * w) / 100;
    mirror_fill(m, x, y, w, 1, BOX_SHADE, attr);
    mirror_fill(m, x, y, n, 1, BOX_FILL, fill);
}
