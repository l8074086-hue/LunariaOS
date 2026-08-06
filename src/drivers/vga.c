#include "vga.h"
#include "port.h"

static volatile char *VGA = (char *)0xb8000;
static int cursor_x = 0;
static int cursor_y = 0;
static int block_x = -1;
static int block_y = -1;

void vga_init(void)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

static void flip_cell(int x, int y)
{
    int i = y * 80 + x;
    unsigned char attr = VGA[i * 2 + 1];
    VGA[i * 2 + 1] = (attr >> 4) | (attr << 4);
}

void block_hide(void)
{
    if (block_y < 0)
        return;
    flip_cell(block_x, block_y);
    block_x = -1;
    block_y = -1;
}

void block_show(void)
{
    flip_cell(cursor_x, cursor_y);
    block_x = cursor_x;
    block_y = cursor_y;
}

void cursor_up(void)
{
    if (cursor_y > 0)
        cursor_y--;
}

void cursor_down(void)
{
    if (cursor_y < 24)
        cursor_y++;
}

void cursor_left(void)
{
    if (cursor_x > 0)
        cursor_x--;
}

void cursor_right(void)
{
    if (cursor_x < 79)
        cursor_x++;
}

static void scroll(void)
{
    for (int i = 0; i < 24 * 80 * 2; i++)
        VGA[i] = VGA[i + 80 * 2];
    for (int i = 24 * 80 * 2; i < 25 * 80 * 2; i += 2)
    {
        VGA[i] = ' ';
        VGA[i + 1] = 0x0f;
    }
    cursor_y = 24;
}

void putchar(char c, color_t color)
{
    if (c == '\n')
    {
        cursor_x = 0;
        cursor_y++;
    }
    else
    {
        int index = cursor_y * 80 + cursor_x;

        VGA[index * 2] = c;
        VGA[index * 2 + 1] = color;

        cursor_x++;
        if (cursor_x >= 80)
        {
            cursor_x = 0;
            cursor_y++;
        }
    }

    if (cursor_y > 24)
        scroll();
}

void putchar_at(char c, int x, int y, color_t color)
{
    if (c != '\n')
    {
        int index = y * 80 + x;

        VGA[index * 2] = c;
        VGA[index * 2 + 1] = color;
    }
}

void clear(color_t color)
{
    for (int i = 0; i < 80 * 25 * 2; i += 2)
    {
        VGA[i] = ' ';
        VGA[i + 1] = color;
    }
    cursor_x = 0;
    cursor_y = 0;
}

void print(const char *str, color_t color)
{
    while (*str)
    {
        putchar(*str, color);
        str++;
    }
}

void bkspc(void) 
{
    if (cursor_x == 0 && cursor_y == 0)
    {
        return;
    }
    if (cursor_x > 0)
    {
        cursor_x--;
    }
    else 
    {
        cursor_y--;
        cursor_x = 79;
    }

    int index = cursor_y * 80 + cursor_x;
    putchar_at(' ', cursor_x, cursor_y, VGA[index * 2 + 1]);
}

void showCursor(void) 
{

}
