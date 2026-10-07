#ifndef MIRROR_H
#define MIRROR_H

#include "user_api.h"

#define MIRROR_COLS SCREEN_COLS
#define MIRROR_ROWS SCREEN_ROWS

typedef unsigned char mirror_attr_t;

typedef struct mirror_cell
{
    char ch;
    mirror_attr_t attr;
} mirror_cell_t;

/* Mirror keeps two copies of the screen. Draw into back[], then call
   mirror_flush(): only the cells that differ from front[] (what the screen
   currently shows) are sent to the kernel, so redraws of unchanged areas
   cost nothing. */
typedef struct mirror
{
    mirror_cell_t back[MIRROR_COLS * MIRROR_ROWS];
    mirror_cell_t front[MIRROR_COLS * MIRROR_ROWS];
} mirror_t;

#define MIRROR_KEY_UP        0xE0
#define MIRROR_KEY_DOWN      0xE1
#define MIRROR_KEY_LEFT      0xE2
#define MIRROR_KEY_RIGHT     0xE3
#define MIRROR_KEY_ESC       0x1B
#define MIRROR_KEY_ENTER     '\n'
#define MIRROR_KEY_BACKSPACE '\b'

void mirror_init(mirror_t *m, mirror_attr_t attr);
void mirror_clear(mirror_t *m, mirror_attr_t attr);
void mirror_putc(mirror_t *m, int x, int y, char ch, mirror_attr_t attr);
void mirror_puts(mirror_t *m, int x, int y, const char *s, mirror_attr_t attr);
void mirror_printf(mirror_t *m, int x, int y, mirror_attr_t attr, const char *fmt, ...);
void mirror_fill(mirror_t *m, int x, int y, int w, int h, char ch, mirror_attr_t attr);
void mirror_box(mirror_t *m, int x, int y, int w, int h, mirror_attr_t attr, const char *title);
void mirror_cursor(int x, int y);
int mirror_getkey(void);
void mirror_flush(mirror_t *m);

void mirror_status(mirror_t *m, int row, const char *left, const char *right, mirror_attr_t attr);
void mirror_menu(mirror_t *m, int x, int y, int w, const char *const *items, int count,
                 int selected, mirror_attr_t attr, mirror_attr_t sel_attr);
int mirror_menu_select(mirror_t *m, int x, int y, int w, const char *const *items, int count,
                       int selected, mirror_attr_t attr, mirror_attr_t sel_attr);
int mirror_input(mirror_t *m, int x, int y, int w, char *buf, int cap, mirror_attr_t attr);
void mirror_progress(mirror_t *m, int x, int y, int w, int percent,
                     mirror_attr_t fill, mirror_attr_t attr);

#endif
