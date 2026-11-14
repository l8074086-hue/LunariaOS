# LunariaOS: The Complete Architecture & Implementation Book

Understanding how an operating system functions from the ground up can feel daunting when faced with low-level computer science terms like protected mode, interrupt vectors, or virtual memory paging. Operating systems act as the primary bridge between raw, physical computer hardware and human-written applications. LunariaOS is a hand-rolled, experimental 32-bit (`x86_32`) hobby operating system written entirely from scratch in C and Assembly. It was created specifically as an educational platform to demonstrate how a computer boots, controls hardware devices, isolates memory, and compiles software on the machine itself.

This guide breaks down every layer of LunariaOS step by step. Rather than assuming prior experience with low-level assembly or kernel design, each chapter defines essential concepts, anchors them in clear analogies, and details the exact software structures used to implement them across the codebase.

---

## 1. Introduction to Operating System Design and LunariaOS

An operating system is a master control program responsible for two fundamental tasks: managing physical hardware resources (such as memory, processor time, and disk drives) and providing a safe, standardized environment for applications to run. Without an operating system, every individual software program would have to contain its own custom routines to communicate directly with hardware chips.

Most modern operating systems are massive codebases built over decades by thousands of engineers. LunariaOS takes a minimalist, hand-rolled approach. It is built as a monolithic kernel, meaning the core operating system services—device drivers, memory management, and filesystem handling—all run together in a single high-privilege hardware mode.

### System Architecture Layers

The architecture of LunariaOS is structured into distinct functional layers. Each layer relies on the services directly beneath it, moving from physical silicon up to interactive user applications.

| System Layer | Primary Components & Subsystems | Core Responsibilities & Functionality |
| :--- | :--- | :--- |
| **User Applications** | Shell Interface, `edit` Text Editor, `tcc` Compiler | Provides interactive user commands, file editing, and on-device program compilation. |
| **User Libraries** | Mirror TUI Library, Userspace `libc.a` | Offers text user interface widgets, dynamic memory (`malloc`), string functions, and `stdio`. |
| **System Call Interface** | `open`, `read`, `write`, `seek`, `close`, `unlink` | Bridges Ring 3 user programs to Ring 0 kernel services via hardware-enforced routines. |
| **Kernel Core** | Physical Memory Manager, Paging, IDT, Custom FS | Manages physical RAM frames, per-process virtual memory spaces, interrupts, and disk files. |
| **Hardware Drivers** | VGA Video Buffer, Keyboard IRQ Driver, ATA Controller | Interacts directly with physical device ports, memory-mapped video RAM, and disk blocks. |
| **Physical Hardware** | x86_32 CPU, RAM, Disk Storage, Display & Keyboard | Executes raw machine instructions and provides physical memory and storage media. |

---

## 2. Hardware Bootstrapping and 32-Bit Protected Mode

When an x86 computer powers on, the central processing unit (CPU) starts in a historical backward-compatibility state known as **16-bit Real Mode**. In Real Mode, the processor can only address 1 Megabyte of memory, has no memory protection features, and allows any program to crash the entire system. To transform the machine into a modern multitasking computer, LunariaOS must bootstrap the hardware into **32-bit Protected Mode**.

### The Bootstrapping Sequence

Bootstrapping is the step-by-step startup process where a simple program loads a progressively more complex program. LunariaOS implements its own custom bootloader to execute this transition.

1. **Power-On Reset**: The CPU powers on, initializes hardware registers, and reads the first 512-byte sector of the boot drive into physical memory.
2. **Bootloader Execution**: The custom bootloader code—written in x86 Assembly (NASM)—begins running in 16-bit Real Mode.
3. **Global Descriptor Table (GDT) Setup**: The bootloader configures a data structure that defines how memory segments are mapped and protected.
4. **Enabling the A20 Line**: The bootloader sends hardware signals to unlock memory lines above 1 Megabyte.
5. **Switching Control Flags**: The bootloader sets the protection enable bit in CPU control register zero (`CR0.PE = 1`).
6. **Far Jump to Protected Mode**: A special assembly instruction flushes the 16-bit instruction queue and jumps directly into the 32-bit kernel entry point.

### Real Mode vs. 32-Bit Protected Mode

The transition from Real Mode to Protected Mode unlocks hardware features that are essential for modern kernel stability and security.

