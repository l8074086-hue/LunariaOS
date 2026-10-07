#ifndef TIME_H
#define TIME_H

#include <stddef.h>

typedef long time_t;

struct tm
{
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;     /* 0-11 */
    int tm_year;    /* years since 1900 */
    int tm_wday;    /* 0-6, Sunday = 0 */
    int tm_yday;    /* 0-365 */
    int tm_isdst;
};

time_t time(time_t *t);
struct tm *localtime(const time_t *t);
struct tm *gmtime(const time_t *t);
size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm);

#endif
