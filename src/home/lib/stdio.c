#include "stdio.h"
#include "stdlib.h"
#include "string.h"

/* A FILE is a thin wrapper over a kernel file handle. File streams are
   unbuffered (the kernel already buffers writable files); stdin/stdout/stderr
   are console streams that go straight to the syscalls. */

struct FILE
{
    int fd;         /* kernel handle, or -1 for a console stream */
    int kind;       /* 0 = file, 1 = console input, 2 = console output */
    int eof;
    int error;
    int ungot;      /* one pushed-back byte, or -1 */
};

static struct FILE console_in  = { -1, 1, 0, 0, -1 };
static struct FILE console_out = { -1, 2, 0, 0, -1 };
static struct FILE console_err = { -1, 2, 0, 0, -1 };

FILE *stdin = &console_in;
FILE *stdout = &console_out;
FILE *stderr = &console_err;

FILE *fopen(const char *path, const char *mode)
{
    int plus = 0;
    for (const char *m = mode; *m; m++)
        if (*m == '+')
            plus = 1;

    int flags;
    switch (mode[0])
    {
        case 'r': flags = plus ? O_RDWR : O_RDONLY; break;
        case 'w': flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC; break;
        case 'a': flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND; break;
        default: return 0;
    }

    int fd = sys_open(path, flags);
    if (fd < 0)
        return 0;

    FILE *f = malloc(sizeof *f);
    if (!f)
    {
        sys_close(fd);
        return 0;
    }
    memset(f, 0, sizeof *f);
    f->fd = fd;
    f->ungot = -1;
    return f;
}

