[org 0x7c00]
[bits 16]

KERNEL_ADDR equ 0x7E00

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    mov [boot_drive], dl

    mov ax, 0
    mov es, ax
    mov bx, KERNEL_ADDR

    mov al, KERNEL_SECTORS
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    mov ah, 0x02
    int 0x13

    jc disk_error
    ; --- E820 ---
    E820_BUF equ 0x5000
    E820_COUNT equ 0x4FFC
    E820_MAX equ 32

    detect_mem:
      xor ax, ax
      mov es, ax
      mov di, E820_BUF
      xor ebx, ebx
      xor bp, bp
      cld
    .next:
      mov eax, 0xE820
      mov edx, 0x534D4150
      mov ecx, 24
      int 0x15
      jc .done
      cmp eax, 0x534D4150
      jne .done
      cmp ecx, 20
      jb .done
      add di, 24
      inc bp
      cmp bp, E820_MAX
      jge .done
      or ebx, ebx
      jnz .next
    .done:
      mov [E820_COUNT], bp

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:init_pm

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000

    call KERNEL_ADDR
    jmp $

disk_error:
    mov si, err_msg
.loop:
    lodsb
    or al, al
    jz .halt
    mov ah, 0x0e
    int 0x10
    jmp .loop
.halt:
    cli
    hlt
    jmp .halt

err_msg: db "DISK READ ERROR", 0

gdt_start:
    dq 0x0
gdt_code:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0
gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

boot_drive: db 0

times 510 - ($ - $$) db 0
dw 0xaa55
