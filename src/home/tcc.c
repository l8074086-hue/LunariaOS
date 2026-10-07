/* tcc -- the on-device C compiler.

   Usage:  tcc <file.c> [args...]         compile file.c and run its main()
           tcc <file.c> -o <out.bin>      link a flat binary and write it
           tcc                           built-in compile-from-memory self test

   The libtcc core comes in as libtcc.a.

   Run mode links the generated code with -nostdlib, so any libc function a
   compiled file calls must be registered with tcc_add_symbol() in add_libc()
   below; the registered pointer is the copy of that function living in this
   program's own libc.a image, and the code runs right here in this process.

   -o mode instead compiles a small embedded runtime (flat_runtime, below)
   together with the source into one self-contained image, baked for the
   fixed TCC_FLAT_BASE address, writes it to the disk behind a 12-byte
   "LUNB" header, and exits -- the shell's `run` command then loads that
   file verbatim at TCC_FLAT_BASE. Only what flat_runtime implements is
   available there; see docs/userspace.md. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "libtcc.h"

static const char demo_src[] =
    "int add(int a, int b) { return a + b; }\n"
    "int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }\n";

/* print_dec: tiny signed-decimal printer, defined below. Compiled code calls
   it in run mode (resolved against this copy here) and flat mode (resolved
   against the copy in flat_runtime); demo programs print numbers either way. */
static void print_dec_(int n);

/* The slice of libc.a this image contains, offered to the tcc linker so a
   compiled file can call the usual suspects. Anything else fails at
   relocation with an undefined-symbol message. */
