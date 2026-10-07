#include "stdlib.h"
#include "user_api.h"
#include "string.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <math.h>

/* The heap is a first-fit free list carved out of the program break. Every
   block carries an 8-byte header and the free list is kept in address order
   so adjacent blocks can be merged when one is released.

   .bss is not part of the flat image the kernel loads, so the break the
   kernel hands us can sit inside .bss. __heap_start (prog.ld) marks the
   first byte past the image, and heap_boot() grows the break up to it
   before the first allocation. */

extern char __heap_start[];

#define MIN_ALIGN 8u
#define BLOCK_HDR sizeof(block_t)

typedef struct block
{
    unsigned int size;      /* total bytes, header included */
    struct block *next;     /* next free block, address-ascending */
} block_t;

static block_t *freelist;
static unsigned int heap_brk;
static int heap_state;      /* 0 = not booted, 1 = usable, -1 = failed */

static unsigned int align8(unsigned int n)
{
    return (n + (MIN_ALIGN - 1)) & ~(MIN_ALIGN - 1);
}

static void heap_boot(void)
{
    unsigned int cur = sys_sbrk(0);
    unsigned int want = (unsigned int)__heap_start;

    if (want > cur)
    {
        /* a refused grow returns the unchanged break */
        if (sys_sbrk(want - cur) <= cur)
        {
            heap_state = -1;
            return;
        }
    }
    heap_brk = sys_sbrk(0);
    heap_state = 1;
}

/* Grow the break and turn the new region into one free block. The new space
   is always above every existing block, so it can merge with the list tail. */
static int morecore(unsigned int need)
{
    unsigned int req = need > 4096 ? need : 4096;
    req = (req + 4095u) & ~4095u;

    unsigned int old = heap_brk;
    unsigned int got = sys_sbrk(req);
    if (got != old + req)
        return 0;
    heap_brk = got;

    block_t *b = (block_t *)old;
    b->size = req;
    b->next = 0;

    if (!freelist)
    {
        freelist = b;
    }
    else
    {
        block_t *p = freelist;
        while (p->next)
            p = p->next;
        if ((unsigned int)p + p->size == (unsigned int)b)
            p->size += b->size;
        else
            p->next = b;
    }
    return 1;
}

void *malloc(unsigned int size)
{
    if (!heap_state)
        heap_boot();
    if (heap_state < 0)
        return 0;

    unsigned int need = align8(size) + BLOCK_HDR;

    for (;;)
    {
        block_t *prev = 0;
        block_t *b = freelist;

        while (b)
        {
            if (b->size >= need)
            {
                block_t *next = b->next;

                /* split when the remainder fits a header and a payload */
                if (b->size >= need + BLOCK_HDR + MIN_ALIGN)
                {
                    block_t *rest = (block_t *)((char *)b + need);
                    rest->size = b->size - need;
                    rest->next = next;
                    b->size = need;
                    if (prev)
                        prev->next = rest;
                    else
                        freelist = rest;
                }
                else if (prev)
                {
                    prev->next = next;
                }
                else
                {
                    freelist = next;
                }
                return (char *)b + BLOCK_HDR;
            }
            prev = b;
            b = b->next;
        }

        if (!morecore(need))
            return 0;
    }
}

void free(void *ptr)
{
    if (!ptr)
        return;

    block_t *b = (block_t *)((char *)ptr - BLOCK_HDR);
    block_t *prev = 0;
    block_t *cur = freelist;

    while (cur && (unsigned int)cur < (unsigned int)b)
    {
        prev = cur;
        cur = cur->next;
    }

    b->next = cur;
    if (prev)
        prev->next = b;
    else
        freelist = b;

    /* merge with the block that follows, then with the one before */
    if (cur && (unsigned int)b + b->size == (unsigned int)cur)
    {
        b->size += cur->size;
        b->next = cur->next;
    }
    if (prev && (unsigned int)prev + prev->size == (unsigned int)b)
    {
        prev->size += b->size;
        prev->next = b->next;
    }
}

void *calloc(unsigned int count, unsigned int size)
{
    unsigned int total = count * size;

    if (count && total / count != size)
        return 0;

    void *p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, unsigned int size)
{
    if (!ptr)
        return malloc(size);
    if (!size)
    {
        free(ptr);
        return 0;
    }

    block_t *b = (block_t *)((char *)ptr - BLOCK_HDR);
    unsigned int payload = b->size - BLOCK_HDR;

    if (payload >= size)
    {
        unsigned int need = align8(size) + BLOCK_HDR;
        if (b->size >= need + BLOCK_HDR + MIN_ALIGN)
        {
            block_t *rest = (block_t *)((char *)b + need);
            rest->size = b->size - need;
            b->size = need;
            free((char *)rest + BLOCK_HDR);
        }
        return ptr;
    }

    void *n = malloc(size);
    if (!n)
        return 0;
    memcpy(n, ptr, payload);
    free(ptr);
    return n;
}

