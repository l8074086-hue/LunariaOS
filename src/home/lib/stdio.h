#ifndef STDIO_H
#define STDIO_H

#include "user_api.h"
#include <stdarg.h>

#ifndef EOF
#define EOF (-1)
#endif

/* print()/putchar() are the cheap kernel-syscall helpers that predate the
   FILE layer; a lot of existing code uses them. */
static inline void putchar(const char c)
{
    sys_write(&c, 1);
}

static inline void print(const char *string)
{
    sys_print(string);
}

/* A stdio stream. The struct lives in stdio.c so programs only ever hold a
   pointer; the three standard streams always exist. */
typedef struct FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
unsigned int fread(void *ptr, unsigned int size, unsigned int nmemb, FILE *f);
unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nmemb, FILE *f);
int fseek(FILE *f, int off, int whence);
int ftell(FILE *f);
void rewind(FILE *f);
int fgetc(FILE *f);
int getc(FILE *f);
int getchar(void);
int fputc(int c, FILE *f);
int putc(int c, FILE *f);
int ungetc(int c, FILE *f);
char *fgets(char *s, int n, FILE *f);
int fputs(const char *s, FILE *f);
int puts(const char *s);
int feof(FILE *f);
int ferror(FILE *f);
void clearerr(FILE *f);
int fflush(FILE *f);
int remove(const char *path);

/* Bind a FILE to an already-open kernel handle, and re-target an existing
   stream (only file streams can be re-opened). */
FILE *fdopen(int fd, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *f);
int fileno(FILE *f);

int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list ap);
int snprintf(char *buf, unsigned int n, const char *fmt, ...);
int vsnprintf(char *buf, unsigned int n, const char *fmt, va_list ap);

#endif
