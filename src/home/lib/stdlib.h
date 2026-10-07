#ifndef STDLIB_H
#define STDLIB_H

/* Pulls in the freestanding string helpers (mem*, str*, atoi, itoa) so that
   programs get the usual stdlib functions from one include. */
#include "string.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* A first-fit heap over the program break; see libc.c. The allocator moves
   the break past .bss on first use, so the heap never overlaps the image. */
void *malloc(unsigned int size);
void *calloc(unsigned int count, unsigned int size);
void *realloc(void *ptr, unsigned int size);
void free(void *ptr);

void exit(int status);
void abort(void);
char *getenv(const char *name);
int abs(int n);

void qsort(void *base, unsigned int n, unsigned int size,
           int (*cmp)(const void *, const void *));

/* Number parsing. The long long variants are needed by the compiler core. */
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
long long strtoll(const char *s, char **end, int base);
unsigned long long strtoull(const char *s, char **end, int base);
double strtod(const char *s, char **end);
float strtof(const char *s, char **end);
long double strtold(const char *s, char **end);

/* No symlinks or ".." resolution yet: the path is copied through unchanged. */
char *realpath(const char *path, char *resolved);

#endif
