
# Writing userspace software for LunariaOS

LunariaOS can run flat binaries in **ring 3** (user mode). User programs can
only access hardware and kernel services through the `int 0x80` syscall
interface described below.

Each program runs in its own virtual address space. LunariaOS uses 32-bit x86
paging with 4 KiB pages, and the user address range is backed on demand as
pages are accessed.

## How a program is built and run

1. Write `src/home/myprog.c`.
2. `make` compiles it with the freestanding flags, links it at `0x100000`
   (`src/home/prog.ld`), converts it to a flat binary, and embeds it into
   `bin/disk.img` under the name `myprog`.
3. In the shell, `run myprog [args...]` creates a new address space and loads
   the program into `USER_BASE`.
4. The shell builds an argument block on the new stack — the argument
   strings, then a NULL-terminated `char **argv` and a NULL `char **envp`,
   then a small cdecl entry frame — and `enter_user()` constructs a Ring 3
   `iret` frame that transfers execution to `_start` with that stack pointer.

The program receives its own page directory. The shell attaches this address
space before loading the binary so that the program's mappings belong to that
invocation rather than a previous program.

Your program should export `void _start(int argc, char **argv)`: `argc` is the
number of arguments after `run` (including the program name), and `argv`
points at the argument strings, with `argv[argc] == 0`. `argv[0]` is the
program name, so `run myprog a b` gives `argc == 3`, `argv[0] == "myprog"`,
`argv[1] == "a"`, `argv[2] == "b"`. A plain `void _start(void)` still works —
the extra frame contents are simply ignored. There is no return value and no
exit code: end the program by calling `sys_exit()` or `sys_exit_to(mode)`.

## Memory and paging

Userspace runs in a separate virtual address space from the kernel.

LunariaOS uses 4 KiB x86 pages. User memory is demand-paged, meaning that a
virtual page does not necessarily consume a physical frame until the program
actually accesses it.

The current user virtual-memory region is:

```text
0x400000 - 0x800000
```

The exact program load address is defined by `USER_BASE` and the userspace
linker script. The initial user stack is defined by `USER_STACK`.

When a program accesses a valid but not-yet-backed page, the CPU raises page
fault exception `#PF` (vector `0x0E`). The kernel's page-fault handler can
allocate a physical frame and map the page before execution continues.

Accessing an invalid address, or violating the permissions of a mapped page,
causes a page fault that the current kernel does not recover from.

Userspace programs therefore should treat pointers as ordinary virtual
addresses belonging to their own address space. They must not assume that a
virtual address corresponds to a particular physical address.

## Headers you can use

All of these are on the include path automatically:

| Header       | Contents                                                         |
| ------------ | ---------------------------------------------------------------- |
| `user_api.h` | syscall wrappers and constants                                   |
| `stdio.h`    | `print()`/`putchar()`, `FILE` streams, `printf`/`fprintf`/`snprintf` |
| `string.h`   | `mem*`/`str*` helpers, `strstr`, `memmove`, `strdup`, `strerror`, `itoa`, `atoi` |
| `stdlib.h`   | `malloc`/`free`/`realloc`, `exit`, `qsort`, `strtol`/`strtod` families, `realpath` |
| `ctype.h`    | `isdigit`, `isalpha`, `isspace`, `toupper`, `tolower`, ...       |
| `mirror.h`   | Mirror: double-buffered text-mode UI library                     |
| `errno.h`    | `errno` and the `E*` error codes                                 |
| `fcntl.h`    | `open()` and the `O_*` flags                                     |
| `unistd.h`   | `open`/`read`/`write`/`lseek`/`close`/`unlink`, `getcwd`, `sysconf`, `environ` |
| `setjmp.h`   | `setjmp`/`longjmp`                                               |
| `time.h`     | `time_t`, `struct tm`, `time`, `localtime`, `gmtime`, `strftime` |
| `sys/time.h` | `struct timeval` and `gettimeofday`                              |
| `sys/mman.h` | `mmap`/`munmap`/`mprotect` (`mprotect` is a no-op)               |
| `math.h`     | `HUGE_VAL`, `ldexp`/`ldexpf`/`ldexpl`                            |
| `inttypes.h` | integer type aliases from `<stdint.h>`                           |

