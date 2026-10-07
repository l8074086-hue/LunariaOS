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

static inline void *memmove(void *dest, const void *src, unsigned int n)
{
  char *d = dest;
  const char *s = src;

  if (d == s || n == 0)
    return dest;

  if (d < s)
  {
    for (unsigned int i = 0; i < n; i++)
      d[i] = s[i];
  }
  else
  {
    for (unsigned int i = n; i > 0; i--)
      d[i - 1] = s[i - 1];
  }
  return dest;
}

static inline int memcmp(const void *a, const void *b, unsigned int n)
{
  const unsigned char *x = a;
  const unsigned char *y = b;

  for (unsigned int i = 0; i < n; i++)
  {
    if (x[i] != y[i])
      return x[i] - y[i];
  }
  return 0;
}

static inline void *memchr(const void *s, int c, unsigned int n)
{
  const unsigned char *p = s;
  unsigned char ch = (unsigned char)c;

  for (unsigned int i = 0; i < n; i++)
  {
    if (p[i] == ch)
      return (void *)(p + i);
  }
  return 0;
}

static inline unsigned int strnlen(const char *s, unsigned int max)
{
  unsigned int n = 0;

  while (n < max && s[n])
    n++;
  return n;
}

static inline char *strncpy(char *dest, const char *src, unsigned int n)
{
  unsigned int i = 0;

  for (; i < n && src[i]; i++)
    dest[i] = src[i];
  for (; i < n; i++)
    dest[i] = '\0';
  return dest;
}

static inline char *strcat(char *dest, const char *src)
{
  char *d = dest;

  while (*d)
    d++;
  while ((*d++ = *src++))
    ;
  return dest;
}

static inline char *strncat(char *dest, const char *src, unsigned int n)
{
  char *d = dest + strlen(dest);
  unsigned int i = 0;

  for (; i < n && src[i]; i++)
    d[i] = src[i];
  d[i] = '\0';
  return dest;
}

static inline int strncmp(const char *a, const char *b, unsigned int n)
{
  for (unsigned int i = 0; i < n; i++)
  {
    if (a[i] != b[i])
      return (unsigned char)a[i] - (unsigned char)b[i];
    if (!a[i])
      break;
  }
  return 0;
}

static inline char *strchr(const char *s, int c)
{
  for (;; s++)
  {
    if (*s == (char)c)
      return (char *)s;
    if (!*s)
      return 0;
  }
}

static inline char *strrchr(const char *s, int c)
{
  const char *last = 0;

  for (;; s++)
  {
    if (*s == (char)c)
      last = s;
    if (!*s)
      break;
  }
  return (char *)last;
}

static inline char *strstr(const char *hay, const char *needle)
{
  if (!*needle)
    return (char *)hay;

  for (; *hay; hay++)
  {
    const char *h = hay;
    const char *n = needle;
    while (*h && *n && *h == *n)
    {
      h++;
      n++;
    }
    if (!*n)
      return (char *)hay;
  }
  return 0;
}

/* implemented in the userspace C library, since they need the heap */
char *strdup(const char *s);
char *strndup(const char *s, unsigned int n);
char *strerror(int errnum);

#endif
