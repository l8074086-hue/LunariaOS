# The LunariaOS Documentation.

LunariaOS is a 32-bit x86 hobby operating system hand-rolled from scratch in
C and assembly. This document describes how the system is put together.

- [Boot Process](#boot-process)
- [Memory Layout](#memory-layout)
- [Disk Layout](#disk-layout)
- [GDT and Privilege Rings](#gdt-and-privilege-rings)
- [IDT and Interrupts](#idt-and-interrupts)
- [Keyboard Driver](#keyboard-driver)
- [VGA Text Mode](#vga-text-mode)
- [ATA Disk Driver](#ata-disk-driver)
- [Filesystem (LUNFS1)](#filesystem-lunfs1)
- [System Calls](#system-calls)
- [The Shell](#the-shell)
- [Workspace Manager and Workspaces](#workspace-manager-and-workspaces)
- [User Programs](#user-programs)
- [Building and Running](#building-and-running)
- [Contributing](contributing.md)
- [Writing userspace software](userspace.md)

## Boot Process

### 1. BIOS loads the bootloader

The bootloader ([`src/boot/boot.asm`](../src/boot/boot.asm)) is written to
sector 0 of `bin/disk.img` and must fit in 512 bytes, ending with the `0xaa55`
signature. The BIOS loads it to `0x7c00` in 16-bit real mode and jumps to it,
passing the boot drive number in `dl`.

### 2. Loading the kernel

The bootloader sets up segment registers and a stack, then uses BIOS
interrupt `0x13` (AH=0x02) to read the kernel from disk:

- `es:bx = 0x0000:0x7E00` — kernel is loaded to `0x7E00`, right after the bootloader
- `al = KERNEL_SECTORS` — number of sectors to read, computed by the Makefile at build time
- `ch = 0`, `cl = 2`, `dh = 0` — read from cylinder 0, sector 2, head 0 (sector 1 is the bootloader)
- `dl` — the boot drive the BIOS gave us

On error (`carry` flag set) it prints `DISK READ ERROR` and halts.

### 3. Switching to protected mode

A minimal GDT is defined inline in the bootloader with a flat 4 GB code
segment (selector `0x08`) and data segment (selector `0x10`). The bootloader:

1. Disables interrupts (`cli`)
2. Loads the GDT (`lgdt`)
3. Sets bit 0 of `cr0` to enable protected mode
4. Far-jumps to `CODE_SEG:init_pm`, which flushes the pipeline

### 4. Entering the kernel

In 32-bit protected mode, the bootloader sets all segment registers to the
flat data segment, points `esp` at `0x90000`, and calls the kernel at
`0x7E00`.

[`src/kernel/entry.asm`](../src/kernel/entry.asm) is the kernel entry point:
it sets up its own stack and calls `kmain()`. The kernel then initializes the
GDT, IDT, VGA text mode, workspace manager, and filesystem before launching the
shell.

### 5. Building the disk image

`make` produces `bin/OS.bin` by concatenating `boot.bin` and `kernel.bin`.
`bin/disk.img` is a 16 MB image built as follows:

```make
dd if=boot.bin of=disk.img          # sector 0
dd if=kernel.bin of=disk.img seek=1 # starts at sector 1
```

The filesystem is seeded onto the image by
[`tools/fs_seeder.c`](../tools/fs_seeder.c), which also embeds the user
programs ([`src/home/*.c`](../src/home)).

## Memory Layout

| Address    | Size     | Contents                                  |
|------------|----------|-------------------------------------------|
| `0x7C00`   | 512 B    | Bootloader (sector 0)                     |
| `0x7E00`   | varies   | Kernel (loaded from sector 1 onward)      |
| `0x90000`  |          | Kernel stack (`esp`)                      |
| `0x100000` |          | User program base (`USER_BASE`)           |
| `0x200000` |          | User stack (`USER_STACK`)                 |
| `0xB8000`  | 4000 B   | VGA text-mode framebuffer (80 x 25 x 2)   |

Kernel and user programs are both linked to these fixed addresses via
[`src/kernel/linker.ld`](../src/kernel/linker.ld) and
[`src/home/prog.ld`](../src/home/prog.ld).

## Disk Layout

The 16 MB disk image (a flat ATA drive) is divided up in
[`src/headers/fs.h`](../src/headers/fs.h):

| LBA        | Sectors | Contents             |
|------------|---------|----------------------|
| 0          | 1       | Bootloader           |
| 1 – ~127   |         | Kernel (`boot.bin` + `kernel.bin`) |
| 128        | 1       | Superblock           |
| 200        | 64      | Directory            |
| 300        |         | File data            |

The kernel must stay smaller than 128 sectors so it never overlaps the
superblock at LBA 128.

## GDT and Privilege Rings

[`src/kernel/gdt.c`](../src/kernel/gdt.c) installs a full GDT, replacing the
bootloader's minimal one, with 6 entries:

| Index | Selector | Access | Purpose        |
|-------|----------|--------|----------------|
| 0     | `0x00`   |        | Null           |
| 1     | `0x08`   | `0x9A` | Kernel code, DPL 0 |
| 2     | `0x10`   | `0x92` | Kernel data, DPL 0 |
| 3     | `0x18`   | `0xFA` | User code, DPL 3   |
| 4     | `0x20`   | `0xF2` | User data, DPL 3   |
| 5     | `0x28`   | `0x89` | TSS               |

All code/data segments are flat 4 GB (limit `0xFFFFF`, 4 KiB granularity).
The TSS provides the ring-0 stack (`esp0 = 0x90000`, `ss0 = 0x10`) used when
user code traps into the kernel. The GDT is loaded with `lgdt`, followed by a
far jump to reload `cs` and reloading the data segment registers; `ltr` loads
the TSS selector.

## IDT and Interrupts

[`src/kernel/idt.c`](../src/kernel/idt.c) remaps the 8259 PIC so IRQs start at
vector `0x20` instead of clashing with CPU exceptions, then fills the IDT:

| Vector | Source             | DPL | Handler                                        |
|--------|--------------------|-----|------------------------------------------------|
| `0x20` | IRQ 0 (timer)     | 0   | ISR 0x20 (stub, no-op)                         |
| `0x21` | IRQ 1 (keyboard)  | 0   | ISR 0x21 -> `isr_common_handler` -> keyboard   |
| `0x0D` | GPF (general protection fault) | 0 | `fault_handler` + halt            |
| `0x0E` | Page fault        | 0   | `fault_handler` + halt                         |
| `0x80` | Syscall           | 3   | `syscall_handler`                              |
| all    | default           | 0   | `isr_dummy` (just `iretd`)                     |

All other vectors point at `isr_dummy` so a spurious interrupt returns
silently instead of crashing. The PIC master is masked to only enable IRQ 1
(keyboard). Interrupt handlers live in
[`src/kernel/isr.asm`](../src/kernel/isr.asm); the common keyboard handler
sends the EOI (`0x20`) to the PIC.

## Keyboard Driver

[`src/drivers/keyboard.c`](../src/drivers/keyboard.c) reads scancodes from
port `0x60` inside the IRQ handler and pushes them into a 256-byte ring
buffer. `kbd_getc()` converts scancodes to characters, blocking until a key
is available.

Features:

- US QWERTY layout with a shift map
- Shift / Ctrl / Alt / Super modifier tracking
- Caps lock toggle (`caps_active()`)
- Extended (`0xE0`) prefix handling for arrow keys and right-side modifiers
- Arrow keys returned as `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`
- `kbd_flush()` discards buffered keys (used before entering user mode)

## VGA Text Mode

[`src/drivers/vga.c`](../src/drivers/vga.c) and
[`src/kernel/wm.c`](../src/kernel/wm.c) write to the text-mode framebuffer at
`0xB8000`. Each screen cell is 2 bytes: ASCII character + attribute byte
(high nibble background, low nibble foreground). Colors come from
[`src/headers/vga.h`](../src/headers/vga.h).

The workspace manager keeps a private `cells` buffer per terminal and copies the
current one to the framebuffer with `wm_blit()` — this is how each workspace
keeps its own screen contents. There is also a software cursor that is
drawn by flipping the attribute byte of the cursor cell.

## ATA Disk Driver

[`src/drivers/ata.c`](../src/drivers/ata.c) is a PIO-mode ATA driver using
the classic I/O ports:

| Port | Function        |
|------|-----------------|
| `0x1F0` | Data words  |
| `0x1F2` | Sector count |
| `0x1F3` | LBA low     |
| `0x1F4` | LBA mid     |
| `0x1F5` | LBA high    |
| `0x1F6` | Drive / LBA top 4 bits |
| `0x1F7` | Status / command    |

`ata_read_sectors()` and `ata_write_sectors()` transfer one 512-byte sector at
a time in 28-bit LBA mode (command `0x20` read, `0x30` write), polling the
status port for `BSY`/`DRQ` with a timeout.

## Filesystem (LUNFS1)

[`src/drivers/fs.c`](../src/drivers/fs.c) implements a simple flat
filesystem (magic `"LUNFS1"`):

- **Superblock** at LBA 128: magic, directory LBA, directory sector count, data LBA
- **Directory**: 64 sectors of fixed-size `file_entry` records
  (16 entries per sector)
- **File entry**: 23-byte name, type byte (`FS_DIR=1`, `FS_FILE=0`), start LBA, size in bytes
- **Data**: file contents stored from LBA 300 onward, written at the first free area

Operations: `fs_mount` (verifies magic), `fs_find`, `fs_ls`/`fs_ls_buf`,
`fs_read`, `fs_write` (create/overwrite, auto-allocates the next free LBA),
`fs_append` (adds a path to a directory), `fs_mkdir`, `fs_cat`, `fs_delete`.

Paths with slashes (`dir/sub`) are supported: the parent directory keeps a
newline-separated list of full child paths. On boot the disk image is
seeded by [`tools/fs_seeder.c`](../tools/fs_seeder.c), which writes the
superblock, a couple of text files, and the compiled user programs.

## System Calls

Ring-3 programs trap via `int 0x80` ([`src/kernel/syscall.c`](../src/kernel/syscall.c)).
The syscall number goes in `eax`, arguments in `ebx`/`ecx`/`edx`/`esi`.
The wrappers live in [`src/home/lib/user_api.h`](../src/home/lib/user_api.h).

| # | Name           | Args (ebx, ecx, edx, esi)              | Returns |
|---|----------------|----------------------------------------|---------|
| 1 | `write`        | buf, len                               | —       |
| 2 | `exit`         | —                                      | —       |
| 3 | `getc`         | —                                      | eax: char |
| 4 | `read_file`    | name, buf, max                         | eax: size / -1 |
| 5 | `write_file`   | name, buf, len                         | eax: 0 / -1 |
| 6 | `list`         | path, buf, max                         | eax: len / -1 |
| 7 | `exit_to`      | mode (0 shell, 1 halt, 2 power off, 3 reboot) | — |
| 8 | `clear`        | —                                      | —       |
| 9 | `putchar_at`   | c, x, y, color                         | —       |
| 10 | `goto_xy`      | x, y                                   | —       |

`exit_to` chooses what happens when the user program calls `exit`: return to
the shell, halt, power off (ACPI `0x604`/`0x2000`), or reboot (keyboard
controller reset via `0x64`/`0xFE`). Syscall 2 (`exit`) rewrites the iret
frame on the kernel stack so control returns to `exit_to_shell()`.

## The Shell

[`src/kernel/shell.c`](../src/kernel/shell.c) provides the interactive
`"sherenity > "` prompt with these commands:

| Command | Description                          |
|---------|--------------------------------------|
| `help`  | List commands                        |
| `clear` | Clear the terminal                   |
| `echo`  | Print its arguments                  |
| `read <lba>` | Dump raw disk sector at an LBA  |
| `ls [dir]` | List the filesystem (or a dir)   |
| `cat <file>` | Print a file                  |
| `mkdir <name>` | Create a directory           |
| `rm <file>` | Delete a file                  |
| `run <file>` | Load and execute a user program |
| `exit`  | Power off the machine                |

Output redirection is supported: `echo foo > file.txt` writes the output of
the command into a file, with `\n`, `\t`, and `\\` escape sequences processed.
`run` loads a program from the filesystem into `0x100000` and jumps into ring 3
via `enter_user` (see below).

## Workspace Manager and Workspaces

[`src/kernel/wm.c`](../src/kernel/wm.c) implements 10 virtual terminals, each
with its own 80 x 25 cell buffer, cursor, and input buffer. Switch between
them while holding **Ctrl**:

- `Ctrl+1` … `Ctrl+9`, `Ctrl+0` (workspaces 0–9)
- `Ctrl+Q W E R T Y U O P A` (workspaces 0–9) — alternative bindings for headless mode, number keys don't seem to work as well.

`wm_blit()` copies the active terminal's buffer to the VGA framebuffer. Note
the README warning that Ctrl+number switching may not work in QEMU's headless
curses mode.

## User Programs

User programs live in [`src/home/`](../src/home) and are compiled as flat
binaries linked at `0x100000`, then embedded into the disk image by the
`fs_seeder` as files named after the program (e.g. `hello`, `tui`).

To run one, type `run hello` (or `run tui`) in the shell. The shell reads the
file into `USER_BASE`, then calls `enter_user(eip, esp)`
([`src/kernel/entry.asm`](../src/kernel/entry.asm)), which builds a ring-3
`iret` frame:

```
ss  = 0x23  (user data, DPL 3)
esp = 0x200000
eflags (IF set)
cs  = 0x1B  (user code, DPL 3)
eip = entry point
```

The program starts at `_start` and only reaches hardware through the `int 0x80`
syscalls. Included examples:

- **hello** — syscall demo: prints, lists the root directory, writes and reads `test.txt`, reads a key, exits
- **tui** — draws a border in the terminal using `sys_putchar_at` / `sys_goto_xy`, waits for keys

## Building and Running

Requirements: NASM, GCC/clang, GNU binutils, QEMU.

```sh
make          # build bin/OS.bin and bin/disk.img
make run      # run in QEMU with a GUI window
make headless # run in QEMU with a curses terminal
make clean    # remove bin/
```