These are LunariaOS-provided headers rather than a hosted C library.

## Syscall API

The wrappers live in `src/home/lib/user_api.h`. Each wrapper invokes
`int $0x80` with the syscall number in `eax` and arguments in
`ebx`/`ecx`/`edx`/`esi`.

| #  | Function                                                                          | Description                                                                                      |
| -- | --------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| 1  | `void sys_write(const char *buf, unsigned int len)`                               | Print `len` bytes of `buf` to the terminal                                                       |
| 2  | `void sys_exit(void)`                                                             | Return to the shell                                                                              |
| 3  | `int sys_getc(void)`                                                              | Block until a key is pressed; returns the character                                              |
| 4  | `int sys_read_file(const char *name, char *buf, unsigned int max)`                | Read a file into `buf`; returns size, or -1                                                      |
| 5  | `int sys_write_file(const char *name, const char *buf, unsigned int len)`         | Create/overwrite a file; returns 0 or -1                                                         |
| 6  | `int sys_list(const char *path, char *buf, unsigned int max)`                     | List a directory into `buf`; returns length or -1                                                |
| 7  | `void sys_exit_to(int mode)`                                                      | Exit and do something: `EXIT_SHELL` (0), `EXIT_HALT` (1), `EXIT_POWEROFF` (2), `EXIT_REBOOT` (3) |
| 8  | `void sys_clear(void)`                                                            | Clear the terminal                                                                               |
| 9  | `void sys_putchar_at(char c, unsigned int x, unsigned int y, unsigned int color)` | Draw a character at a screen position                                                            |
| 10 | `void sys_goto_xy(unsigned int x, unsigned int y)`                                | Move the terminal cursor                                                                         |
| 11 | `unsigned int sys_sbrk(unsigned int inc)`                                         | Grow the heap; returns the new break                                                             |
| 12 | `int sys_open(const char *name, int flags)`                                       | Open a file; returns a handle >= 3, or -1                                                        |
| 13 | `int sys_read(int fd, char *buf, unsigned int n)`                                 | Read up to `n` bytes; returns bytes read or -1                                                   |
| 14 | `int sys_fwrite(int fd, const char *buf, unsigned int n)`                         | Write `n` bytes at the file position; returns bytes written or -1                                |
| 15 | `int sys_lseek(int fd, int off, int whence)`                                      | Move the file position (`SEEK_SET`/`SEEK_CUR`/`SEEK_END`)                                        |
| 16 | `int sys_close(int fd)`                                                           | Flush and close a handle                                                                         |
| 17 | `int sys_unlink(const char *name)`                                                | Delete a file                                                                                    |

Useful constants:

```c
SCREEN_COLS 80
SCREEN_ROWS 25

C_BLACK ... C_WHITE
USER_COLOR(bg, fg)
```

Colors use the same 16-color VGA palette as the kernel.

### File handles

`sys_open` takes a name and a set of flags (`O_RDONLY`, `O_WRONLY`, `O_RDWR`,
plus `O_CREAT`, `O_TRUNC`, `O_APPEND`) and returns a handle. Handles are small
integers starting at 3; 0-2 are left for a future stdio.

A read-only handle streams straight from the disk, so it costs almost nothing
to keep open. A writable handle instead keeps the whole file in a kernel
buffer: `sys_fwrite` edits that buffer and `sys_close` writes it back with the
same in-place logic as `sys_write_file`. Only one writable handle can be open
at a time, and its contents are capped at 64 KB. Any handle a program leaves
open is flushed and dropped when it exits.

### stdio