static void add_libc(TCCState *s)
{
    tcc_add_symbol(s, "printf", (void *)&printf);
    tcc_add_symbol(s, "fprintf", (void *)&fprintf);
    tcc_add_symbol(s, "vprintf", (void *)&vprintf);
    tcc_add_symbol(s, "vfprintf", (void *)&vfprintf);
    tcc_add_symbol(s, "sprintf", (void *)&sprintf);
    tcc_add_symbol(s, "vsprintf", (void *)&vsprintf);
    tcc_add_symbol(s, "snprintf", (void *)&snprintf);
    tcc_add_symbol(s, "vsnprintf", (void *)&vsnprintf);
    tcc_add_symbol(s, "puts", (void *)&puts);
    tcc_add_symbol(s, "putchar", (void *)&putchar);
    tcc_add_symbol(s, "print", (void *)&print);
    tcc_add_symbol(s, "print_dec", (void *)&print_dec_);
    tcc_add_symbol(s, "fopen", (void *)&fopen);
    tcc_add_symbol(s, "fclose", (void *)&fclose);
    tcc_add_symbol(s, "fread", (void *)&fread);
    tcc_add_symbol(s, "fwrite", (void *)&fwrite);
    tcc_add_symbol(s, "fseek", (void *)&fseek);
    tcc_add_symbol(s, "ftell", (void *)&ftell);
    tcc_add_symbol(s, "rewind", (void *)&rewind);
    tcc_add_symbol(s, "fgetc", (void *)&fgetc);
    tcc_add_symbol(s, "getc", (void *)&getc);
    tcc_add_symbol(s, "getchar", (void *)&getchar);
    tcc_add_symbol(s, "fputc", (void *)&fputc);
    tcc_add_symbol(s, "putc", (void *)&putc);
    tcc_add_symbol(s, "ungetc", (void *)&ungetc);
    tcc_add_symbol(s, "fgets", (void *)&fgets);
    tcc_add_symbol(s, "fputs", (void *)&fputs);
    tcc_add_symbol(s, "feof", (void *)&feof);
    tcc_add_symbol(s, "ferror", (void *)&ferror);
    tcc_add_symbol(s, "clearerr", (void *)&clearerr);
    tcc_add_symbol(s, "fflush", (void *)&fflush);
    tcc_add_symbol(s, "remove", (void *)&remove);
    tcc_add_symbol(s, "fileno", (void *)&fileno);

    tcc_add_symbol(s, "malloc", (void *)&malloc);
    tcc_add_symbol(s, "calloc", (void *)&calloc);
    tcc_add_symbol(s, "realloc", (void *)&realloc);
    tcc_add_symbol(s, "free", (void *)&free);
    tcc_add_symbol(s, "exit", (void *)&exit);
    tcc_add_symbol(s, "abort", (void *)&abort);
    tcc_add_symbol(s, "getenv", (void *)&getenv);
    tcc_add_symbol(s, "abs", (void *)&abs);
    tcc_add_symbol(s, "qsort", (void *)&qsort);
    tcc_add_symbol(s, "strtol", (void *)&strtol);
    tcc_add_symbol(s, "strtoul", (void *)&strtoul);
    tcc_add_symbol(s, "strtoll", (void *)&strtoll);
    tcc_add_symbol(s, "strtoull", (void *)&strtoull);

    tcc_add_symbol(s, "strlen", (void *)&strlen);
    tcc_add_symbol(s, "strcmp", (void *)&strcmp);
    tcc_add_symbol(s, "strncmp", (void *)&strncmp);
    tcc_add_symbol(s, "strcpy", (void *)&strcpy);
    tcc_add_symbol(s, "strncpy", (void *)&strncpy);
    tcc_add_symbol(s, "strcat", (void *)&strcat);
    tcc_add_symbol(s, "strncat", (void *)&strncat);
    tcc_add_symbol(s, "strchr", (void *)&strchr);
    tcc_add_symbol(s, "strrchr", (void *)&strrchr);
    tcc_add_symbol(s, "strstr", (void *)&strstr);
    tcc_add_symbol(s, "strdup", (void *)&strdup);
    tcc_add_symbol(s, "strndup", (void *)&strndup);
    tcc_add_symbol(s, "strerror", (void *)&strerror);
    tcc_add_symbol(s, "memset", (void *)&memset);
    tcc_add_symbol(s, "memcpy", (void *)&memcpy);
    tcc_add_symbol(s, "memmove", (void *)&memmove);
    tcc_add_symbol(s, "memcmp", (void *)&memcmp);
    tcc_add_symbol(s, "memchr", (void *)&memchr);

    tcc_add_symbol(s, "open", (void *)&open);
    tcc_add_symbol(s, "close", (void *)&close);
    tcc_add_symbol(s, "read", (void *)&read);
    tcc_add_symbol(s, "write", (void *)&write);
    tcc_add_symbol(s, "lseek", (void *)&lseek);
    tcc_add_symbol(s, "unlink", (void *)&unlink);
}

/* print_dec: tiny signed-decimal printer. Compiled code calls it in run
   mode (resolved against this copy here) and flat mode (resolved against
   the copy in flat_runtime below); demo programs print numbers either way. */
static void print_dec_(int n)
{
    char b[16];
    unsigned int v, i = 15;
    if (n < 0)
    {
        print("-");
        v = (unsigned int)(0 - n);
    }
    else
        v = (unsigned int)n;
    do { b[i--] = (char)('0' + v % 10); v /= 10; } while (v);
    print(&b[i + 1]);
}

/* The runtime compiled into every -o image, as its own translation unit so
   it needs no add_libc() registration. It uses the syscall trap directly
   (there are no headers on the device disk to #include). Keep the set
   small and tcc-compilable. */
