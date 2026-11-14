# LunariaOS 

LunariaOS is a **32-bit x86 hobby operating system** written from scratch in C and assembly. It's a learning project — no code copied from Linux, BSD, or other OSes.

**New here? Start with the [Concepts Guide](docs/concepts.md)** — it explains everything from "what is an OS?" to paging, syscalls, and filesystems, assuming zero OS background.

## What It Does (Features)

| Area | What's Implemented |
|------|-------------------|
| **Boot** | Custom 512-byte bootloader → protected mode → kernel |
| **CPU** | GDT (privilege rings), IDT (interrupts), paging (virtual memory) |
| **Drivers** | Keyboard (IRQ), VGA text (80×25), ATA disk (PIO), PIT timer |
| **Filesystem** | LUNFS1 — simple flat FS with directories, 16 MB disk image |
| **Shell** | Interactive prompt, 15+ commands, output redirection (`echo hi > file`) |
| **User Programs** | Run in Ring 3, demand-paged, own address space per run |
| **Syscalls** | 17 calls: console, files, heap, screen drawing, exit/reboot |
| **Userspace Libs** | `malloc`/`free`, `string.h`, `ctype.h`, `stdio.h` (`printf`!), `mirror.h` (TUI) |
| **Apps** | `edit` (vi-like editor), `paint` (drawing), `mirrordemo` (TUI), `tcc` (C compiler!) |
| **On-Device Compiler** | TinyCC embedded — `run tcc demo.c` compiles & runs in RAM; `run tcc x.c -o x` writes flat binary |

## Quick Start

### Prerequisites (Linux / WSL / macOS)
```bash
# Ubuntu/Debian
sudo apt install nasm gcc binutils qemu-system-x86

# Fedora
sudo dnf install nasm gcc binutils qemu

# Arch
sudo pacman -S nasm gcc binutils qemu-base
```

### Build & Run
```bash
make          # Builds bin/OS.bin and bin/disk.img
make run      # Boots in QEMU GUI window
make headless # Boots in terminal (curses) — Ctrl+number keys may not work
```

### First Commands to Try
```bash
help          # List all commands
ls            # List filesystem
cat hello     # Print a file
run hello     # Run a user program (prints, writes file, reads key)
run edit      # Text editor (press ESC for : commands)
run paint     # Drawing app (arrows, space to draw, [ ] colors, s=save)
run tcc demo.c fib  # Compile & run C code ON THE OS
```

## Documentation

| Doc | Audience | What You'll Learn |
|-----|----------|-------------------|
| **[Concepts Guide](docs/concepts.md)** | **Beginners** | Every OS concept from zero: boot, rings, paging, syscalls, FS, VGA... |
| **[Technical Reference](docs/index.md)** | Developers | Dense spec: memory map, GDT/IDT tables, syscall table, disk layout |
| **[Userspace Guide](docs/userspace.md)** | App writers | How to write C programs for LunariaOS: headers, syscalls, Mirror TUI, libc |
| **[Contributing](docs/contributing.md)** | Contributors | Build system, code style, adding kernel modules/user programs |

## Project Status

🧪 **Experimental / Learning Project** — Not for production use.  
Expect bugs, missing features, and breaking changes.  
See `todo.txt` for planned work (paging improvements, ELF loader, multiprocess, wall clock, etc.).

## License

GPL-3.0 — see [LICENSE](LICENSE). Copyleft applies.
