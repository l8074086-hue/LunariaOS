#ifndef VGA_H
#define VGA_H

typedef unsigned char color_t;

void vga_init(void);
void clear(color_t color);
void putchar(char c, color_t color);
void print(const char *str, color_t color);
void bkspc(void);
void block_show(void);
void block_hide(void);
void cursor_up(void);
void cursor_down(void);
void cursor_left(void);
void cursor_right(void);

#define VGA_COLOR(bg, fg) (((bg) << 4) | (fg))
#define BLACK 0x0
#define BLUE 0x1
#define GREEN 0x2
#define CYAN 0x3
#define RED 0x4
#define MAGENTA 0x5
#define BROWN 0x6
#define LIGHT_GRAY 0x7
#define DARK_GRAY 0x8
#define LIGHT_BLUE 0x9
#define LIGHT_GREEN 0xA
#define LIGHT_CYAN 0xB
#define LIGHT_RED 0xC
#define LIGHT_MAGENTA 0xD
#define YELLOW 0xE
#define WHITE 0xF

#endif
