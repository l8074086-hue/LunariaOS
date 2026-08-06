[bits 32]
[extern kmain]

global _start
_start:
    mov esp, 0x90000
    call kmain
    jmp $

; enter_user(unsigned int eip, unsigned int esp)
; Builds a ring-3 iret frame and drops privilege. Never returns here
; normally; sys_exit lands back in the kernel via exit_to_shell.
global enter_user
enter_user:
    mov eax, [esp + 4]
    mov ecx, [esp + 8]
    push dword 0x23          ; user data segment (ss)
    push ecx                 ; user stack pointer
    pushfd
    or dword [esp], 0x200    ; set IF so user code gets interrupts
    push dword 0x1B          ; user code segment (cs)
    push eax                 ; entry point (eip)
    iretd