static const char flat_runtime[] =
    "static unsigned int rt_len(const char *s)\n"
    "{ unsigned int n = 0; while (s[n]) n++; return n; }\n"
    "\n"
    "void print(const char *s)\n"
    "{ unsigned int n = rt_len(s);\n"
    "  __asm__ volatile(\"int $0x80\" :: \"a\"(1), \"b\"(s), \"c\"(n) : \"memory\"); }\n"
    "\n"
    "void putchar(int c)\n"
    "{ char ch = (char)c;\n"
    "  __asm__ volatile(\"int $0x80\" :: \"a\"(1), \"b\"(&ch), \"c\"(1) : \"memory\"); }\n"
    "\n"
    "void puts(const char *s) { print(s); print(\"\\n\"); }\n"
    "\n"
    "void print_dec(int n)\n"
    "{\n"
    "  char b[16]; unsigned int v, i = 15;\n"
    "  if (n < 0) { print(\"-\"); v = (unsigned int)(0 - n); }\n"
    "  else v = (unsigned int)n;\n"
    "  do { b[i--] = (char)('0' + v % 10); v /= 10; } while (v);\n"
    "  print(&b[i + 1]);\n"
    "}\n"
    "\n"
    "void exit(int status)\n"
    "{ (void)status; __asm__ volatile(\"int $0x80\" :: \"a\"(2));\n"
    "  for (;;) {}\n"
    "}\n"
    "\n"
    "unsigned int strlen(const char *s) { return rt_len(s); }\n"
    "\n"
    "int strcmp(const char *a, const char *b)\n"
    "{ while (*a && *a == *b) { a++; b++; } return *a - *b; }\n"
    "\n"
    "int atoi(const char *s)\n"
    "{ int r = 0, sign = 1;\n"
    "  if (*s == '-') { sign = -1; s++; }\n"
    "  while (*s >= '0' && *s <= '9') { r = r * 10 + (*s - '0'); s++; }\n"
    "  return r * sign; }\n"
    "\n"
    "int abs(int n) { return n < 0 ? -n : n; }\n"
    "\n"
    "void *memset(void *d, int c, unsigned int n)\n"
    "{ unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }\n"
    "\n"
    "void *memcpy(void *d, const void *s, unsigned int n)\n"
    "{ unsigned char *p = d; const unsigned char *q = s;\n"
    "  while (n--) *p++ = *q++; return d; }\n"
    "\n"
    "char *strcpy(char *d, const char *s)\n"
    "{ char *p = d; while ((*p++ = *s++)) ; return d; }\n"
    "\n"
    "extern int main();\n"
    "void _start(int argc, char **argv) { exit(main(argc, argv, 0)); }\n";

/* Read a whole .c file from the disk into a NUL-terminated buffer. */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        print("tcc: cannot open ");
        print(path);
        print("\n");
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 1 || sz > 0x10000)
    {
        print("tcc: file empty or too big\n");
        fclose(f);
        return 0;
    }

    char *src = malloc((unsigned int)sz + 1);
    if (!src)
    {
        print("tcc: out of memory\n");
        fclose(f);
        return 0;
    }
    unsigned int got = fread(src, 1, (unsigned int)sz, f);
    src[got] = '\0';
    fclose(f);
    return src;
}

/* tcc <file.c> [args...]: read the file, compile it to memory, and call its
   main(argc, argv, NULL) with the arguments that followed the file name. */
static void run_file(const char *path, int argc, char **argv)
{
    char *src = read_file(path);
    if (!src)
        return;

    TCCState *s = tcc_new();
    if (!s)
    {
        print("tcc: out of memory\n");
        free(src);
        return;
    }
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tcc_set_options(s, "-nostdlib");
    add_libc(s);

    if (tcc_compile_string(s, src) != 0)
    {
        print("tcc: compile failed\n");
        free(src);
        tcc_delete(s);
        return;
    }
    free(src);

    if (tcc_relocate(s) < 0)
    {
        print("tcc: relocation failed\n");
        tcc_delete(s);
        return;
    }

    int (*main_fn)(int, char **, char **) = tcc_get_symbol(s, "main");
    if (main_fn)
    {
        int r = main_fn(argc, argv, 0);
        printf("tcc: main() -> %d\n", r);
    }
    else
        print("tcc: no main() in file\n");

    tcc_delete(s);
}

/* tcc <file.c> -o <out.bin>: compile the file together with flat_runtime,
   bake the image for TCC_FLAT_BASE, and write it behind the flat-binary
   header. The image must be able to load at TCC_FLAT_BASE, stay below the
   user stack, and fit the disk's 64K writable-file buffer. */
