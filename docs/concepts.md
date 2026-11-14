# LunariaOS Concepts Guide — From Zero to OS

This guide explains **every concept** used in LunariaOS, assuming zero prior OS knowledge. If you know C but have never written a bootloader or touched assembly, start here.

---

## Table of Contents

1. [What Even Is an Operating System?](#what-even-is-an-operating-system)
2. [The Boot Process — From Power On to Your Code](#the-boot-process--from-power-on-to-your-code)
3. [Real Mode vs Protected Mode — The CPU's Two Personalities](#real-mode-vs-protected-mode--the-cpus-two-personalities)
4. [Privilege Rings — Why Your Browser Can't Format Your Disk](#privilege-rings--why-your-browser-cant-format-your-disk)
5. [The Global Descriptor Table (GDT) — The CPU's Rulebook](#the-global-descriptor-table-gdt--the-cpus-rulebook)
6. [Interrupts & The IDT — How Hardware Gets the CPU's Attention](#interrupts--the-idt--how-hardware-gets-the-cpus-attention)
7. [System Calls — How User Programs Ask the Kernel for Help](#system-calls--how-user-programs-ask-the-kernel-for-help)
8. [Virtual Memory & Paging — The Magic Trick That Makes Programs Isolated](#virtual-memory--paging--the-magic-trick-that-makes-programs-isolated)
9. [The Filesystem — Organizing Data on Disk](#the-filesystem--organizing-data-on-disk)
10. [VGA Text Mode — Drawing Text Without a GPU](#vga-text-mode--drawing-text-without-a-gpu)
11. [The Shell — Your Command Center](#the-shell--your-command-center)
12. [Building & Running — From Source to Booting OS](#building--running--from-source-to-booting-os)

---

## What Even Is an Operating System?

**Short answer:** An OS is a program that runs *other* programs and manages the hardware so they don't have to.

When you turn on a computer, the CPU starts executing code at a fixed address. There's no "OS" yet — just bare metal. The OS's job is to:

1. **Set up the hardware** (CPU mode, memory, devices)
2. **Provide services** (read disk, draw text, handle keyboard)
3. **Run user programs** safely isolated from each other and the kernel
4. **Manage resources** (CPU time, memory, disk space)

LunariaOS does all of this in ~3,000 lines of C + assembly.

### Kernel vs User Space

| Kernel Space (Ring 0) | User Space (Ring 3) |
|----------------------|---------------------|
| Can execute *any* instruction | Restricted instruction set |
| Accesses all memory | Only its own virtual memory |
| Talks to hardware directly | Must ask kernel via syscalls |
| Crashes = whole system dies | Crashes = just that program dies |
| Written in C + assembly | Written in C (freestanding) |

**The Golden Rule:** User programs *never* touch hardware directly. They ask the kernel politely via **system calls**.

---

## The Boot Process — From Power On to Your Code

### Step 0: You Press the Power Button

The CPU resets and starts executing at address `0xFFFF0` (in BIOS ROM). The BIOS (Basic Input/Output System) runs:
- Power-on self-test (POST)
- Hardware enumeration
- Finds a bootable disk (looks for `0xAA55` at sector 0)

### Step 1: BIOS Loads the Bootloader (Sector 0)

```
┌─────────────────────────────────────────────────────────────┐
│                     DISK (bin/disk.img)                     │
├────────┬────────────────────────────────────────────────────┤
│Sector 0│ Bootloader (512 bytes, ends with 0xAA55)          │
│Sector 1│ Kernel starts here                                 │
│  ...   │                                                    │
│Sector128│ Filesystem superblock                             │
└────────┴────────────────────────────────────────────────────┘
```

The BIOS loads sector 0 to **physical address `0x7C00`** and jumps there. The CPU is in **16-bit Real Mode** (like an 8086 from 1978).

### Step 2: Bootloader Loads the Kernel

Our bootloader (`src/boot/boot.asm`) does three things:

```asm
; 1. Set up a stack and segments
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7C00        ; stack grows down from here

; 2. Read kernel from disk using BIOS interrupt 0x13
mov bx, 0x7E00        ; load kernel to 0x7E00 (right after bootloader)
mov al, KERNEL_SECTORS; how many sectors (computed at build time)
mov ch, 0             ; cylinder 0
mov cl, 2             ; sector 2 (sector 1 = bootloader)
mov dh, 0             ; head 0
mov dl, [boot_drive]  ; BIOS gave us the drive number in dl
mov ah, 0x02          ; "read sectors" command
int 0x13              ; CALL BIOS
jc disk_error         ; if carry flag set = error
```

**Key insight:** The bootloader is *tiny* (512 bytes max). It can't do much — just load the real kernel and switch CPU modes.

### Step 3: Switch to Protected Mode (32-bit)

Real mode is limited: 1 MB memory, no memory protection, 16-bit registers. We need **Protected Mode** for:
- 4 GB address space
- Memory protection (paging)
- Privilege rings

```asm
; Disable interrupts while we mess with CPU state
cli

; Load the GDT (Global Descriptor Table) — defines memory segments
lgdt [gdt_descriptor]

; Set bit 0 of CR0 (control register 0) — THE MAGIC BIT
mov eax, cr0
or eax, 1
mov cr0, eax

; Far jump to flush pipeline and load new CS selector
jmp CODE_SEG:init_pm
```

After this `jmp`, the CPU is in **32-bit Protected Mode**. `init_pm` is a label in the same file but now executing as 32-bit code.

### Step 4: Enter the Kernel

```asm
[bits 32]           ; Tell NASM we're 32-bit now
init_pm:
    mov ax, DATA_SEG    ; Load data segment selector into all segment regs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000    ; Set up kernel stack (grows down from 0x90000)
    call KERNEL_ADDR    ; Call kernel at 0x7E00 (where we loaded it)
    jmp $               ; Hang if kernel returns
```

The kernel entry point (`src/kernel/entry.asm`) is minimal:

```asm
[bits 32]
[extern kmain]        ; kmain is defined in kernel.c
global _start
_start:
    mov esp, 0x90000  ; Set up stack again (kernel's own stack)
    call kmain        ; Jump to C code!
    jmp $             ; Never returns
```

**Now we're in C code (`kmain` in `src/kernel/kernel.c`).** The OS is alive.

---

## Real Mode vs Protected Mode — The CPU's Two Personalities

### Real Mode (16-bit, the default)
- **Memory:** 20-bit addresses = 1 MB max (`segment << 4 + offset`)
- **Segments:** Overlapping, no protection
- **Registers:** 16-bit (`ax`, `bx`, `si`, `di`, `sp`, `bp`, `ip`)
- **Privilege:** Everything runs at max privilege
- **Use case:** Bootloaders, DOS, BIOS

### Protected Mode (32-bit, what we want)
- **Memory:** 32-bit addresses = 4 GB
- **Segments:** Descriptor tables (GDT/LDT) with base, limit, permissions
- **Registers:** 32-bit (`eax`, `ebx`, `esi`, `edi`, `esp`, `ebp`, `eip`)
- **Privilege:** 4 rings (0 = kernel, 3 = user)
- **Paging:** Optional but we use it for virtual memory
- **Use case:** Modern OS kernels

### The Transition

| Step | What Happens |
|------|--------------|
| 1 | `cli` — disable interrupts (can't handle them in transition) |
| 2 | `lgdt` — load Global Descriptor Table |
| 3 | Set `CR0.PE = 1` — enable protected mode |
| 4 | Far `jmp` — load new code segment selector, flush pipeline |
| 5 | Load data segment selectors into `ds`, `es`, `ss`, `fs`, `gs` |
| 6 | Set up 32-bit stack (`esp`) |
| 7 | Call kernel |

**Why the far jump?** The CPU caches the code segment descriptor. A far jump forces it to reload `CS` from the GDT with the new 32-bit descriptor.

---

## Privilege Rings — Why Your Browser Can't Format Your Disk

x86 has **4 privilege levels (rings)**:

```
Ring 0 (DPL 0) ──────────────────► Kernel (full access)
Ring 1 (DPL 1) ──────────────────► (Unused in LunariaOS)
Ring 2 (DPL 2) ──────────────────► (Unused in LunariaOS)
Ring 3 (DPL 3) ──────────────────► User programs (restricted)
```

**DPL = Descriptor Privilege Level.** Stored in segment descriptors and checked on every privilege transition.

### What Ring 3 *Cannot* Do
- Execute `cli`, `sti`, `hlt`, `lgdt`, `lidt`, `ltr`, `mov crX`
- Access I/O ports (`in`, `out`)
- Modify page tables
- Access kernel memory

### What Ring 3 *Can* Do
- Normal arithmetic, logic, control flow
- Access its own memory (enforced by paging)
- Execute `int 0x80` (system call) — **the only way to ask kernel for help**

### Ring Transitions

| Direction | Mechanism | What Happens |
|-----------|-----------|--------------|
| Ring 0 → Ring 3 | `iret` / `iretd` | Loads `CS`, `EIP`, `SS`, `ESP`, `EFLAGS` from stack |
| Ring 3 → Ring 0 | `int 0x80` (or interrupt/exception) | CPU pushes old state, loads kernel `CS`/`SS` from IDT, jumps to handler |

**The `iret` frame** (what the CPU expects on stack for privilege change):

```
[ESP]       = SS (user data segment selector, e.g., 0x23)
[ESP + 4]   = ESP (user stack pointer)
[ESP + 8]   = EFLAGS (with IF=1 so user gets interrupts)
[ESP + 12]  = CS (user code segment selector, e.g., 0x1B)
[ESP + 16]  = EIP (entry point of user program)
```

Our `enter_user` function in `entry.asm` builds this exact frame and executes `iretd`.

---

## The Global Descriptor Table (GDT) — The CPU's Rulebook

The GDT is an array of **segment descriptors**. Each descriptor is 8 bytes describing a memory segment:

```c
// Conceptual layout of a segment descriptor (8 bytes)
struct gdt_entry {
    uint16_t limit_low;      // Limit bits 0-15
    uint16_t base_low;       // Base bits 0-15
    uint8_t  base_mid;       // Base bits 16-23
    uint8_t  access;         // Access byte (type, DPL, present)
    uint8_t  granularity;    // Limit bits 16-19 + flags (4KB granularity, 32-bit)
    uint8_t  base_high;      // Base bits 24-31
};
```

### LunariaOS's GDT (6 entries)

| Index | Selector | Purpose | Base | Limit | DPL | Type |
|-------|----------|---------|------|-------|-----|------|
| 0 | 0x00 | **Null** (required) | — | — | — | — |
| 1 | 0x08 | Kernel Code | 0 | 4 GB | 0 | Execute/Read |
| 2 | 0x10 | Kernel Data | 0 | 4 GB | 0 | Read/Write |
| 3 | 0x18 | User Code | 0 | 4 GB | 3 | Execute/Read |
| 4 | 0x20 | User Data | 0 | 4 GB | 3 | Read/Write |
| 5 | 0x28 | TSS (Task State Segment) | `&tss` | `sizeof(tss)` | 0 | Available 32-bit TSS |

**Selectors** are indices × 8 (each descriptor is 8 bytes):
- Kernel code = index 1 → selector `1 * 8 = 0x08`
- User code = index 3 → selector `3 * 8 = 0x18`
- But user code uses RPL=3 → `0x18 | 3 = 0x1B`
- User data uses RPL=3 → `0x20 | 3 = 0x23`

### What's a "Flat" Segment?

All our segments have **base = 0, limit = 4 GB**. This means:
- Logical address = linear address (no segment translation)
- Segment registers just control *privilege*, not addressing
- We rely on **paging** for memory isolation, not segmentation

This is standard for modern 32-bit OSes. 64-bit mode (long mode) essentially *requires* flat segments.

### The TSS (Task State Segment)

The TSS tells the CPU where the **Ring 0 stack** lives. When user code does `int 0x80`:
1. CPU sees DPL change (3 → 0)
2. Reads `SS0` and `ESP0` from TSS
3. Switches to kernel stack
4. Pushes user `SS`, `ESP`, `EFLAGS`, `CS`, `EIP` onto kernel stack
5. Jumps to handler

Our TSS (`src/kernel/gdt.c`):
```c
tss.ss0 = 0x10;      // Kernel data segment
tss.esp0 = 0x90000;  // Kernel stack top
```

---

## Interrupts & The IDT — How Hardware Gets the CPU's Attention

### What Is an Interrupt?

An **interrupt** is a signal that pauses the current code and runs a handler. Two sources:

| Source | Examples | Vector Range |
|--------|----------|--------------|
| **CPU Exceptions** | Divide by zero, page fault, general protection fault | 0–31 (fixed by Intel) |
| **Hardware IRQs** | Timer, keyboard, disk, network | 32–47 (remappable) |

### The PIC (Programmable Interrupt Controller)

The **8259 PIC** manages hardware IRQs. By default it maps IRQ 0–15 to CPU vectors 0x08–0x0F — **clashing with CPU exceptions!**

**We must remap it:**

```c
// In idt_init() - remap PIC so IRQs start at 0x20
outb(0x20, 0x11);  // Master PIC: init command
outb(0xA0, 0x11);  // Slave PIC: init command
outb(0x21, 0x20);  // Master offset = 0x20 (IRQ 0 → vector 0x20)
outb(0xA1, 0x28);  // Slave offset = 0x28 (IRQ 8 → vector 0x28)
outb(0x21, 0x04);  // Master has slave on IRQ 2
outb(0xA1, 0x02);  // Slave cascade identity
outb(0x21, 0x01);  // 8086 mode
outb(0xA1, 0x01);
```

Now:
- IRQ 0 (timer) → vector 0x20
- IRQ 1 (keyboard) → vector 0x21
- etc.

### The IDT (Interrupt Descriptor Table)

Like the GDT but for **interrupt handlers**. 256 entries, each 8 bytes:

```c
struct idt_entry {
    uint16_t offset_low;   // Handler address bits 0-15
    uint16_t selector;     // Code segment selector (kernel CS = 0x08)
    uint8_t  zero;         // Always 0
    uint8_t  type_attr;    // Gate type, DPL, present
    uint16_t offset_high;  // Handler address bits 16-31
};
```

### LunariaOS's IDT

| Vector | Source | DPL | Handler | Notes |
|--------|--------|-----|---------|-------|
| 0x0D | General Protection Fault | 0 | `fault_handler` | Kernel bug → halt |
| 0x0E | Page Fault | 0 | `fault_handler` | Handles demand paging |
| 0x20 | IRQ 0 (Timer) | 0 | ISR stub | Currently no-op |
| 0x21 | IRQ 1 (Keyboard) | 0 | `keyboard_handler` | Reads scancode, pushes to buffer |
| 0x80 | **System Call** | **3** | `syscall_handler` | **Only user-callable gate** |
| Others | — | 0 | `isr_dummy` | Just `iretd` |

**Critical:** The syscall gate (0x80) has **DPL = 3**. This allows Ring 3 code to `int 0x80`. All other gates have DPL = 0 (kernel only).

### Interrupt Flow (Keyboard Example)

```
1. User presses 'A'
2. Keyboard controller asserts IRQ 1
3. PIC sees IRQ 1, checks mask (we only unmask IRQ 1)
4. PIC signals CPU via INTR pin
5. CPU finishes current instruction, acknowledges interrupt
6. PIC sends vector 0x21 (IRQ 1 + 0x20 offset)
7. CPU looks up IDT[0x21], finds handler address
8. CPU pushes EFLAGS, CS, EIP (and SS, ESP if privilege change)
9. CPU jumps to handler (kernel CS:handler)
10. Handler runs (in assembly: isr.asm → isr_common_handler)
11. Handler reads scancode from port 0x60
12. Handler pushes scancode to ring buffer
13. Handler sends EOI (0x20) to PIC master: "I handled it"
14. Handler returns via iret → back to interrupted code
```

---

## System Calls — How User Programs Ask the Kernel for Help

### The Problem

User code (Ring 3) can't:
- Write to VGA memory (0xB8000)
- Read keyboard port (0x60)
- Read/write disk sectors
- Allocate physical memory

### The Solution: `int 0x80`

User code executes `int 0x80` with:
- **EAX** = syscall number
- **EBX, ECX, EDX, ESI** = up to 4 arguments
- Returns value in **EAX**

### The Kernel Side (syscall_handler)

```c
// Simplified from src/kernel/syscall.c
void syscall_handler(regs_t *r) {
    switch (r->eax) {
        case 1:  // sys_write
            term_write(r->ebx, r->ecx);  // buf, len
            break;
        case 3:  // sys_getc
            r->eax = kbd_getc();         // returns char in eax
            break;
        case 11: // sys_sbrk
            r->eax = heap_sbrk(r->ebx);  // grow heap
            break;
        // ... 17 syscalls total
    }
}
```

### The User Side (user_api.h)

```c
// Inline wrapper - expands to inline assembly
static inline void sys_write(const char *buf, unsigned int len) {
    __asm__ volatile (
        "int $0x80"
        : /* no outputs */
        : "a" (1), "b" (buf), "c" (len)  // eax=1, ebx=buf, ecx=len
        : "memory"
    );
}
```

### Full Syscall Table

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 1 | `write` | buf, len | — | Print to terminal |
| 2 | `exit` | — | — | Return to shell |
| 3 | `getc` | — | char | Block until keypress |
| 4 | `read_file` | name, buf, max | size/-1 | Read entire file |
| 5 | `write_file` | name, buf, len | 0/-1 | Create/overwrite file |
| 6 | `list` | path, buf, max | len/-1 | List directory |
| 7 | `exit_to` | mode | — | Exit with action (halt, poweroff, reboot) |
| 8 | `clear` | — | — | Clear terminal |
| 9 | `putchar_at` | c, x, y, color | — | Draw char at position |
| 10 | `goto_xy` | x, y | — | Move cursor |
| 11 | `sbrk` | delta | old break | Grow heap |
| 12 | `open` | name, flags | fd/-1 | Open file handle |
| 13 | `read` | fd, buf, len | bytes/-1 | Read from handle |
| 14 | `write` | fd, buf, len | bytes/-1 | Write to handle |
| 15 | `lseek` | fd, off, whence | pos/-1 | Move file position |
| 16 | `close` | fd | 0/-1 | Close handle |
| 17 | `unlink` | name | 0/-1 | Delete file |

---

## Virtual Memory & Paging — The Magic Trick That Makes Programs Isolated

### The Problem

Without virtual memory:
- All programs share physical RAM
- Program A can overwrite Program B's data
- Program A can overwrite kernel data
- Must load entire program into RAM at once

### The Solution: Paging

**Paging** splits memory into **4 KB pages**. The CPU translates *virtual addresses* (what the program sees) to *physical addresses* (actual RAM) using **page tables**.

```
Virtual Address (32 bits)
┌──────────────┬──────────────┬────────────────────┐
│  10 bits     │  10 bits     │    12 bits         │
│  Page Dir    │  Page Table  │    Offset          │
│  Index       │  Index       │    (within page)   │
└──────────────┴──────────────┴────────────────────┘
       │              │                  │
       ▼              ▼                  ▼
  Page Directory  Page Table        Physical Page
  (1024 entries)  (1024 entries)    Frame (4 KB)
       │              │                  │
       └──────────────┴──────────────────┘
                      │
                      ▼
              Physical Address
```

### Page Directory & Page Tables

- **Page Directory:** 1024 entries (4 KB), each points to a Page Table
- **Page Table:** 1024 entries (4 KB), each points to a Physical Frame
- **Total:** 1024 × 1024 × 4 KB = 4 GB virtual address space

Each process gets its **own Page Directory** (via `vm_create()`). The kernel switches `CR3` (page directory base register) on context switch.

### Demand Paging — Only Allocate When Needed

When a program starts:
1. Kernel creates new page directory (empty except kernel mappings)
2. Loads program binary into virtual addresses (page faults happen!)
3. On first access to each page → **Page Fault (#PF, vector 0x0E)**
4. Kernel allocates physical frame, maps it, returns
5. Program continues, now page is backed

This means:
- Program can be larger than physical RAM (swap not implemented yet)
- Unused pages cost zero physical memory
- Stack grows naturally via page faults

### LunariaOS Memory Layout

| Virtual Range | Purpose | Notes |
|---------------|---------|-------|
| 0x000000 – 0x3FFFFF | Kernel (identity mapped) | Same virtual = physical |
| 0x400000 – 0x7FFFFF | **User Program** | Demand paged, per-process |
| 0x800000 – 0xFFFFFFFF | (Unused) | Future expansion |

**User program fixed addresses:**
- Code/data loaded at `0x400000` (`USER_BASE`)
- Stack top at `0x7FF000` (`USER_STACK`)
- Heap grows up from end of binary via `sys_sbrk`
- Flat binaries from TCC load at `0x680000` (`TCC_FLAT_BASE`)

### Page Fault Handler Flow

```c
// Simplified from memory.c
void page_fault_handler(regs_t *r) {
    uint32_t fault_addr = get_cr2();  // CR2 = faulting address
    
    if (fault_addr >= USER_BASE && fault_addr < USER_TOP) {
        // Valid user address - allocate frame and map it
        uint32_t frame = alloc_frame();
        map_page(current_dir, fault_addr, frame, PRESENT | USER | RW);
        vm_faults_count++;  // Statistics
        return;             // Retry instruction
    }
    
    // Invalid access - kill process (not implemented, just halt)
    panic("Page fault at invalid address");
}
```

---

## The Filesystem — Organizing Data on Disk

### LUNFS1 — LunariaOS Filesystem v1

A **simple flat filesystem** designed for a 16 MB disk image.

### Disk Layout (LBA = Logical Block Address, 512 bytes each)

| LBA Range | Sectors | Purpose |
|-----------|---------|---------|
| 0 | 1 | Bootloader |
| 1 – ~127 | ~127 | Kernel (boot.bin + kernel.bin) |
| 128 | 1 | **Superblock** (magic, directory location, data start) |
| 200 | 64 | **Directory** (64 sectors × 16 entries = 1024 files max) |
| 300+ | Rest | **File Data** (contiguous allocation) |

### Superblock (LBA 128)

```c
struct superblock {
    char magic[6];      // "LUNFS1"
    uint16_t dir_lba;   // 200
    uint16_t dir_sectors; // 64
    uint32_t data_lba;  // 300
};
```

### Directory Entry (32 bytes each, 16 per sector)

```c
struct file_entry {
    char name[23];      // Null-terminated, max 22 chars
    uint8_t type;       // FS_FILE (0) or FS_DIR (1)
    uint32_t start_lba; // First data sector
    uint32_t size;      // Size in bytes
};
```

### Path Handling

Directories store **newline-separated full paths** of children:
```
# Directory entry for "bin"
bin/hello
bin/edit
bin/tui
```

This is simple but limits scalability (no subdirectory entries, just path lists).

### Filesystem Operations

| Function | Description |
|----------|-------------|
| `fs_mount()` | Verify magic at LBA 128 |
| `fs_find(name)` | Search directory for entry |
| `fs_read(name, buf, max)` | Read file into buffer |
| `fs_write(name, type, buf, len)` | Create/overwrite, allocates next free LBA |
| `fs_append(dir, name)` | Add entry to directory's child list |
| `fs_mkdir(name)` | Create directory entry |
| `fs_delete(name)` | Mark entry unused (no space reclamation!) |

### Seeding the Disk Image

`tools/fs_seeder.c` (host program) runs at build time:
1. Creates empty 16 MB disk image
2. Writes bootloader + kernel to sectors 0+
3. Writes superblock at LBA 128
4. Creates directory at LBA 200
5. Copies user programs (`src/home/*.c` → compiled → embedded as files)
6. Copies `libc.a` (userspace C library archive)

---

## VGA Text Mode — Drawing Text Without a GPU

### The Framebuffer

At physical address **0xB8000** lives a **4000-byte** memory region (80 cols × 25 rows × 2 bytes).

**Each cell = 2 bytes:**
```
Byte 0: ASCII character (0-255)
Byte 1: Attribute byte
        ┌─────────────┬─────────────┐
        │ Background  │ Foreground  │
        │ (4 bits)    │ (4 bits)    │
        └─────────────┴─────────────┘
        Colors 0-15: Black, Blue, Green, Cyan, Red, Magenta, Brown, Light Grey,
                     Dark Grey, Light Blue, Light Green, Light Cyan, Light Red,
                     Light Magenta, Yellow, White
```

### Writing a Character

```c
// From vga.c
void vga_putchar_at(char c, int x, int y, uint8_t color) {
    uint16_t *vga = (uint16_t *)0xB8000;
    vga[y * 80 + x] = (uint16_t)c | ((uint16_t)color << 8);
}
```

### The Workspace Manager (Virtual Terminals)

LunariaOS has **10 virtual terminals** (workspaces), each with:
- Own 80×25 cell buffer (4000 bytes)
- Own cursor position
- Own input buffer
- Switch with **Ctrl+1…Ctrl+0** or **Ctrl+Q W E R T Y U O P A**

```c
// wm.c - each workspace has this
struct workspace {
    uint16_t cells[80 * 25];  // Back buffer
    int cursor_x, cursor_y;
    char input_buf[256];
    int input_len;
};
```

**Blitting:** `wm_blit(ws)` copies workspace's back buffer to real VGA memory (0xB8000). `wm_blit_row()` copies just one row for efficiency.

---

## The Shell — Your Command Center

The shell (`src/kernel/shell.c`) runs in **kernel space** (Ring 0) and provides:

### Built-in Commands

| Command | Description |
|---------|-------------|
| `help` | List commands |
| `clear` | Clear screen |
| `echo [args...]` | Print arguments (supports `\n`, `\t`, `\\`) |
| `read <lba>` | Dump raw disk sector |
| `ls [dir]` | List filesystem |
| `cat <file>` | Print file contents |
| `mkdir <name>` | Create directory |
| `rm <file>` | Delete file |
| `run <file> [args...]` | **Load and execute user program** |
| `free` | Show memory stats |
| `uptime` | Show time since boot |
| `banner` | Print OS banner |
| `sleep` | Pause |
| `exit` | Power off |

### Running a User Program (`run`)

```c
// Simplified from shell.c
void cmd_run(char *name, int argc, char **argv) {
    // 1. Create new address space for this program
    uint32_t dir = vm_create();
    vm_attach(dir);  // Switch to it (load CR3)
    
    // 2. Load program binary from filesystem into USER_BASE (0x400000)
    int size = fs_read(name, (char *)USER_BASE, USER_PROG_MAX);
    vm_note_load(size);  // Track heap start
    
    // 3. Build argument stack frame for _start(argc, argv)
    build_argv_stack(argc, argv);
    
    // 4. Enter user mode (Ring 3) - NEVER RETURNS HERE
    enter_user(entry_point, user_stack_pointer);
}
```

### Output Redirection

```bash
echo hello > file.txt    # Write "hello\n" to file.txt
echo foo > bar.txt       # Overwrites bar.txt
```

The shell parses `>`, opens file for writing, captures command output, writes to file.

---

## Building & Running — From Source to Booting OS

### Prerequisites (Linux/macOS/WSL)

| Tool | Purpose | Ubuntu Install |
|------|---------|----------------|
| `nasm` | Assemble `.asm` → `.o` | `sudo apt install nasm` |
| `gcc`/`clang` | Compile C → `.o` | `sudo apt install gcc` |
| `binutils` | `ld`, `objcopy` | `sudo apt install binutils` |
| `qemu-system-x86` | Emulate x86 | `sudo apt install qemu-system-x86` |

### Build Commands

```bash
make          # Build everything → bin/OS.bin, bin/disk.img
make run      # Run in QEMU with GUI window
make headless # Run in QEMU curses terminal (Ctrl+keys may not work)
make clean    # Remove bin/
make test     # Run automated tests (headless + expect scripts)
```

### What `make` Does (Simplified)

```makefile
# 1. Bootloader
nasm -f bin src/boot/boot.asm -o bin/boot.bin

# 2. Kernel objects
nasm -f elf32 src/kernel/entry.asm -o bin/entry.o
nasm -f elf32 src/kernel/isr.asm -o bin/isr.o
gcc -m32 -ffreestanding -nostdlib -c src/kernel/*.c -o bin/*.o
gcc -m32 -ffreestanding -nostdlib -c src/drivers/*.c -o bin/*.o

# 3. Link kernel
ld -m elf_i386 -T src/kernel/linker.ld -o bin/kernel.elf bin/*.o
objcopy -O binary bin/kernel.elf bin/kernel.bin

# 4. User programs (each .c in src/home/)
gcc -m32 -ffreestanding -nostdlib -T src/home/prog.ld -o bin/prog.elf src/home/prog.c
objcopy -O binary bin/prog.elf bin/prog.bin

# 5. Static libraries
ar rcs bin/libmirror.a bin/mirror.o
ar rcs bin/libc.a bin/libc.o bin/stdio.o bin/setjmp.o
ar rcs bin/libtcc.a thirdparty/tinycc/*.o  # Only linked into tcc program

# 6. Create disk image
dd if=/dev/zero of=bin/disk.img bs=1M count=16
dd if=bin/boot.bin of=bin/disk.img conv=notrunc        # Sector 0
dd if=bin/kernel.bin of=bin/disk.img seek=1 conv=notrunc # Sector 1+

# 7. Seed filesystem (host tool)
gcc tools/fs_seeder.c -o bin/fs_seeder
./bin/fs_seeder bin/disk.img  # Writes superblock, dir, programs, libc.a

# 8. Final OS binary (for ISO/floppy boot)
cat bin/boot.bin bin/kernel.bin > bin/OS.bin
```

### Running in QEMU

```bash
# GUI mode (SDL window)
qemu-system-i386 -drive file=bin/disk.img,format=raw -serial stdio

# Headless (curses in terminal)
qemu-system-i386 -drive file=bin/disk.img,format=raw -nographic -serial mon:stdio
```

---

## Quick Reference: Key Addresses & Constants

| Symbol | Value | Meaning |
|--------|-------|---------|
| `0x7C00` | Bootloader load address |
| `0x7E00` | Kernel load address |
| `0x90000` | Kernel stack top |
| `0xB8000` | VGA text framebuffer |
| `0x100000` | User program link address (build time) |
| `0x400000` | `USER_BASE` — user virtual base |
| `0x680000` | `TCC_FLAT_BASE` — TCC flat binary load address |
| `0x7FF000` | `USER_STACK` — initial user stack top |
| `0x800000` | `USER_TOP` — user region end |
| `0x20` | PIC master command port |
| `0x21` | PIC master data port |
| `0x60` | Keyboard data port |
| `0x1F0-0x1F7` | ATA primary channel ports |

---

## Learning Path Recommendations

1. **Read this guide** ← You are here
2. **Read `docs/index.md`** — Technical reference (what you just read, condensed)
3. **Read `docs/userspace.md`** — How to write user programs
4. **Trace `kmain()` in `src/kernel/kernel.c`** — See initialization order
5. **Look at `src/home/hello.c`** — Simplest user program
6. **Modify `hello.c`, rebuild, test** — Add a print, change a color
7. **Read `src/kernel/syscall.c`** — See how syscalls dispatch
8. **Read `src/drivers/keyboard.c`** — See interrupt-driven input
9. **Read `src/kernel/memory.c`** — See paging/frame allocator
10. **Pick a `todo.txt` item** — Contribute!

---

## Glossary

| Term | Definition |
|------|------------|
| **BIOS** | Basic Input/Output System — firmware that initializes hardware and boots the OS |
| **Bootloader** | Tiny program (512 bytes) that loads the kernel |
| **Real Mode** | 16-bit CPU mode, 1 MB memory, no protection |
| **Protected Mode** | 32-bit CPU mode, 4 GB memory, privilege rings, paging |
| **GDT** | Global Descriptor Table — defines memory segments and privileges |
| **IDT** | Interrupt Descriptor Table — maps interrupt vectors to handlers |
| **PIC** | Programmable Interrupt Controller — routes hardware IRQs to CPU |
| **IRQ** | Interrupt Request — hardware signal (keyboard, timer, disk) |
| **Syscall** | System call — user program requests kernel service via `int 0x80` |
| **Ring 0/3** | Privilege levels (0 = kernel, 3 = user) |
| **Paging** | Virtual memory using 4 KB pages and page tables |
| **Page Fault** | CPU exception when accessing unmapped page — kernel handles it |
| **Demand Paging** | Allocate physical frames only when page is first accessed |
| **Frame** | 4 KB physical memory block |
| **VGA Text Mode** | 80×25 character display at 0xB8000 |
| **LBA** | Logical Block Address — sector number on disk (512 bytes) |
| **TSS** | Task State Segment — holds kernel stack pointer for privilege transitions |
| **CR0/CR2/CR3** | Control registers (CR0: paging enable, CR2: fault address, CR3: page directory) |
| **EOI** | End of Interrupt — tell PIC we're done handling an IRQ |
| **Flat Binary** | Raw machine code, no headers, loaded at fixed address |
| **Freestanding C** | C without standard library (`-ffreestanding -nostdlib`) |

---

*Welcome to OS development! Every expert started exactly where you are now — confused but curious. Keep going.*