`stdio.h` puts a familiar `FILE` layer on top of those handles:

```c
FILE *f = fopen("notes.txt", "w");
fprintf(f, "%d\n", 42);
fclose(f);

f = fopen("notes.txt", "r");
char line[80];
while (fgets(line, sizeof line, f))
    printf("%s", line);
fclose(f);
```

Supported: `fopen` (`r`/`w`/`a` optionally with `+`), `fclose`, `fread`,
`fwrite`, `fseek`, `ftell`, `rewind`, `fgetc`/`getc`/`getchar`,
`fputc`/`putc`, `ungetc`, `fgets`, `fputs`, `puts`, `feof`, `ferror`,
`clearerr`, `fflush`, `remove`, `fdopen`, `freopen`, `fileno`, and the `printf`
family (`printf`, `fprintf`, `vprintf`, `vfprintf`, `sprintf`, `vsprintf`,
`snprintf`, `vsnprintf`) with the usual flags, width and precision. The
formatter understands the `ll` length modifier, so `%llu`/`%lld`/`%08llx`
print full 64-bit values.

`stdin`, `stdout` and `stderr` are always open: reads come from the keyboard
and writes go to the current terminal. File streams are unbuffered, and the
stdio layer inherits the kernel's "one writable file at a time" limit.

## Example program

```c
#include "stdio.h"
#include "string.h"

void _start(void)
{
    char buf[1024];

    print("hello from ring 3!\n");

    if (sys_write_file("demo.txt", "file contents\n", 14) == 0)
        print("wrote demo.txt\n");

    int n = sys_read_file("demo.txt", buf, sizeof buf);
    if (n >= 0)
        sys_write(buf, (unsigned int)n);

    sys_putchar_at('@', 40, 12, USER_COLOR(C_BLACK, C_GREEN));

    sys_getc();
    sys_exit();
}
```

Build with `make`, boot with `make run`, then type:

```text
run demo
```

in the shell.

## The Mirror TUI library

`mirror.h` is a small text-mode UI library built on the syscalls. It is
compiled into `bin/libmirror.a`, which the Makefile links into every user
program; archive members are only pulled in when you reference them.

Mirror keeps two copies of the whole 80x25 screen. You draw into the back
buffer, then `mirror_flush()` sends only the cells that differ from what the
screen currently shows, so redrawing an unchanged area costs nothing.

* `mirror_init()` clears the screen with a single `sys_clear()` and parks
  the caret in the corner
* drawing: `mirror_clear()`, `mirror_putc()`, `mirror_puts()`,
  `mirror_printf()` (`%s`, `%c`, `%d`, `%x`, `%%`), `mirror_fill()`,
  `mirror_box()` (CP437 box drawing), `mirror_status()`, `mirror_progress()`
* input: `mirror_getkey()` blocks for one key and returns ASCII or
  `MIRROR_KEY_UP`/`DOWN`/`LEFT`/`RIGHT`; `mirror_menu_select()` runs an
  arrow-key menu and returns the chosen index or -1 on `esc`;
  `mirror_input()` edits a one-line field with left/right movement,
  backspace and horizontal scrolling, returning 0 on enter or -1 on `esc`
* flushing a fully blank screen collapses into one `sys_clear()` call, so
  clearing up at exit is instant

Example:

