#ifndef STRING_H
#define STRING_H

static inline int strcmp(const char *a, const char *b)
{
  while (*a && *a == *b)
  {
    a++;
    b++;
  }
  return *a - *b;
}

static inline int strlen(const char *str)
{
  int i = 0;
  while (*str)
  {
    i++;
    str++;
  }
  return i;
}

static inline int atoi(const char *s)
{
  int n = 0;
  while (*s >= '0' && *s <= '9')
    n = n * 10 + (*s++ - '0');
  return n;
}

static inline char *strcpy(char *dest, const char *src)
{
  char *d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

static inline void *memset(void *dest, int c, unsigned int n)
{
  char *d = dest;
  for (unsigned int i = 0; i < n; i++)
    d[i] = (char)c;
  return dest;
}

static inline void *memcpy(void *dest, const void *src, unsigned int n)
{
  char *d = dest;
  const char *s = src;
  for (unsigned int i = 0; i < n; i++)
    d[i] = s[i];
  return dest;
}

static inline void itoa(int n, char *buf)
{
  unsigned int m;
  int i = 0, j;
  char t;

  if (n < 0)
    m = 0u - (unsigned int)n;
  else
    m = (unsigned int)n;

  do
  {
    buf[i++] = '0' + (m % 10);
    m /= 10;
  } while (m > 0);

  if (n < 0)
    buf[i++] = '-';

  buf[i] = '\0';

  for (j = 0; j < i / 2; j++)
  {
    t = buf[j];
    buf[j] = buf[i - 1 - j];
    buf[i - 1 - j] = t;
  }
}

#endif
