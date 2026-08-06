[bits 32]
[extern isr_common_handler]
[extern fault_handler]
[extern syscall_handler]

%macro ISR 1
global isr%1
isr%1:
    pushad
    push %1
    call isr_common_handler
    add esp, 4
    popad
    iretd
%endmacro

ISR 0x21      ; keyboard
ISR 0x20      ; timer (when you add it)

global isr_dummy
isr_dummy:
    iretd

; int 0x80 syscall from ring 3. CPU pushed the iret frame
; (eip, cs, eflags, user_esp, user_ss) on the ring-0 stack.
; Hand the saved registers + frame pointer to the C dispatcher.
global isr0x80
isr0x80:
    pushad
    mov eax, esp
    lea edx, [eax + 32]
    push edx          ; frame pointer
    push eax          ; saved registers pointer
    call syscall_handler
    add esp, 8
    popad
    iretd

%macro FAULT 1
global isrfault%1
isrfault%1:
    push %1
    call fault_handler
    cli
    hlt
    jmp $-2
%endmacro

FAULT 0x0D      ; general protection fault
FAULT 0x0E      ; page fault
