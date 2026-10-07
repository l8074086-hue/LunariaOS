# LunariaOS 

LunariaOS is a x86_32 hobby operating system completely hand-rolled from scratch in C and assembly.

## Features

- Custom bootloader
- Protected mode kernel
- IDT and interrupt handling
- Keyboard driver
- VGA text rendering
- ATA disk driver 
- Custom filesystem
- Shell
- User programs
- Basic privilege separation
- Mirror userspace TUI library
- `edit` text editor (userspace, built on Mirror)
- Userspace C library (`malloc`, `string`, `ctype`)
- File handles (`open`/`read`/`write`/`seek`/`close`/`unlink`)
- stdio `FILE` layer (`fopen`/`fread`/`fwrite`/`fseek`/`fclose`, `printf`)
- On-device C compiler — embedded TinyCC core (`run tcc`)

## Build

Requirements:
- NASM
- GCC / clang
- GNU binutils
- QEMU

For GUI run:
```make
make run
```
For QEMU headless run:
```make
make headless
```
> [!NOTE ]
> Note that workspace switching via ctrl + number keys may not work well in headless mode

## Project status

This project is entirely experimental and for learning purposes only. It is not yet mature enough for actual real world use cases and will likely not be until Version 1 fully releases.
