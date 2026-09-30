#include "stdio.h"
#include "string.h"

void _start(void)
{
    print("PROBE: ring3 active\n");

    if (sys_write_file("PROBE.LOG", "PROBE: wrote from ring3\n", 24) == 0)
        print("probe wrote ok\n");
    else
        print("probe write failed\n");

    /* ring-3 read of a kernel (supervisor) page must #PF */
    volatile char c = *(volatile char *)0xB8000;
    (void)c;
    print("PROBE: KERNEL MEMORY READABLE - isolation broken\n");
    sys_exit();
}