| Feature Dimension | 16-Bit Real Mode (Power-On State) | 32-Bit Protected Mode (LunariaOS Kernel) |
| :--- | :--- | :--- |
| **Addressable RAM** | Up to 1 Megabyte ($2^{20}$ bytes) | Up to 4 Gigabytes ($2^{32}$ bytes) |
| **Memory Protection** | None (programs can overwrite kernel RAM) | Hardware-enforced memory isolation |
| **Register Width** | 16-bit registers (`AX`, `BX`, `CX`, `DX`) | 32-bit registers (`EAX`, `EBX`, `ECX`, `EDX`) |
| **Privilege Levels** | Single unrestricted mode | Multi-ring hierarchy (Ring 0 vs. Ring 3) |

---

## 3. Interrupt Handling and Hardware Device Drivers

Computers interact with the outside world through hardware devices like keyboards, video screens, and storage drives. Because hardware devices operate at different speeds than the main processor, the operating system uses **Interrupts** to handle events asynchronously. An interrupt acts like a doorbell: when a key is pressed, the hardware pauses CPU execution, jumps to a designated handling function, processes the keypress, and resumes previous work.

### Interrupt Descriptor Table (IDT)

LunariaOS implements an **Interrupt Descriptor Table (IDT)** to manage CPU interrupts and hardware Interrupt Requests (IRQs). The IDT is an array of memory pointers that maps numerical interrupt codes to specific function handlers in the kernel.

* **CPU Exceptions (Interrupts 0–31)**: Internal processor events, such as divide-by-zero errors or page faults, which trigger kernel recovery routines.
* **Hardware IRQs (Interrupts 32–47)**: External physical events generated by expansion chips, such as keypresses or disk transfer completions.
* **System Call Vector**: A software interrupt channel that user applications trigger to request privileged services from the kernel.

### Built-in Device Drivers

The kernel includes direct C drivers for three fundamental hardware interfaces:

1. **Keyboard Driver**: Listens for hardware interrupts generated whenever a user presses or releases a physical key. It converts raw scan codes sent by the keyboard controller into ASCII characters and stores them in an input buffer for user applications.
2. **VGA Text Rendering Driver**: Instead of relying on complex graphical graphics cards, LunariaOS uses the standard x86 VGA text buffer mapped directly to physical address `0xB8000`. To render a character on screen, the driver writes a two-byte sequence to this memory region: one byte for the ASCII character code and one byte for text color attributes.
3. **ATA Disk Driver**: To store data persistently, the kernel includes an Advanced Technology Attachment (ATA) hard disk driver. The driver communicates directly with disk controller hardware ports, sending commands to read and write physical 512-byte disk blocks (sectors).

---

## 4. Memory Management, Paging, and Privilege Isolation

Physical RAM is a large linear sequence of memory bytes. Without active organization, programs would overwrite each other's data. LunariaOS divides memory management into two distinct layers: tracking raw physical memory frames and creating virtual, per-process memory views.

### Physical Memory Manager (PMM)

The **Physical Memory Manager (PMM)** acts as the system's primary RAM inventory manager. Physical memory is divided into fixed-size chunks called **frames**, each measuring 4,096 bytes (4 Kilobytes). The PMM uses bitmap tracking to record whether each physical frame is free or currently allocated to a kernel structure or user process.

### Per-Process Virtual Memory Paging

To protect processes from interfering with one another, LunariaOS uses x86 hardware **paging**. Paging maps virtual addresses (the addresses used inside software code) to physical RAM frames (the actual silicon storage).

When a program runs, the CPU consults a process-specific **Page Directory**. This gives every process the illusion that it owns a clean, continuous 4-Gigabyte address space, even if its physical data is scattered across different physical frames in RAM.

> **Virtual Memory Mapping Example**:
> * **Process A**: Virtual Page `0x0800` maps to Physical Frame `0x1200`.
> * **Process B**: Virtual Page `0x0800` maps to Physical Frame `0x3900`.
> 
> Even though both processes reference the same virtual address (`0x0800`), the hardware paging table directs their requests to completely isolated physical frames in RAM.

### Hardware Privilege Isolation (Rings)

The x86 architecture defines four security rings, though LunariaOS utilizes two primary privilege levels:

