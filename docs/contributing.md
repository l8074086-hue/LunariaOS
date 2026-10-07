# Contributing to LunariaOS

Thanks for wanting to help! This page covers how the project is organized,
how to build it, and how to add code without breaking the build.

## Project layout

```
bin/                  build output (kernel images, objects, disk.img)
src/
  boot/               bootloader (16-bit real mode -> protected mode)
  kernel/             core kernel: entry, GDT, IDT, ISRs, shell, wm, syscalls
  drivers/            hardware drivers: VGA, keyboard, ATA, filesystem
  headers/            kernel headers shared by kernel and drivers
  lib/                kernel string library (header-only)
  home/               user programs + user-space headers
    lib/              user API: syscall wrappers, POSIX headers, stdio, C library
tools/                host tools (fs_seeder.c) built and run on Linux
thirdparty/
  tinycc/             vendored TinyCC core for the on-device C compiler
docs/                 documentation
```

## Build requirements

- NASM
- GCC / clang
- GNU binutils (`ld`, `objcopy`)
- QEMU (to run)

Build and run:

```sh
make          # build OS.bin and disk.img
make run      # QEMU with a GUI window
make headless # QEMU in a curses terminal
make clean    # remove bin/
```

## How the build works

The Makefile compiles everything freestanding for 32-bit x86:

- `src/kernel/*.asm` -> object files with NASM
- `src/kernel/*.c`, `src/drivers/*.c` -> objects with `cc -m32 -ffreestanding
  -nostdlib` (no libc, no stack protector, no SSE/MMX; the x87 unit is enabled)
- `src/kernel/linker.ld` links the kernel to `0x7E00`
- `objcopy -O binary` strips the kernel to a flat binary
- `src/home/*.c` -> flat user binaries linked at `0x100000` by `src/home/prog.ld`
- `src/home/lib/mirror.c` -> `bin/libmirror.a`, a static userspace library
  linked into every user program; archive members are only pulled in when the
  program references them, so other programs pay nothing for it
- `src/home/lib/libc.c`, `stdio.c` and `setjmp.asm` -> `bin/libc.a`, the
  userspace C library (allocator, `string.h`, `ctype.h`, `stdlib.h`, `FILE`
  streams, POSIX wrappers), linked the same lazy way
- `thirdparty/tinycc/` -> `bin/libtcc.a`, the embedded TinyCC core, linked only
  into the `tcc` program. `thirdparty/tinycc/conftest.c` is built on the host
  as `c2str` to turn `include/tccdefs.h` into `tccdefs_.h`
- `tools/fs_seeder.c` (a host Linux program) writes the superblock, sample
  files, and the user programs into `bin/disk.img`

`bin/OS.bin` is just `boot.bin` concatenated with `kernel.bin`; `disk.img` is
that pair placed at sectors 0 and 1 of a 16 MB image, with the filesystem
seeded above LBA 128.

## Adding a new kernel module

Kernel code goes in `src/kernel/`, driver code in `src/drivers/`. Headers go
in `src/headers/`.

1. Write `src/kernel/foo.c` (or `src/drivers/foo.c`) and `src/headers/foo.h`.
2. Add an explicit rule in the Makefile mirroring the existing ones, e.g.:

   ```make
   $(BUILD_DIR)/foo.o: $(KERNEL_DIR)/foo.c | $(BUILD_DIR)
   	$(CC) $(CFLAGS) -c $< -o $@
   ```

3. Add `$(BUILD_DIR)/foo.o` to the `kernel.elf` target's dependency/object
   list 

Dependency files (`bin/*.d`) are generated automatically, so header changes
trigger rebuilds. Remember that the kernel must stay **smaller than 128
sectors** — it must never overlap the filesystem superblock at LBA 128.

## Adding a user program

Drop a `src/home/myprog.c` file into the directory. The Makefile wildcard
`src/home/*.c` picks it up automatically; it is compiled, linked at
`0x100000`, and embedded into the disk image under the name `myprog`.
Run it in the shell with `run myprog`.

See [Writing userspace software](userspace.md) for the full guide.

## Code style and constraints

- **Language**: C99 (`-std=c99`). Kernel code is freestanding — no libc, no
  `printf`, no `malloc`, no standard headers (except a few like `<stdint.h>`);
  user programs get the userspace C library instead.
- **No SSE/MMX**: everything is compiled with `-mno-sse -mno-mmx`. x87 is
  enabled (`fninit` runs at boot), so `float`/`double`/`long double` work;
  TinyCC relies on `long double` for constant folding.
- **No comments unless they add real value**; the existing code keeps them
  minimal.
- Headers under `src/headers/`, `src/lib/`, and `src/home/lib/` should keep
  the include-guard style already used (`#ifndef X_H / #define X_H`).
- Small helper functions may live as `static inline` in headers (that is how
  `string.h`, `port.h`, and `user_api.h` work today).
- Hardware access goes through `src/headers/port.h` (`inb`/`outb`/`inw`/`outw`).
- Keep the system buildable with `make` before opening a PR; test with
  `make run` or `make headless`.
- `make test` boots the image headless and runs the regression scenarios in
  `tests/` (they drive the shell over the QEMU monitor and check the screen).

## Licensing

LunariaOS is released under the **GPL-3.0** (see `LICENSE`). By contributing
you agree that your code is licensed under the GPL-3.0 and your name appears
in the copyright notice. No selling of the project as proprietary software —
copyleft is the point.

## What to work on

`todo.txt` in the repo root tracks planned work, one task per line as
`(P) YYYY-MM-DD <what to do> +<Project> @<context>`, where `P` is a priority
from `A` (blocking) to `E` (whenever). When a task is finished, move its line
out of `todo.txt` into `done.txt` and prefix it with `x YYYY-MM-DD` in place
of the priority. Keep items specific enough to act on — point at the file and
function you mean. The README also lists the feature set; good first tasks
include the `kprintf` and serial COM1 items, filling in the empty
`showCursor()` stub, or adding new user programs and syscalls.
