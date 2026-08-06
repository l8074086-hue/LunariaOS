# Writing userspace software for LunariaOS

LunariaOS can run flat binaries in **ring 3** (user mode). User programs can
only touch hardware and kernel services through the `int 0x80` syscall
interface described below.

## How a program is built and run

1. Write `src/home/myprog.c`.
2. `make` compiles it with the freestanding flags, links it at `0x100000`
   (`src/home/prog.ld`), converts it to a flat binary, and embeds it into
   `bin/disk.img` under the name `myprog`.
3. In the shell, `run myprog` loads the file into memory at `0x100000` and
   drops to ring 3 via a crafted `iret` frame (user stack at `0x200000`).

Your program must export `void _start(void)`. There is no `main()`, no
return value, and no exit code — end the program by calling `sys_exit()`
(or `sys_exit_to(mode)`).

## Headers you can use

All of these are on the include path automatically:

| Header | Contents |
|--------|----------|
| `user_api.h` | syscall wrappers and constants |
| `stdio.h`    | `print()` and `putchar()` built on the syscalls |
| `string.h`   | `strlen`, `strcmp`, `strcpy`, `memset`, `memcpy`, `itoa`, `atoi` |

## Syscall API

The wrappers live in `src/home/lib/user_api.h`; each expands to
`int $0x80` with the syscall number in `eax` and arguments in
`ebx`/`ecx`/`edx`/`esi`.

| # | Function | Description |
|---|----------|-------------|
| 1 | `void sys_write(const char *buf, unsigned int len)` | Print `len` bytes of `buf` to the terminal |
| 2 | `void sys_exit(void)` | Return to the shell |
| 3 | `int sys_getc(void)` | Block until a key is pressed; returns the character |
| 4 | `int sys_read_file(const char *name, char *buf, unsigned int max)` | Read a file into `buf`; returns size, or -1 |
| 5 | `int sys_write_file(const char *name, const char *buf, unsigned int len)` | Create/overwrite a file; returns 0 or -1 |
| 6 | `int sys_list(const char *path, char *buf, unsigned int max)` | List a directory into `buf`; returns length or -1 |
| 7 | `void sys_exit_to(int mode)` | Exit and do something: `EXIT_SHELL`(0), `EXIT_HALT`(1), `EXIT_POWEROFF`(2), `EXIT_REBOOT`(3) |
| 8 | `void sys_clear(void)` | Clear the terminal |
| 9 | `void sys_putchar_at(char c, unsigned int x, unsigned int y, unsigned int color)` | Draw a character at a screen position |
| 10 | `void sys_goto_xy(unsigned int x, unsigned int y)` | Move the terminal cursor |

Useful constants:

```c
SCREEN_COLS 80   SCREEN_ROWS 25
C_BLACK ... C_WHITE            // 16 VGA colors
USER_COLOR(bg, fg)             // e.g. USER_COLOR(C_BLACK, C_GREEN)
```

Colors are the same 16-color palette as the kernel (see `vga.h`).

## Example program

```c
#include "stdio.h"
#include "string.h"

void _start(void)
{
    char buf[1024];

    print("hello from ring 3!\n");

    /* write a file */
    if (sys_write_file("demo.txt", "file contents\n", 14) == 0)
        print("wrote demo.txt\n");

    /* read it back */
    int n = sys_read_file("demo.txt", buf, sizeof buf);
    if (n >= 0)
        sys_write(buf, (unsigned int)n);

    /* draw a colored cell */
    sys_putchar_at('@', 40, 12, USER_COLOR(C_BLACK, C_GREEN));

    /* wait for a key, then leave */
    sys_getc();
    sys_exit();
}
```

Build with `make`, boot with `make run`, then type `run demo` in the shell.

## Constraints

- Freestanding C99, `-nostdlib`: no libc, no `printf`, no `malloc`, no
  standard headers beyond a few like `<stdint.h>`.
- No floating point (`-mno-sse -mno-mmx -mno-80387`).
- Code and data are linked at `0x100000`; the kernel loads at most
  `USER_PROG_MAX` (1 MB) — keep binaries small.
- Pointers you pass to syscalls must point into your own program's memory.
  The kernel trusts the addresses, but you are running at DPL 3, so any
  privileged instruction will fault (the kernel prints `FAULT 13` and halts).
- Do not return from `_start` — call `sys_exit()` / `sys_exit_to()`.

## Adding a new syscall

1. Pick the next number and add it to `src/home/lib/user_api.h`
   (`SYS_*` define + an inline wrapper).
2. Add a `case` in `syscall_handler()` in `src/kernel/syscall.c`.
   Arguments arrive in `r->ebx`, `r->ecx`, `r->edx`, `r->esi`; the return
   value goes in `r->eax`. Kernel helpers like `term_putchar` and `wm_blit`
   are available to you there.
3. Update the syscall table in this document and in `docs/index.md`.
