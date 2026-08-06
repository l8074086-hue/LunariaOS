#include "stdio.h"
#include "string.h"

void _start(void)
{
    sys_clear();

    for (unsigned int x = 0; x < SCREEN_COLS; x++)
    {
        sys_putchar_at('-', x, 0, USER_COLOR(C_BLACK, C_GREEN));
        sys_putchar_at('-', x, SCREEN_ROWS - 1, USER_COLOR(C_BLACK, C_GREEN));
    }
    for (unsigned int y = 0; y < SCREEN_ROWS; y++)
    {
        sys_putchar_at('|', 0, y, USER_COLOR(C_BLACK, C_GREEN));
        sys_putchar_at('|', SCREEN_COLS - 1, y, USER_COLOR(C_BLACK, C_GREEN));
    }

    sys_goto_xy(36, 6);
    print("TUI DEMO");
    sys_goto_xy(28, 11);
    print("press a key to continue");
    sys_goto_xy(40, 11);

    int c = sys_getc();

    sys_clear();
    for (unsigned int x = 0; x < SCREEN_COLS; x++)
    {
        sys_putchar_at(' ', x, 0, USER_COLOR(C_CYAN, C_BLACK));
        sys_putchar_at(' ', x, SCREEN_ROWS - 1, USER_COLOR(C_CYAN, C_BLACK));
    }
    for (unsigned int y = 0; y < SCREEN_ROWS; y++)
    {
        sys_putchar_at(' ', 0, y, USER_COLOR(C_CYAN, C_BLACK));
        sys_putchar_at(' ', SCREEN_COLS - 1, y, USER_COLOR(C_CYAN, C_BLACK));
    }

    sys_goto_xy(30, 11);
    print("you pressed: ");
    sys_write((const char *)&c, 1);
    sys_goto_xy(30, 12);
    char num[12];
    itoa(c, num);
    print("value ");
    print(num);

    sys_goto_xy(30, 14);
    print("press any key to exit");
    sys_getc();

    sys_clear();
    sys_exit();
}
