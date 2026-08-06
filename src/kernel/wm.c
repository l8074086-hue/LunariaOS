#include "string.h"
#include "vga.h"
#include "wm.h"

terminal_t terms[MAX_TERMS];
int current_term = 0;

void wm_init(void)
{
  for (int i = 0; i < MAX_TERMS; i++)
  {
    terms[i].color = VGA_COLOR(BLACK, WHITE);
    terms[i].len = 0;
    terms[i].prompt = 0;
    term_clear(&terms[i]);
  }
  current_term = 0;
  wm_blit(&terms[0]);
}

void wm_switch(int idx)
{
  if (idx < 0 || idx >= MAX_TERMS)
    return;
  current_term = idx;
  wm_blit(&terms[current_term]);
}

terminal_t *wm_current(void)
{
  return &terms[current_term];
}

void term_putchar(terminal_t *t, char c, color_t color)
{
  if (c == '\n')
  {
    t->cursor_x = 0;
    t->cursor_y++;
  }
  else
  {
    int index = t->cursor_y * 80 + t->cursor_x;

    t->cells[index * 2] = c;
    t->cells[index * 2 + 1] = color;

    t->cursor_x++;
    if (t->cursor_x >= 80)
    {
      t->cursor_x = 0;
      t->cursor_y++;
    }
  }
  if (t->cursor_y > 24)
    term_scroll(t);
}

void term_scroll(terminal_t *t)
{
  for (int i = 0; i < 24 * 80 * 2; i++)
    t->cells[i] = t->cells[i + 80 * 2];
  for (int i = 24 * 80 * 2; i < 25 * 80 * 2; i += 2)
  {
    t->cells[i] = ' ';
    t->cells[i + 1] = t->color;
  }
  t->cursor_y = 24;
}

void term_putchar_at(terminal_t *t, char c, int x, int y, color_t color)
{
  if (c == '\n')
    return;
  int index = y * 80 + x;
  t->cells[index * 2] = c;
  t->cells[index * 2 + 1] = color;
}

void term_clear(terminal_t *t)
{
  for (int i = 0; i < 80 * 25 * 2; i += 2)
  {
    t->cells[i] = ' ';
    t->cells[i + 1] = t->color;
  }
  t->cursor_x = 0;
  t->cursor_y = 0;
}

void term_print(terminal_t *t, const char *str)
{
  while (*str)
    term_putchar(t, *str++, t->color);
}

void term_print_color(terminal_t *t, const char *str, color_t color)
{
  while (*str)
    term_putchar(t, *str++, color);
}

void term_backspace(terminal_t *t)
{
  if (t->cursor_x == 0 && t->cursor_y == 0)
    return;
  if (t->cursor_x > 0)
    t->cursor_x--;
  else
  {
    t->cursor_y--;
    t->cursor_x = 79;
  }

  int index = t->cursor_y * 80 + t->cursor_x;
  t->cells[index * 2] = ' ';
}

void term_cursor_up(terminal_t *t)
{
  if (t->cursor_y > 0)
    t->cursor_y--;
}

void term_cursor_down(terminal_t *t)
{
  if (t->cursor_y < 24)
    t->cursor_y++;
}

void term_cursor_left(terminal_t *t)
{
  if (t->cursor_x > 0)
    t->cursor_x--;
}

void term_cursor_right(terminal_t *t)
{
  if (t->cursor_x < 79)
    t->cursor_x++;
}

void wm_blit(const terminal_t *t)
{
  char *vga = (char *)0xb8000;
  memcpy(vga, t->cells, 80 * 25 * 2);
  if (t->cursor_y <= 24 && t->cursor_x >= 0 && t->cursor_x <= 79)
  {
    int idx = (t->cursor_y * 80 + t->cursor_x) * 2 + 1;
    unsigned char attr = (unsigned char)vga[idx];
    vga[idx] = (char)((attr >> 4) | (attr << 4));
  }
}
