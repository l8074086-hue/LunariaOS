#include "stdio.h"
#include "string.h"

static char buf[256];
static int failures;
static int total;

static void check(const char *name, int ok)
{
    print(ok ? "PASS " : "FAIL ");
    print(name);
    print("\n");
    total++;
    if (!ok)
        failures++;
}

void _start(void)
{
    int n;

    /* --- snprintf ------------------------------------------------------- */
    n = snprintf(buf, sizeof buf, "%d %s %x %c %5d|%-5d|",
                 42, "hi", 0xbeef, 'Z', 7, 8);
    check("snprintf basic", n == 25 && strcmp(buf, "42 hi beef Z     7|8    |") == 0);

    n = snprintf(buf, 5, "abcdef");
    check("snprintf truncates", n == 6 && strcmp(buf, "abcd") == 0);

    n = snprintf(buf, sizeof buf, "%05d %+d %d %u %%", 42, 5, -7, 9u);
    check("snprintf flags", n == 15 && strcmp(buf, "00042 +5 -7 9 %") == 0);

    /* --- write a file through FILE -------------------------------------- */
    FILE *f = fopen("ST.TXT", "w");
    check("fopen w", f != 0);
    if (f)
    {
        check("fprintf", fprintf(f, "line %s\n", "one") == 9);
        check("fputs", fputs("line two\n", f) == 0);
        check("fputc", fputc('X', f) == 'X');
        check("fclose", fclose(f) == 0);
    }

    /* --- read it back --------------------------------------------------- */
    f = fopen("ST.TXT", "r");
    check("fopen r", f != 0);
    if (f)
    {
        char *s = fgets(buf, sizeof buf, f);
        check("fgets line", s && strcmp(buf, "line one\n") == 0);
        check("fgetc", fgetc(f) == 'l');
        check("ftell", ftell(f) == 10);

        check("fseek", fseek(f, 0, SEEK_SET) == 0);
        check("fread all", fread(buf, 1, 19, f) == 19 &&
                         memcmp(buf, "line one\nline two\nX", 19) == 0);
        check("feof clear until read", feof(f) == 0);
        check("fgetc at eof", fgetc(f) == EOF && feof(f) == 1);
        check("clearerr", (clearerr(f), feof(f) == 0));
        fclose(f);
    }

    /* --- append --------------------------------------------------------- */
    f = fopen("ST.TXT", "a");
    check("fopen a", f != 0);
    if (f)
    {
        check("append write", fputs("tail", f) == 0);
        fclose(f);
    }
    f = fopen("ST.TXT", "r");
    check("append read", fread(buf, 1, 23, f) == 23 &&
                        memcmp(buf + 19, "tail", 4) == 0);
    fclose(f);

    /* --- console streams and remove ------------------------------------- */
    check("fprintf stderr", fprintf(stderr, "[stdiotest]\n") == 12);
    check("remove", remove("ST.TXT") == 0);
    check("gone", fopen("ST.TXT", "r") == 0);

    print(failures ? "STDIOTEST FAILED\n" : "STDIOTEST ALL PASS\n");
    printf("printf works: %d/%d checks\n", total - failures, total);
    sys_exit();
}
