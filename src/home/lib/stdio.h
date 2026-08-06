#ifndef STDIO_H
#define STDIO_H

#include "user_api.h"

static inline void putchar(const char c)
{
  sys_write(&c, 1);
}
static inline void print(const char *string)
{
  sys_print(string);
}

#endif