char *strdup(const char *s)
{
    unsigned int n = strlen(s) + 1;
    char *p = malloc(n);

    if (p)
        memcpy(p, s, n);
    return p;
}

char *strndup(const char *s, unsigned int n)
{
    unsigned int len = strnlen(s, n);
    char *p = malloc(len + 1);

    if (p)
    {
        memcpy(p, s, len);
        p[len] = '\0';
    }
    return p;
}

void exit(int status)
{
    (void)status;
    sys_exit();
    for (;;)
        ;
}

void abort(void)
{
    sys_exit();
    for (;;)
        ;
}

char *getenv(const char *name)
{
    (void)name;
    return 0;
}

int abs(int n)
{
    return n < 0 ? -n : n;
}

void qsort(void *base, unsigned int n, unsigned int size,
           int (*cmp)(const void *, const void *))
{
    char *a = base;

    for (unsigned int i = 1; i < n; i++)
    {
        for (unsigned int j = i; j > 0; j--)
        {
            char *x = a + j * size;
            char *y = a + (j - 1) * size;

            if (cmp(x, y) >= 0)
                break;
            for (unsigned int k = 0; k < size; k++)
            {
                char t = x[k];
                x[k] = y[k];
                y[k] = t;
            }
        }
    }
}

/* --- POSIX-flavoured wrappers over the kernel syscalls ----------------- */

int errno;
char **environ;

int open(const char *path, int flags, ...)
{
    return sys_open(path, flags);
}

int close(int fd)
{
    return sys_close(fd);
}

ssize_t read(int fd, void *buf, size_t n)
{
    return sys_read(fd, (char *)buf, (unsigned int)n);
}

ssize_t write(int fd, const void *buf, size_t n)
{
    return sys_fwrite(fd, (const char *)buf, (unsigned int)n);
}

long lseek(int fd, long off, int whence)
{
    return sys_lseek(fd, (int)off, whence);
}

int unlink(const char *path)
{
    return sys_unlink(path);
}

int access(const char *path, int mode)
{
    (void)mode;
    int fd = sys_open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    sys_close(fd);
    return 0;
}

char *getcwd(char *buf, size_t size)
{
    if (!buf)
    {
        buf = malloc((unsigned int)size);
        if (!buf)
            return 0;
    }
    if (size < 2)
    {
        errno = ERANGE;
        return 0;
    }
    buf[0] = '/';
    buf[1] = '\0';
    return buf;
}

/* No symlinks and no ".." resolution: the path is copied through unchanged. */
char *realpath(const char *path, char *resolved)
{
    char *out = resolved;

    if (!path)
        return 0;
    if (!out)
        out = malloc((unsigned int)strlen(path) + 1);
    if (!out)
        return 0;
    strcpy(out, path);
    return out;
}

long sysconf(int name)
{
    (void)name;
    return 4096;
}

int mprotect(void *addr, unsigned int len, int prot)
{
    (void)addr;
    (void)len;
    (void)prot;
    return 0;       /* user pages are already readable/writable/executable */
}

/* --- number parsing ---------------------------------------------------- */

static int digit_value(int c, int base)
{
    int v;
    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'a' && c <= 'z')
        v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z')
        v = c - 'A' + 10;
    else
        return -1;
    return v < base ? v : -1;
}

static int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r'
        || c == '\f' || c == '\v';
}

unsigned long long strtoull(const char *s, char **end, int base)
{
    const char *p = s;
    unsigned long long acc = 0;
    int neg = 0, any = 0, d;

    while (is_space((unsigned char)*p))
        p++;
    if (*p == '+' || *p == '-')
        neg = *p++ == '-';

    if ((base == 0 || base == 16) && p[0] == '0'
        && (p[1] == 'x' || p[1] == 'X') && digit_value(p[2], 16) >= 0)
    {
        p += 2;
        base = 16;
    }
    else if (base == 0 && p[0] == '0')
    {
        base = 8;
    }
    else if (base == 0)
    {
        base = 10;
    }

    while ((d = digit_value((unsigned char)*p, base)) >= 0)
    {
        acc = acc * (unsigned int)base + (unsigned int)d;
        any = 1;
        p++;
    }

    if (end)
        *end = (char *)(any ? p : s);
    return neg ? (unsigned long long)(-(long long)acc) : acc;
}

long long strtoll(const char *s, char **end, int base)
{
    return (long long)strtoull(s, end, base);
}

unsigned long strtoul(const char *s, char **end, int base)
{
    return (unsigned long)strtoull(s, end, base);
}

long strtol(const char *s, char **end, int base)
{
    return (long)strtoull(s, end, base);
}

long double ldexpl(long double x, int exp)
{
    long double r = x;

    if (exp > 0)
    {
        long double p = 2.0L;
        while (exp)
        {
            if (exp & 1)
                r *= p;
            exp >>= 1;
            if (exp)
                p *= p;
        }
    }
    else if (exp < 0)
    {
        long double p = 0.5L;
        unsigned int e = (unsigned int)(-exp);
        while (e)
        {
            if (e & 1)
                r *= p;
            e >>= 1;
            if (e)
                p *= p;
        }
    }
    return r;
}