```c
#include "mirror.h"

static mirror_t screen;
static const char *items[] = { "say hello", "quit" };

void _start(void)
{
    mirror_init(&screen, USER_COLOR(C_BLACK, C_LIGHT_GREY));

    mirror_box(&screen, 4, 3, 30, 8, USER_COLOR(C_BLACK, C_CYAN), " menu ");
    mirror_puts(&screen, 6, 5, "pick an entry", USER_COLOR(C_BLACK, C_WHITE));
    mirror_status(&screen, 24, " demo", "enter selects",
                  USER_COLOR(C_BLUE, C_WHITE));
    mirror_flush(&screen);

    int sel = 0;
    for (;;)
    {
        sel = mirror_menu_select(&screen, 6, 6, 26, items, 2, sel,
                                 USER_COLOR(C_BLACK, C_LIGHT_GREY),
                                 USER_COLOR(C_WHITE, C_BLACK));
        if (sel < 0 || sel == 1)
            break;

        mirror_puts(&screen, 6, 9, "hello!", USER_COLOR(C_BLACK, C_YELLOW));
        mirror_flush(&screen);
    }

    mirror_clear(&screen, USER_COLOR(C_BLACK, C_LIGHT_GREY));
    mirror_flush(&screen);
    sys_exit();
}
```

`run mirrordemo` in the shell runs a larger example with a menu, a text
field, a progress bar and an event log.

## The edit text editor

`run edit` starts a small screen editor built entirely on Mirror and the
file syscalls — it is the largest example of putting the two together
([`src/home/edit.c`](../src/home/edit.c)).

* a startup prompt asks for a file name: `enter` opens it (or starts a new
  one), `esc` starts with an empty unnamed buffer
* the screen is a title bar, 23 rows of text and a status bar showing the
  message and the cursor position; the dirty-row blit keeps redraws cheap
* the buffer is a fixed 64 KB with horizontal and vertical scrolling and
  tab stops every 4 columns
* editing is always in insert mode: printable keys insert, `backspace`
  deletes (joining lines at a line start), `enter` splits, `tab` pads to
  the next tab stop, arrows move
* `esc` opens a vi-like `:` command line: `w [file]` writes, `wq` writes
  and quits, `q` quits, `q!` discards, `e <file>` loads another file,
  `n` starts an empty buffer
* a modified buffer refuses `q` until it is saved or forced with `q!`

Loading uses `sys_read_file()` and saving uses `sys_write_file()`.
Non-printable bytes from a loaded file are replaced with `.` so binary
files cannot corrupt the display.

## The userspace C library

`stdlib.h`, `ctype.h` and the extended `string.h` form a small C library built
into `bin/libc.a`. Like `libmirror.a` it is linked into every program but only
pulled in when something actually references it.

* `malloc()`, `calloc()`, `realloc()` and `free()` are a first-fit heap over
  `sys_sbrk()`. They grow the program break on demand and merge adjacent
  blocks when memory is released.
* `.bss` is not part of the flat image the kernel loads, so the allocator
  first moves the break past `__heap_start` (defined in `src/home/prog.ld`).
  The heap can therefore never overlap the program's globals.
* `string.h` adds `memmove`, `memcmp`, `memchr`, `strncpy`, `strncat`,
  `strcat`, `strncmp`, `strchr`, `strrchr`, `strstr`, `strnlen`, `strdup`
  and `strndup`.
* `stdlib.h` brings `exit()`/`abort()` (back to the shell), `qsort()`,
  `abs()` and a `getenv()` that always returns NULL, since there is no
  environment yet.
* number parsing (`strtol`/`strtoul`/`strtoll`/`strtoull`, `strtod`/`strtof`/
  `strtold`), `strerror`, `realpath` and `getcwd`/`access`.
* `ctype.h` provides the usual character classification and case conversion.
* the POSIX-named wrappers `open`/`read`/`write`/`lseek`/`close`/`unlink` sit
  on top of the file-handle syscalls, and `errno` is a plain global.
* `setjmp`/`longjmp` live in `src/home/lib/setjmp.asm` so the callee-saved
  registers are captured exactly.
* `time()` reports the Unix epoch (there is no wall clock yet); `localtime`,
  `gmtime` and `strftime` convert it, and `mprotect`/`sysconf` are stubs.

`run libctest` exercises all of it and prints one `PASS`/`FAIL` line per check.

Still missing for a full hosted toolchain: an executable loader with ELF
input, a richer filesystem (free-space allocation, more than one writable
handle), and a wall clock.