/* Translate an fopen-style mode string into kernel open flags, or -1. */
static int mode_flags(const char *mode)
{
    int plus = 0;
    for (const char *m = mode; *m; m++)
        if (*m == '+')
            plus = 1;

    switch (mode[0])
    {
        case 'r': return plus ? O_RDWR : O_RDONLY;
        case 'w': return (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
        case 'a': return (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
        default: return -1;
    }
}

FILE *fdopen(int fd, const char *mode)
{
    (void)mode;
    if (fd < 0)
        return 0;

    FILE *f = malloc(sizeof *f);
    if (!f)
        return 0;
    memset(f, 0, sizeof *f);
    f->fd = fd;
    f->ungot = -1;
    return f;
}

FILE *freopen(const char *path, const char *mode, FILE *f)
{
    if (!f || f->kind != 0)
        return 0;       /* the standard streams cannot be re-targeted */

    if (path)
    {
        int flags = mode_flags(mode);
        if (flags < 0)
            return 0;
        if (f->fd >= 0)
            sys_close(f->fd);
        int fd = sys_open(path, flags);
        if (fd < 0)
        {
            f->fd = -1;
            return 0;
        }
        f->fd = fd;
    }

    f->eof = 0;
    f->error = 0;
    f->ungot = -1;
    return f;
}

int fileno(FILE *f)
{
    return f ? f->fd : -1;
}

int fclose(FILE *f)
{
    if (!f)
        return EOF;
    if (f->kind != 0)
        return 0;               /* the standard streams stay open */
    int r = sys_close(f->fd);
    free(f);
    return r == 0 ? 0 : EOF;
}

int fgetc(FILE *f)
{
    if (!f)
        return EOF;
    if (f->ungot >= 0)
    {
        int c = f->ungot;
        f->ungot = -1;
        return c;
    }
    if (f->kind == 1)
        return sys_getc();
    if (f->kind != 0)
        return EOF;

    unsigned char c;
    int r = sys_read(f->fd, (char *)&c, 1);
    if (r <= 0)
    {
        if (r == 0)
            f->eof = 1;
        else
            f->error = 1;
        return EOF;
    }
    return c;
}

int getc(FILE *f) { return fgetc(f); }
int getchar(void) { return fgetc(stdin); }

int fputc(int c, FILE *f)
{
    if (!f)
        return EOF;
    unsigned char b = (unsigned char)c;

    if (f->kind == 2)
    {
        sys_write((const char *)&b, 1);
        return b;
    }
    if (f->kind != 0)
        return EOF;

    if (sys_fwrite(f->fd, (const char *)&b, 1) != 1)
    {
        f->error = 1;
        return EOF;
    }
    return b;
}

int putc(int c, FILE *f) { return fputc(c, f); }

int ungetc(int c, FILE *f)
{
    if (!f || c == EOF)
        return EOF;
    f->ungot = (unsigned char)c;
    f->eof = 0;
    return (unsigned char)c;
}

unsigned int fread(void *ptr, unsigned int size, unsigned int nmemb, FILE *f)
{
    if (!f || size == 0 || nmemb == 0)
        return 0;
    unsigned int total = size * nmemb;
    if (total / size != nmemb)
    {
        f->error = 1;
        return 0;
    }

    char *p = ptr;
    unsigned int got = 0;
    while (got < total)
    {
        int c = fgetc(f);
        if (c == EOF)
            break;
        p[got++] = (char)c;
    }
    return got / size;
}

unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nmemb, FILE *f)
{
    if (!f || size == 0 || nmemb == 0)
        return 0;
    unsigned int total = size * nmemb;
    if (total / size != nmemb)
    {
        f->error = 1;
        return 0;
    }

    const char *p = ptr;
    if (f->kind == 2)
    {
        sys_write(p, total);
        return nmemb;
    }
    if (f->kind != 0)
        return 0;

    unsigned int done = 0;
    while (done < total)
    {
        int r = sys_fwrite(f->fd, p + done, total - done);
        if (r <= 0)
        {
            f->error = 1;
            break;
        }
        done += (unsigned int)r;
    }
    return done / size;
}

int fseek(FILE *f, int off, int whence)
{
    if (!f || f->kind != 0)
        return -1;
    if (sys_lseek(f->fd, off, whence) < 0)
    {
        f->error = 1;
        return -1;
    }
    f->eof = 0;
    f->ungot = -1;
    return 0;
}

int ftell(FILE *f)
{
    if (!f || f->kind != 0)
        return -1;
    return sys_lseek(f->fd, 0, SEEK_CUR);
}

void rewind(FILE *f)
{
    fseek(f, 0, SEEK_SET);
    clearerr(f);
}

char *fgets(char *s, int n, FILE *f)
{
    if (!s || n <= 0)
        return 0;
    int i = 0;
    while (i < n - 1)
    {
        int c = fgetc(f);
        if (c == EOF)
            break;
        s[i++] = (char)c;
        if (c == '\n')
            break;
    }
    s[i] = '\0';
    return i == 0 ? 0 : s;
}

int fputs(const char *s, FILE *f)
{
    unsigned int n = strlen(s);
    return fwrite(s, 1, n, f) == n ? 0 : EOF;
}

int puts(const char *s)
{
    if (fputs(s, stdout) == EOF)
        return EOF;
    return fputc('\n', stdout) == EOF ? EOF : 0;
}

int feof(FILE *f) { return f ? f->eof : 0; }
int ferror(FILE *f) { return f ? f->error : 0; }
void clearerr(FILE *f)
{
    if (f)
    {
        f->eof = 0;
        f->error = 0;
    }
}

int fflush(FILE *f)
{
    (void)f;
    return 0;       /* every write already reached the kernel handle */
}

int remove(const char *path)
{
    return sys_unlink(path) == 0 ? 0 : -1;
}

/* --- formatted output -------------------------------------------------- */

typedef void (*emit_t)(void *ctx, char c);

struct out_ctx
{
    FILE *f;
    char *buf;
    unsigned int cap;
    unsigned int len;
};

static void emit_file(void *ctx, char c)
{
    struct out_ctx *o = ctx;
    fputc(c, o->f);
    o->len++;
}

static void emit_buf(void *ctx, char c)
{
    struct out_ctx *o = ctx;
    if (o->len + 1 < o->cap)
        o->buf[o->len] = c;
    o->len++;
}

static void put_str(emit_t emit, void *ctx, const char *s, int n)
{
    for (int i = 0; i < n && s[i]; i++)
        emit(ctx, s[i]);
}

static void put_pad(emit_t emit, void *ctx, char c, int n)
{
    while (n-- > 0)
        emit(ctx, c);
}

static void put_uint(emit_t emit, void *ctx, unsigned long long v, int base,
                     int upper, int width, int zero, int left, int alt)
{
    char tmp[32];
    int n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (v == 0 && base == 8 && alt)
        tmp[n++] = '0';
    do
    {
        tmp[n++] = digits[v % (unsigned int)base];
        v /= (unsigned int)base;
    } while (v);

    int prefix = (alt && base == 16) ? 2 : 0;
    int total = n + prefix;

    if (!left && !zero)
        put_pad(emit, ctx, ' ', width - total);
    if (prefix)
    {
        emit(ctx, '0');
        emit(ctx, upper ? 'X' : 'x');
    }
    if (!left && zero)
        put_pad(emit, ctx, '0', width - total);
    while (n > 0)
        emit(ctx, tmp[--n]);
    if (left)
        put_pad(emit, ctx, ' ', width - total);
}

static int do_format(emit_t emit, void *ctx, const char *fmt, va_list ap)
{
    struct out_ctx *o = ctx;
    o->len = 0;

    for (const char *p = fmt; *p; p++)
    {
        if (*p != '%')
        {
            emit(ctx, *p);
            continue;
        }
        p++;
        if (*p == '%')
        {
            emit(ctx, '%');
            continue;
        }

        int left = 0, zero = 0, plus = 0, space = 0, alt = 0;
        for (;; p++)
        {
            if (*p == '-') left = 1;
            else if (*p == '0') zero = 1;
            else if (*p == '+') plus = 1;
            else if (*p == ' ') space = 1;
            else if (*p == '#') alt = 1;
            else break;
        }

        int width = 0;
        if (*p == '*')
        {
            width = va_arg(ap, int);
            p++;
            if (width < 0) { left = 1; width = -width; }
        }
        else
        {
            while (*p >= '0' && *p <= '9')
                width = width * 10 + (*p++ - '0');
        }

        int prec = -1;
        if (*p == '.')
        {
            p++;
            prec = 0;
            if (*p == '*')
            {
                prec = va_arg(ap, int);
                p++;
                if (prec < 0) prec = -1;
            }
            else
            {
                while (*p >= '0' && *p <= '9')
                    prec = prec * 10 + (*p++ - '0');
            }
        }

        int is_ll = 0;
        while (*p == 'h' || *p == 'l' || *p == 'z' || *p == 'j' || *p == 't')
        {
            if (*p == 'l' && p[1] == 'l')
            {
                is_ll = 1;
                p += 2;
                continue;
            }
            if (*p == 'j')
                is_ll = 1;
            p++;
        }

        char conv = *p;
        switch (conv)
        {
            case 'd':
            case 'i':
            {
                long long v = is_ll ? va_arg(ap, long long)
                                    : (long long)va_arg(ap, int);
                unsigned long long mag = v < 0 ? 0ull - (unsigned long long)v
                                               : (unsigned long long)v;
                int signlen = (v < 0 || plus || space) ? 1 : 0;
                char sign = v < 0 ? '-' : (plus ? '+' : ' ');
                char tmp[32];
                int n = 0;
                if (mag == 0)
                    tmp[n++] = '0';
                while (mag)
                {
                    tmp[n++] = (char)('0' + mag % 10);
                    mag /= 10;
                }
                int total = n + signlen;
                if (!left && !zero)
                    put_pad(emit, ctx, ' ', width - total);
                if (signlen)
                    emit(ctx, sign);
                if (!left && zero)
                    put_pad(emit, ctx, '0', width - total);
                while (n > 0)
                    emit(ctx, tmp[--n]);
                if (left)
                    put_pad(emit, ctx, ' ', width - total);
                break;
            }
            case 'u':
                put_uint(emit, ctx, is_ll ? va_arg(ap, unsigned long long)
                                          : va_arg(ap, unsigned int),
                         10, 0, width, zero, left, 0);
                break;
            case 'x':
                put_uint(emit, ctx, is_ll ? va_arg(ap, unsigned long long)
                                          : va_arg(ap, unsigned int),
                         16, 0, width, zero, left, alt);
                break;
            case 'X':
                put_uint(emit, ctx, is_ll ? va_arg(ap, unsigned long long)
                                          : va_arg(ap, unsigned int),
                         16, 1, width, zero, left, alt);
                break;
            case 'o':
                put_uint(emit, ctx, is_ll ? va_arg(ap, unsigned long long)
                                          : va_arg(ap, unsigned int),
                         8, 0, width, zero, left, alt);
                break;
            case 'p':
            {
                void *ptr = va_arg(ap, void *);
                if (!ptr)
                {
                    put_str(emit, ctx, "(nil)", -1);
                    break;
                }
                put_uint(emit, ctx, (unsigned long long)(unsigned int)ptr,
                         16, 0, width, zero, left, 1);
                break;
            }
            case 'c':
            {
                char ch = (char)va_arg(ap, int);
                if (!left)
                    put_pad(emit, ctx, ' ', width - 1);
                emit(ctx, ch);
                if (left)
                    put_pad(emit, ctx, ' ', width - 1);
                break;
            }
            case 's':
            {
                const char *s = va_arg(ap, const char *);
                if (!s)
                    s = "(null)";
                int n = 0;
                while (s[n] && (prec < 0 || n < prec))
                    n++;
                if (!left)
                    put_pad(emit, ctx, ' ', width - n);
                put_str(emit, ctx, s, n);
                if (left)
                    put_pad(emit, ctx, ' ', width - n);
                break;
            }
            default:
                emit(ctx, '%');
                if (conv)
                    emit(ctx, conv);
                break;
        }
    }
    return (int)o->len;
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    struct out_ctx o;
    o.f = f;
    o.buf = 0;
    o.cap = 0;
    o.len = 0;
    return do_format(emit_file, &o, fmt, ap);
}

int vsnprintf(char *buf, unsigned int n, const char *fmt, va_list ap)
{
    struct out_ctx o;
    o.f = 0;
    o.buf = buf;
    o.cap = n;
    o.len = 0;
    int r = do_format(emit_buf, &o, fmt, ap);
    if (n > 0)
    {
        unsigned int end = o.len < n - 1 ? o.len : n - 1;
        buf[end] = '\0';
    }
    return r;
}

int vprintf(const char *fmt, va_list ap)
{
    return vfprintf(stdout, fmt, ap);
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return n;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int snprintf(char *buf, unsigned int n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int vsprintf(char *buf, const char *fmt, va_list ap)
{
    struct out_ctx o;
    o.f = 0;
    o.buf = buf;
    o.cap = ~0u;        /* sprintf writes however much it needs */
    o.len = 0;
    int r = do_format(emit_buf, &o, fmt, ap);
    buf[o.len] = '\0';
    return r;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}
