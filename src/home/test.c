#include "stdio.h"
#include "string.h"

void _start(void)
{
    char buf[12];
    int num1 = 1;
    int num2 = 3;
    int result = num1 + num2;

    print("Hello, World!\n");
    itoa(result, buf);
    print("result = ");
    print(buf);
    print("\n");

    sys_exit();
}