* **Ring 0 (Kernel Mode)**: The highest privilege level. The kernel runs in Ring 0 with unrestricted access to hardware ports, control registers, and all physical RAM.
* **Ring 3 (User Mode)**: The restricted execution level where user software runs. If a Ring 3 program attempts to access kernel memory directly, the CPU hardware generates a Page Fault exception, stopping the unauthorized access immediately.

---

## 5. Filesystem Architecture and Storage Primitives

Persistent storage allows programs and data to remain available even after the computer is powered down. LunariaOS implements a custom, lightweight filesystem designed for low overhead and straightforward metadata inspection.

### Disk Formatting with `fs_seeder`

During the operating system build process, a dedicated host tool written in Python called `fs_seeder` prepares the initial disk image. The `fs_seeder` utility formats raw storage space, constructs root directory index tables, and embeds default files, user utilities, and system libraries directly into the disk image.

### Low-Level File Descriptor Primitives

Applications interact with storage through low-level system call primitives. These functions manipulate **file descriptors**—integer handles that represent open file streams inside the kernel.

| Primitive Function | Operational Mechanism & Behavior |
| :--- | :--- |
| **`open(path, flags)`** | Searches directory tables for the matching path string and returns a file descriptor handle. |
| **`read(fd, buf, count)`** | Transfers bytes from the disk sector offset assigned to descriptor `fd` into a memory buffer. |
| **`write(fd, buf, count)`** | Writes bytes from memory to disk sectors assigned to `fd`, allocating disk blocks as required. |
| **`seek(fd, offset, whence)`** | Adjusts the internal byte offset pointer for descriptor `fd`, controlling read/write positioning. |
| **`close(fd)`** | Releases the file descriptor handle and flushes pending metadata changes to physical disk storage. |
| **`unlink(path)`** | Removes a file's directory entry and marks its allocated storage blocks as free for future use. |

---

## 6. The Userspace C Library (`libc`) and Standard I/O

Writing applications directly against low-level system call handles can be cumbersome. To provide a standard C programming environment, LunariaOS includes its own custom userspace C library, compiled and archived into a static library named `libc.a`.

### Core Memory and String Utilities

The custom `libc` provides foundational helper modules required by C software:

* **Dynamic Memory (`malloc`)**: Manages the process heap space, allocating dynamic memory blocks for programs when requested and maintaining memory chunk headers.
* **String Utilities (`string`)**: Implements functions for text buffer manipulation, including string copying (`strcpy`), concatenation (`strcat`), length calculation (`strlen`), and byte comparisons (`strcmp`).
* **Character Classification (`ctype`)**: Offers functions to test individual character properties, such as identifying digits (`isdigit`), letters (`isalpha`), or whitespace (`isspace`).

### Standard Stream Layer (`stdio`)

The `stdio` library builds a buffered abstraction on top of raw file descriptors. Instead of operating on raw integer handles, programs use `FILE` stream pointers that manage internal memory buffers for smoother reading and writing.

1. **User Application Call**: Software calls high-level functions like `printf("Hello %s", name)` or `fprintf(fp, ...)`.
2. **`stdio` Stream Buffering**: The stream layer formats text data and accumulates output in an in-memory stream buffer.
3. **Low-Level System Calls**: When the buffer fills or is flushed, `stdio` invokes the underlying `write(fd, buffer, length)` primitive.
4. **Kernel ATA Driver**: The kernel driver writes the data bytes directly to the physical disk sectors or display hardware.

### Standard I/O Function Summary

* **`fopen` / `fclose`**: Allocate and destroy `FILE` stream instances, initializing memory buffers alongside the underlying system file descriptor.
* **`fread` / `fwrite`**: Perform buffered block input and output transfers, minimizing expensive raw hardware disk calls.
* **`fseek`**: Repositions the internal `FILE` stream offset pointer.
* **`printf`**: Parses formatted text string specifiers (such as `%s`, `%d`, `%x`) and renders the resulting text stream to standard output.

---

## 7. Interactive Shell, Mirror TUI, and Userspace Tools

Once the kernel finishes booting, it starts the **Shell**—an interactive command-line interface that allows users to type commands, inspect system status, and launch programs.

### Built-in Shell Commands

The shell provides several built-in commands for interacting with the operating system:

