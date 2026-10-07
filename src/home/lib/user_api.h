#ifndef USER_API_H
#define USER_API_H

#define SYS_WRITE 1
#define SYS_EXIT  2
#define SYS_GETC  3
#define SYS_READ_FILE 4
#define SYS_WRITE_FILE 5
#define SYS_LIST  6
#define SYS_EXIT_TO 7
#define SYS_CLEAR  8
#define SYS_PUTCHAR_AT 9
#define SYS_GOTO_XY 10
#define SYS_SBRK 11
#define SYS_OPEN 12
#define SYS_READ 13
#define SYS_FWRITE 14
#define SYS_SEEK 15
#define SYS_CLOSE 16
#define SYS_UNLINK 17

/* open() flags and lseek() whence; keep in step with src/headers/fs.h */
#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR   0x0004
#define O_CREAT  0x0008
#define O_TRUNC  0x0010
#define O_APPEND 0x0020

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* tcc flat binaries: where a compiled `-o` image is baked and loaded.
   Keep in step with src/headers/user.h. Images are also capped by the
   disk's 64K writable-file buffer (see fs.c), which is far smaller than
   the room above TCC_FLAT_BASE, so that cap is the real limit. */
#define TCC_FLAT_MAGIC0 'L'
#define TCC_FLAT_MAGIC1 'U'
#define TCC_FLAT_MAGIC2 'N'
#define TCC_FLAT_MAGIC3 'B'
#define TCC_FLAT_BASE   0x680000
#define TCC_FLAT_MAX    0x10000

#define EXIT_SHELL    0
#define EXIT_HALT     1
#define EXIT_POWEROFF 2
#define EXIT_REBOOT   3

#define SCREEN_COLS 80
#define SCREEN_ROWS 25

#define C_BLACK 0
#define C_BLUE 1
#define C_GREEN 2
#define C_CYAN 3
#define C_RED 4
#define C_MAGENTA 5
#define C_BROWN 6
#define C_LIGHT_GREY 7
#define C_DARK_GREY 8
#define C_LIGHT_BLUE 9
#define C_LIGHT_GREEN 10
#define C_LIGHT_CYAN 11
#define C_LIGHT_RED 12
#define C_LIGHT_MAGENTA 13
#define C_YELLOW 14
#define C_WHITE 15

#define USER_COLOR(bg, fg) (((bg) << 4) | (fg))

static inline void sys_write(const char *buf, unsigned int len)
{
    __asm__ volatile("int $0x80" : : "a"(SYS_WRITE), "b"(buf), "c"(len) : "memory");
}

static inline void sys_exit(void)
{
    __asm__ volatile("int $0x80" : : "a"(SYS_EXIT));
}

static inline void sys_exit_to(int mode)
{
    __asm__ volatile("int $0x80" : : "a"(SYS_EXIT_TO), "b"(mode));
}

static inline void sys_clear(void)
{
    __asm__ volatile("int $0x80" : : "a"(SYS_CLEAR));
}

static inline void sys_putchar_at(char c, unsigned int x, unsigned int y, unsigned int color)
{
    __asm__ volatile("int $0x80" : : "a"(SYS_PUTCHAR_AT), "b"(c), "c"(x), "d"(y), "S"(color));
}

static inline void sys_goto_xy(unsigned int x, unsigned int y)
{
    __asm__ volatile("int $0x80" : : "a"(SYS_GOTO_XY), "b"(x), "c"(y));
}

static inline void sys_print(const char *s)
{
    unsigned int len = 0;
    while (s[len])
        len++;
    sys_write(s, len);
}

static inline int sys_getc(void)
{
    int c;
    __asm__ volatile("int $0x80" : "=a"(c) : "a"(SYS_GETC));
    return c;
}

static inline int sys_read_file(const char *name, char *buf, unsigned int max)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_READ_FILE), "b"(name), "c"(buf), "d"(max));
    return r;
}

static inline int sys_write_file(const char *name, const char *buf, unsigned int len)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_WRITE_FILE), "b"(name), "c"(buf), "d"(len));
    return r;
}

static inline int sys_list(const char *path, char *buf, unsigned int max)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_LIST), "b"(path), "c"(buf), "d"(max));
    return r;
}

/* Grow the heap by inc bytes and return the new break. The pages behind it
   are not backed until the program actually touches them. */
static inline unsigned int sys_sbrk(unsigned int inc)
{
    unsigned int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_SBRK), "b"(inc));
    return r;
}

/* File handles. The kernel hands back a small integer >= 3, or -1. */
static inline int sys_open(const char *name, int flags)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_OPEN), "b"(name), "c"(flags) : "memory");
    return r;
}

static inline int sys_read(int fd, char *buf, unsigned int n)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(n) : "memory");
    return r;
}

static inline int sys_fwrite(int fd, const char *buf, unsigned int n)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_FWRITE), "b"(fd), "c"(buf), "d"(n) : "memory");
    return r;
}

static inline int sys_lseek(int fd, int off, int whence)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_SEEK), "b"(fd), "c"(off), "d"(whence));
    return r;
}

static inline int sys_close(int fd)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_CLOSE), "b"(fd));
    return r;
}

static inline int sys_unlink(const char *name)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SYS_UNLINK), "b"(name));
    return r;
}

#endif
