#include "stdio.h"
#include "string.h"

void _start(void)
{
    char buf[1024];

    print("hello from ring 3!\n");

    if (sys_write_file("demo.txt", "file contents\n", 14) == 0)
        print("wrote demo.txt\n");

    int n = sys_read_file("demo.txt", buf, sizeof buf);
    if (n >= 0)
        sys_write(buf, (unsigned int)n);

    sys_putchar_at('@', 40, 12, USER_COLOR(C_BLACK, C_GREEN));

    sys_getc();
    sys_exit();
}