* **`run <program>`**: Loads a flat binary executable from the filesystem, creates a new process address space with custom paging tables, and transfers execution to the program.
* **`mem`**: Queries the Physical Memory Manager to display system RAM metrics, showing total allocated versus free physical frames.

### The Mirror Text User Interface (TUI) Library

To support interactive visual programs without a full graphical desktop, LunariaOS includes **Mirror**—a custom Text User Interface framework. Mirror allows programmers to construct terminal-based window layouts, drawing borders, text input fields, buttons, and visual selection menus using colored text attributes.

### The `edit` Text Editor

Using the Mirror TUI library, LunariaOS provides a built-in text editor called `edit`. The `edit` utility runs entirely in userspace, opening text files via `fopen`, rendering interactive editing windows with Mirror, processing keyboard input events, and saving modified buffers back to disk with `fwrite`.

---

## 8. On-Device Compilation with Embedded TinyCC (`tcc`)

One of the most notable architectural capabilities of LunariaOS is its ability to compile C source code directly on the running machine without requiring a host computer. This is achieved by embedding a custom port of **TinyCC (`tcc`)** into the operating system.

### How On-Device Compilation Works

TinyCC is an extremely fast, lightweight C compiler. The compilation workflow on LunariaOS follows a complete self-contained execution pipeline:

1. **Write Source Code**: The user opens the built-in text editor (`edit`) to write C source code (such as `hello.c`) and saves it to the filesystem.
2. **Invoke the Compiler**: The user runs the shell command `run tcc hello.c -o hello`.
3. **Parsing & Machine Code Generation**: TinyCC reads `hello.c`, parses the syntax, constructs an Abstract Syntax Tree (AST), and emits x86 machine code instructions directly into memory.
4. **Linking Against `libc.a`**: TinyCC links the generated binary image against the on-disk `libc.a` static archive to resolve standard C library symbols.
5. **Output Flat Binary**: The compiler writes the finished executable to disk as a flat binary file named `hello`.
6. **Execution**: The user launches the newly created application immediately using the shell command `run hello`.

---

## 9. Build Infrastructure, Language Stats, and Execution Pipeline

Building an entire operating system requires a specialized toolchain to assemble low-level machine code, compile C modules, and construct bootable disk images.

### Repository Language Composition

The LunariaOS codebase relies on a precise mix of programming languages, build scripts, and configuration specifications:

| Language / File Format | Codebase Percentage | Functional Role in the Repository |
| :--- | :--- | :--- |
| **C** | **88.4%** | Primary language for kernel subsystems, device drivers, `libc`, Mirror TUI, and `edit`. |
| **Python** | **4.9%** | Build automation tools, including the `fs_seeder` filesystem image generator. |
| **Makefile** | **3.0%** | Compilation rules, dependency tracking, and target build orchestration. |
| **Assembly (x86 NASM)** | **2.8%** | Low-level bootstrapping code, Real Mode entry routines, and context switching. |
| **Shell Scripting** | **0.5%** | Execution helper scripts for running release images (`run.sh`, `cat.sh`). |
| **Linker Script** | **0.3%** | Defines physical memory layout addresses for kernel and userspace binary images. |
| **Nix** | **0.1%** | Declarative development shell environment configuration (`shell.nix`). |

### Host Build Requirements

To assemble and compile LunariaOS from source, the host system requires standard cross-development build tools:

* **NASM (Netwide Assembler)**: Assembles 16-bit and 32-bit x86 assembly source files into object code.
* **GCC / Clang**: Compiles C source files with specific flags (`-ffreestanding`, `-nostdlib`) to ensure no host operating system assumptions are included.
* **GNU Binutils**: Provides binary linkers (`ld`) and archive tools (`ar`) to assemble `libc.a` and final executable images.
* **QEMU Emulator**: Simulates x86 hardware execution for running and testing the operating system image.

### Building and Launching LunariaOS

The project's `Makefile` defines two primary commands for launching the OS inside the QEMU emulator:

```bash
# Launch LunariaOS inside a graphical QEMU display window
make run

# Launch LunariaOS inside a headless command-line terminal emulator
make headless
```

Running `make run` compiles all assembly routines, C drivers, kernel modules, `libc.a`, and user applications; executes `fs_seeder` to build the filesystem disk image; and boots the resulting system inside QEMU. This provides a complete hands-on environment for exploring low-level operating system development.
