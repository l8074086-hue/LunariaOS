#include "stdio.h"
#include "string.h"

void _start(void)
{
    char buf[1024];
    int n;

    print("hello from ring 3!\n");

    print("-- list root --\n");
    n = sys_list("", buf, sizeof buf);
    if (n >= 0)
        sys_write(buf, (unsigned int)n);
    else
        print("list failed\n");

    print("-- write test.txt --\n");
    n = sys_write_file("test.txt", "created from ring 3!\n", 20);
    if (n == 0)
        print("wrote ok\n");
    else
        print("write failed\n");

    print("-- read test.txt --\n");
    n = sys_read_file("test.txt", buf, sizeof buf);
    if (n >= 0)
        sys_write(buf, (unsigned int)n);
    else
        print("read failed\n");

    print("-- getc --\npress a key: ");
    int c = sys_getc();
    buf[0] = (char)c;
    buf[1] = '\n';
    buf[2] = '\0';
    print("got: ");
    sys_write(buf, 3);
    print("(value ");
    char num[12];
    itoa(c, num);
    print(num);
    print(")\n");

    sys_exit();
}
