#ifndef WM_H
#define WM_H

#define MAX_TERMS 10
#define TERM_BUF_SIZE 128

typedef struct terminal
{
  int cursor_x, cursor_y;
  char cells[80 * 25 * 2];
  color_t color;
  char buf[TERM_BUF_SIZE];
  int len;
  int prompt;
} terminal_t;

extern terminal_t terms[MAX_TERMS];
extern int current_term;

void wm_init(void);
void wm_switch(int idx);
terminal_t *wm_current(void);

void term_putchar(terminal_t *t, char c, color_t color);
void term_scroll(terminal_t *t);
void term_putchar_at(terminal_t *t, char c, int x, int y, color_t color);
void term_clear(terminal_t *t);
void term_print(terminal_t *t, const char *str);
void term_print_color(terminal_t *t, const char *str, color_t color);
void term_backspace(terminal_t *t);
void term_cursor_up(terminal_t *t);
void term_cursor_down(terminal_t *t);
void term_cursor_left(terminal_t *t);
void term_cursor_right(terminal_t *t);
void wm_blit(const terminal_t *t);

#endif