double ldexp(double x, int exp)
{
    return (double)ldexpl((long double)x, exp);
}

float ldexpf(float x, int exp)
{
    return (float)ldexpl((long double)x, exp);
}

long double strtold(const char *s, char **end)
{
    const char *p = s;
    long double val = 0.0L;
    int neg = 0, any = 0;

    while (is_space((unsigned char)*p))
        p++;
    if (*p == '+' || *p == '-')
        neg = *p++ == '-';

    while (*p >= '0' && *p <= '9')
    {
        val = val * 10.0L + (long double)(*p - '0');
        any = 1;
        p++;
    }

    if (*p == '.')
    {
        long double scale = 0.1L;
        p++;
        while (*p >= '0' && *p <= '9')
        {
            val += (long double)(*p - '0') * scale;
            scale *= 0.1L;
            any = 1;
            p++;
        }
    }

    if (any && (*p == 'e' || *p == 'E'))
    {
        const char *e = p + 1;
        int esign = 1, eval = 0, edig = 0;

        if (*e == '+' || *e == '-')
            esign = (*e++ == '-') ? -1 : 1;
        while (*e >= '0' && *e <= '9')
        {
            if (eval < 100000000)
                eval = eval * 10 + (*e - '0');
            e++;
            edig = 1;
        }
        if (edig)
        {
            val = ldexpl(val, esign * eval);
            p = e;
        }
    }

    if (end)
        *end = (char *)(any ? p : s);
    return neg ? -val : val;
}

float strtof(const char *s, char **end)
{
    return (float)strtold(s, end);
}

double strtod(const char *s, char **end)
{
    return (double)strtold(s, end);
}

char *strerror(int errnum)
{
    (void)errnum;
    return "error";
}

/* --- time -------------------------------------------------------------- */

static struct tm tm_buf;

struct tm *localtime(const time_t *t)
{
    long long secs = t ? (long long)*t : 0;
    long long days = secs / 86400;
    long long rem = secs % 86400;

    if (rem < 0)
    {
        rem += 86400;
        days--;
    }

    tm_buf.tm_hour = (int)(rem / 3600);
    tm_buf.tm_min = (int)((rem % 3600) / 60);
    tm_buf.tm_sec = (int)(rem % 60);
    tm_buf.tm_wday = (int)((days + 4) % 7);
    if (tm_buf.tm_wday < 0)
        tm_buf.tm_wday += 7;
    tm_buf.tm_yday = 0;
    tm_buf.tm_isdst = 0;

    /* days since 1970-01-01 -> civil date (Howard Hinnant's algorithm) */
    long long z = days + 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned int doe = (unsigned int)(z - era * 146097);
    unsigned int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = (long long)yoe + era * 400;
    unsigned int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned int mp = (5 * doy + 2) / 153;
    unsigned int d = doy - (153 * mp + 2) / 5 + 1;
    unsigned int m = mp + (mp < 10 ? 3 : -9);

    tm_buf.tm_year = (int)(y + (m <= 2) - 1900);
    tm_buf.tm_mon = (int)m - 1;
    tm_buf.tm_mday = (int)d;
    return &tm_buf;
}

struct tm *gmtime(const time_t *t)
{
    return localtime(t);
}

time_t time(time_t *t)
{
    /* No wall clock yet; report the Unix epoch. */
    if (t)
        *t = 0;
    return 0;
}

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
    static const char *const month[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static const char *const wday[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    size_t n = 0;
    char tmp[16];

    for (const char *p = fmt; *p && n + 1 < max; p++)
    {
        const char *ins = 0;
        if (*p != '%')
        {
            s[n++] = *p;
            continue;
        }
        p++;
        switch (*p)
        {
            case 'Y': itoa(tm->tm_year + 1900, tmp); ins = tmp; break;
            case 'm': itoa(tm->tm_mon + 1, tmp); ins = tmp; break;
            case 'd': itoa(tm->tm_mday, tmp); ins = tmp; break;
            case 'H': itoa(tm->tm_hour, tmp); ins = tmp; break;
            case 'M': itoa(tm->tm_min, tmp); ins = tmp; break;
            case 'S': itoa(tm->tm_sec, tmp); ins = tmp; break;
            case 'b': ins = month[tm->tm_mon % 12]; break;
            case 'a': ins = wday[tm->tm_wday % 7]; break;
            case '%': ins = "%"; break;
            default: tmp[0] = '%'; tmp[1] = *p; tmp[2] = '\0'; ins = tmp; break;
        }
        for (const char *q = ins; *q && n + 1 < max; q++)
            s[n++] = *q;
    }
    if (max)
        s[n] = '\0';
    return n;
}
