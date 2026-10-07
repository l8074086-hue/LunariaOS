#include "stdio.h"
#include "string.h"
#include "user_api.h"

static char buf[256];
static int failures;

static void check(const char *name, int ok)
{
    print(ok ? "PASS " : "FAIL ");
    print(name);
    print("\n");
    if (!ok)
        failures++;
}

void _start(void)
{
    const char *data = "Hello, handle world";
    int len = strlen(data);

    sys_unlink("IO.TEST");

    check("open missing fails", sys_open("IO.TEST", O_RDONLY) == -1);

    int fd = sys_open("IO.TEST", O_CREAT | O_WRONLY | O_TRUNC);
    check("create", fd >= 3);

    check("write counts", sys_fwrite(fd, data, 6) == 6 &&
                         sys_fwrite(fd, data + 6, len - 6) == len - 6);

    check("seek set", sys_lseek(fd, 0, SEEK_SET) == 0);
    int r = sys_read(fd, buf, len);
    check("read from write handle", r == len && memcmp(buf, data, len) == 0);

    check("seek end", sys_lseek(fd, 0, SEEK_END) == len);
    check("seek end -5", sys_lseek(fd, -5, SEEK_END) == len - 5);
    r = sys_read(fd, buf, 5);
    check("tail read", r == 5 && memcmp(buf, "world", 5) == 0);

    check("close", sys_close(fd) == 0);

    /* reopen read-only: now the bytes must come back off the disk */
    fd = sys_open("IO.TEST", O_RDONLY);
    check("reopen", fd >= 3);
    r = sys_read(fd, buf, len);
    check("disk read", r == len && memcmp(buf, data, len) == 0);
    check("read at eof", sys_read(fd, buf, 8) == 0);
    check("rewind", sys_lseek(fd, 0, SEEK_SET) == 0);
    r = sys_read(fd, buf, 5);
    check("partial read", r == 5 && memcmp(buf, "Hello", 5) == 0);
    sys_close(fd);

    /* O_APPEND tacks the new bytes on the end */
    fd = sys_open("IO.TEST", O_WRONLY | O_APPEND);
    check("append open", fd >= 3);
    check("append write", sys_fwrite(fd, "!", 1) == 1);
    sys_close(fd);
    fd = sys_open("IO.TEST", O_RDONLY);
    r = sys_read(fd, buf, sizeof buf);
    check("append persisted", r == len + 1 && buf[len] == '!');
    sys_close(fd);

    /* the whole-file syscall sees the same bytes */
    r = sys_read_file("IO.TEST", buf, sizeof buf);
    check("read_file agrees", r == len + 1 && memcmp(buf, data, len) == 0);

    check("bad fd", sys_read(99, buf, 4) == -1);
    check("close bad fd", sys_close(99) == -1);

    check("unlink", sys_unlink("IO.TEST") == 0);
    check("gone", sys_open("IO.TEST", O_RDONLY) == -1);

    print(failures ? "FILETEST FAILED\n" : "FILETEST ALL PASS\n");
    sys_exit();
}