static void build_flat(const char *path, const char *out)
{
    char *src = read_file(path);
    if (!src)
        return;

    TCCState *s = tcc_new();
    if (!s)
    {
        print("tcc: out of memory\n");
        free(src);
        return;
    }
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tcc_set_options(s, "-nostdlib");

    if (tcc_compile_string(s, flat_runtime) != 0 || tcc_compile_string(s, src) != 0)
    {
        print("tcc: compile failed\n");
        free(src);
        tcc_delete(s);
        return;
    }
    free(src);

    int size = tcc_relocate_at(s, (void *)TCC_FLAT_BASE);
    if (size < 0)
    {
        print("tcc: relocation failed\n");
        tcc_delete(s);
        return;
    }

    unsigned int entry = (unsigned int)tcc_get_symbol(s, "_start");
    if (!entry || entry < TCC_FLAT_BASE)
    {
        print("tcc: no _start symbol\n");
        tcc_delete(s);
        return;
    }
    entry -= TCC_FLAT_BASE;

    if ((unsigned int)size + 12u > TCC_FLAT_MAX)
    {
        print("tcc: image too big\n");
        tcc_delete(s);
        return;
    }

    FILE *f = fopen(out, "wb");
    if (!f)
    {
        print("tcc: cannot create ");
        print(out);
        print("\n");
        tcc_delete(s);
        return;
    }

    /* 12-byte header: "LUNB" + u32 image size + u32 _start offset. The
       shell's loader recognizes this magic and loads the image at
       TCC_FLAT_BASE instead of USER_BASE. */
    char hdr[12];
    hdr[0] = TCC_FLAT_MAGIC0; hdr[1] = TCC_FLAT_MAGIC1;
    hdr[2] = TCC_FLAT_MAGIC2; hdr[3] = TCC_FLAT_MAGIC3;
    hdr[4] = (char)size;            hdr[5] = (char)(size >> 8);
    hdr[6] = (char)(size >> 16);    hdr[7] = (char)(size >> 24);
    hdr[8] = (char)entry;           hdr[9] = (char)(entry >> 8);
    hdr[10] = (char)(entry >> 16);  hdr[11] = (char)(entry >> 24);
    fwrite(hdr, 1, sizeof hdr, f);
    fwrite((void *)TCC_FLAT_BASE, 1, (unsigned int)size, f);
    fclose(f);

    printf("tcc: wrote %s (%d bytes, entry 0x%x)\n",
           out, size, (unsigned int)entry);
    tcc_delete(s);
}

/* Compile-and-call from memory: the quick way to prove the core works
   without needing a .c file on the disk image. */
static void self_test(void)
{
    TCCState *s = tcc_new();
    if (!s)
    {
        print("tcc: out of memory\n");
        return;
    }

    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tcc_set_options(s, "-nostdlib");

    if (tcc_compile_string(s, demo_src) != 0)
    {
        print("tcc: compile failed\n");
        tcc_delete(s);
        return;
    }

    if (tcc_relocate(s) < 0)
    {
        print("tcc: relocation failed\n");
        tcc_delete(s);
        return;
    }

    int (*add)(int, int) = tcc_get_symbol(s, "add");
    int (*fib)(int) = tcc_get_symbol(s, "fib");

    if (add && fib)
        printf("tcc: add(2,3)=%d fib(10)=%d\n", add(2, 3), fib(10));
    else
        print("tcc: symbol lookup failed\n");

    tcc_delete(s);
}

void _start(int argc, char **argv)
{
    if (argc < 2)
    {
        self_test();
        sys_exit();
        return; /* not reached */
    }

    if (argv[1][0] == '-')
    {
        print("tcc: usage: tcc <file.c> [-o out.bin] [args...]\n");
        sys_exit();
        return;
    }

    /* tcc <file.c> -o <out.bin> -- the -o pair must be the whole tail. */
    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 >= argc || i + 2 != argc)
            {
                print("tcc: usage: tcc <file.c> -o <out.bin>\n");
                sys_exit();
                return;
            }
            build_flat(argv[1], argv[i + 1]);
            sys_exit();
            return;
        }
    }

    run_file(argv[1], argc - 1, argv + 1);
    sys_exit();
}