## The C compiler (TinyCC)

`run tcc` is an on-device C compiler. It embeds a pruned [TinyCC
0.9.28rc](https://github.com/TinyCC/tinycc) core (i386 target) vendored under
[`thirdparty/tinycc/`](../thirdparty/tinycc) — the preprocessor, code
generator, assembler, ELF writer and in-memory relocator, but none of the
other architectures or the `tcc` command-line driver.

It is built as `bin/libtcc.a` by a dedicated `make` rule and linked only into
the `tcc` program, so the rest of the image is unaffected. TinyCC wants a few
things a freestanding program does not normally have — x87 for `long double`
constant folding, `setjmp`/`longjmp` for error recovery, 64-bit division from
`libgcc`, and its own `dlopen` stubs. `thirdparty/tinycc/config.h` pins the
target to i386 and turns on `CONFIG_TCC_STATIC`, which is why the compiler
needs no `<dlfcn.h>`.

`tcc` runs in three modes:

* `run tcc <file.c> [args...]` compiles `<file.c>` straight off the disk,
  relocates it in memory, and calls its `main(argc, argv, NULL)` with the
  arguments that followed the file name (so the compiled program sees
  `argv[0]` as the file name). It prints `tcc: main() -> N` with the return
  value. A sample source, `demo.c`, is seeded on the disk image by
  `tools/fs_seeder.c`: `run tcc demo.c fib` compiles it and runs it with
  `argv[1] == "fib"`.
* `run tcc <file.c> -o <out.bin>` links `<file.c>` into a *flat binary* and
  writes it to the disk; it does not run it. `run tcc demo.c -o out.bin`
  followed by `run out.bin fib` compiles, links, writes, loads and runs a
  program entirely on the device.
* `run tcc` with no arguments is the original self test: it compiles a small
  program from memory and calls the generated `add()`/`fib()`.

Generated code is linked with `-nostdlib`, so anything a compiled file calls
must be registered with the tcc linker. `add_libc()` in `src/home/tcc.c` does
this for the common subset of `libc.a` that lives in this image (the `printf`
family, `FILE` streams, the allocator, the `str*`/`mem*` helpers and the
`open`/`read`/`write`/`lseek` wrappers); other symbols fail at relocation with
an undefined-symbol message.

### Flat binaries (`-o`)

Unlike run mode, a `-o` image cannot call into the compiler's own process, so
the driver compiles a small embedded runtime (`flat_runtime` in
`src/home/tcc.c`) together with the source, links the on-disk `libc.a`
archive, and bakes the whole image for the fixed link address
`TCC_FLAT_BASE`. The archive is seeded by `tools/fs_seeder.c`, so a flat
image gets the full userspace library (`printf`, the `FILE` layer, `malloc`,
`puts`, `exit`, `abs`, ...) just like a link-time program; the embedded
runtime adds only what the archive does not export:

* the syscalls `print()` and `putchar()` — via the `int $0x80` trap directly,
  because there are no headers on the device disk to `#include`;
* `print_dec()` for printing a signed integer;
* the string staples `strlen`, `strcmp`, `atoi`, `memset`, `memcpy`, `strcpy`.

Two pieces of machinery make the archive link work:

* `flat_intrin` — word-at-a-time `__udivdi3`/`__umoddi3`/`__divdi3`/`__moddi3`
  plus the 64-bit shift helpers, compiled into the image. The archive's `%ll`
  formatting (and any `long long` math in the source) needs them, and there is
  no libgcc on the device. Run mode compiles the same copy, so 64-bit math
  works there too.
* `__heap_start` — the archive's allocator anchors its break at this symbol.
  tcc anchors it to `TCC_FLAT_BASE`, and the shell's loader raises the break
  to just past the image (`vm_note_load((TCC_FLAT_BASE - USER_BASE) + fsize)`
  in `shell.c`), so the anchor only acts as a floor and the heap grows above
  the image.

`run` recognises the 12-byte header the compiler prepends (`"LUNB"` magic +
u32 image size + u32 `_start` offset), copies the image to `TCC_FLAT_BASE`
and enters it with the usual argv frame. `demo.c` works in both modes because
it sticks to `print()`/`print_dec()`; the seeded `print.c` exercises the
full-library path — `run tcc print.c -o print` then `run print hello` prints
`print.c: argc=2`, `print.c: argv[1]=hello` and a 64-bit value from `%llu`.

Limits:

* the image plus its header must stay under the disk's 64K writable-file
  buffer (`TCC_FLAT_MAX`) — a typical printf-style image is 12-20K;
* the kernel rejects an image that would reach the user stack;
* a compiled file may declare `main` as `int main(void)`,
  `int main(int argc, char **argv)` or the three-argument form — the `_start`
  stub calls it with `(argc, argv, NULL)` either way.

The `"LUNB"` layout is shared with the kernel through `TCC_FLAT_BASE` and
friends — see `src/headers/user.h` and `src/home/lib/user_api.h`. See `@tcc`
in `todo.txt` for the remaining steps toward a full on-device toolchain.

## Constraints

* Freestanding C99 with `-nostdlib`. There is no hosted C runtime; programs
  use the userspace C library described above.
* No SSE or MMX (`-mno-sse -mno-mmx`). The x87 unit is available: the kernel
  clears `CR0.EM` and runs `fninit` at boot, so `float`, `double` and
  `long double` work — TinyCC relies on this.
* Binaries must fit within `USER_PROG_MAX`.
* Flat binaries (`tcc -o`) are loaded at `TCC_FLAT_BASE` instead of
  `USER_BASE` and are instead capped by `TCC_FLAT_MAX`.
* Code and data are linked at the userspace load address defined by
  `src/home/prog.ld`.
* The initial stack is provided by the kernel at `USER_STACK`.
* Syscall pointer arguments must refer to memory mapped in the calling
  process's address space.
* Userspace executes at DPL 3. Privileged instructions cannot be executed from
  Ring 3 and will generate a general-protection fault.
* Invalid memory accesses can generate page faults.
* Do not return from `_start`; call `sys_exit()` or `sys_exit_to()`.

## What userspace cannot do

Ring 3 programs cannot directly access kernel-only facilities such as:

* ATA I/O ports
* VGA hardware registers
* interrupt controller registers
* page tables
* kernel memory
* privileged CPU instructions

Instead, programs request kernel services through syscalls.

For example, a program does not write directly to VGA memory. It calls:

```c
sys_write("hello\n", 6);
```

or:

```c
sys_putchar_at('@', 40, 12, USER_COLOR(C_BLACK, C_GREEN));
```

The kernel performs the privileged work on its behalf.

## Adding a new syscall

1. Pick the next syscall number and add it to
   `src/home/lib/user_api.h` (`SYS_*` define plus an inline wrapper).
2. Add a `case` to `syscall_handler()` in `src/kernel/syscall.c`.
3. Read arguments from `r->ebx`, `r->ecx`, `r->edx`, and `r->esi`.
4. Place the return value in `r->eax`.
5. Ensure any user pointers passed to the kernel are handled within the
   calling process's address space.
6. Update the syscall table in this document and in `docs/index.md`.

Kernel helpers such as `term_putchar()` and `wm_blit()` can be used by syscall
implementations where appropriate.

## Current userspace model

LunariaOS currently provides a simple userspace environment rather than a
complete process model.

A program has:

* its own page directory
* its own Ring 3 address space
* a fixed initial stack
* a flat executable image
* access to kernel services through `int 0x80`

Programs currently execute one at a time from the shell. Calling
`sys_exit()` returns control to the shell rather than returning to a scheduler.

This model is intentionally small and provides the foundation for future
features such as a userspace heap, multiple processes, dynamic memory
allocation, executable formats, and more complete process management.
