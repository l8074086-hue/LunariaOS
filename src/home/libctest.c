#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ctype.h"

/* A .bss sentinel the allocator must never write to. Because .bss is not part
   of the loaded image, this only stays intact if heap_boot() grows the break
   past __heap_start. */
static char sentinel[4096];
static int failures;

static void check(const char *name, int ok)
{
    print(ok ? "PASS " : "FAIL ");
    print(name);
    print("\n");
    if (!ok)
        failures++;
}

static int cmp_int(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

void _start(void)
{
    int i;

    for (i = 0; i < (int)sizeof sentinel; i++)
        sentinel[i] = 0x5A;

    char *a = malloc(100);
    char *b = malloc(200);
    char *c = malloc(3000);
    check("malloc non-null", a && b && c);
    check("malloc aligned", ((unsigned int)a & 7) == 0 &&
                            ((unsigned int)b & 7) == 0 &&
                            ((unsigned int)c & 7) == 0);
    check("malloc distinct", a != b && b != c && a != c);

    memset(a, 'A', 100);
    memset(b, 'B', 200);
    memset(c, 'C', 3000);
    for (i = 0; i < 100; i++)
        if (a[i] != 'A')
            break;
    check("block a intact", i == 100);
    for (i = 0; i < 200; i++)
        if (b[i] != 'B')
            break;
    check("block b intact", i == 200);

    /* a freed block is reused by the next same-sized request */
    free(b);
    char *d = malloc(150);
    check("freed block reused", d == b);

    /* realloc preserves the old contents when it has to move */
    char *e = realloc(a, 5000);
    int kept = e != 0;
    for (i = 0; kept && i < 100; i++)
        if (e[i] != 'A')
            kept = 0;
    check("realloc preserves", kept);

    /* calloc hands back zeroed memory */
    char *z = calloc(512, 1);
    int zeroed = z != 0;
    for (i = 0; zeroed && i < 512; i++)
        if (z[i] != 0)
            zeroed = 0;
    check("calloc zeroed", zeroed);

    /* a large request forces the break to grow */
    char *big = malloc(200000);
    check("large malloc", big != 0);
    if (big)
    {
        big[0] = 1;
        big[199999] = 2;
        check("large writable", big[0] == 1 && big[199999] == 2);
    }

    free(c);
    free(d);
    free(e);
    free(z);
    free(big);

    int intact = 1;
    for (i = 0; i < (int)sizeof sentinel; i++)
        if (sentinel[i] != 0x5A)
        {
            intact = 0;
            break;
        }
    check("bss untouched by heap", intact);

    char buf[32];
    strcpy(buf, "hello");
    strcat(buf, ", world");
    check("strcat", strcmp(buf, "hello, world") == 0);
    strncpy(buf, "abcXXXX", 3);
    buf[3] = '\0';
    check("strncpy", strcmp(buf, "abc") == 0);
    check("strchr/strrchr", strchr("abcabc", 'b') != 0 &&
                           strrchr("abcabc", 'b') == strchr("abcabc", 'b') + 3);
    check("strstr", strstr("hello world", "world") != 0 &&
                    strstr("hello", "xyz") == 0);
    check("memcmp", memcmp("abc", "abd", 2) == 0 && memcmp("abc", "abd", 3) != 0);

    char mm[8];
    strcpy(mm, "abcdef");
    memmove(mm + 1, mm, 5);
    mm[6] = '\0';
    check("memmove overlap", strcmp(mm, "aabcde") == 0);

    char *dup = strdup("copied");
    check("strdup", dup != 0 && strcmp(dup, "copied") == 0);
    free(dup);

    check("isdigit", isdigit('7') && !isdigit('x'));
    check("isalpha", isalpha('Q') && !isalpha('7'));
    check("isspace", isspace(' ') && isspace('\n') && !isspace('x'));
    check("toupper/tolower", toupper('q') == 'Q' && tolower('Q') == 'q');

    int nums[5] = { 4, 1, 3, 2, 0 };
    qsort(nums, 5, sizeof nums[0], cmp_int);
    check("qsort", nums[0] == 0 && nums[1] == 1 && nums[2] == 2 &&
                   nums[3] == 3 && nums[4] == 4);

    print(failures ? "LIBCTEST FAILED\n" : "LIBCTEST ALL PASS\n");
    sys_exit();
}
