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

; Faults that push an error code (0x0D GP, 0x0E PF). We forward the
; saved registers, the faulting address (CR2) and the error code to C,
; then return cleanly so a recovered fault can resume execution.
%macro FAULT_ERR 1
global isrfault%1
isrfault%1:
    pushad
    mov eax, [esp + 32]        ; error code pushed by CPU
    mov edx, cr2               ; faulting address (0 for non-PF)
    push %1                    ; vector
    push eax                   ; error code
    push edx                   ; cr2
    push esp                   ; saved registers
    call fault_handler
    add esp, 16
    popad
    add esp, 4                 ; drop the CPU-pushed error code
    iretd
%endmacro

; RAW capture for #PF diagnostics: dword cells at 0x6000+i written at entry.
; Captured before pushad, so esp still points at the CPU's frame:
;  [0]=vector [1]=cr2 [2]=error code [3]=eip [4]=cs
global isrfault0x0E
isrfault0x0E:
    mov eax, cr2
    mov dword [0x6000], 0x0E
    mov dword [0x6004], eax
    mov eax, [esp]
    mov dword [0x6008], eax
    mov eax, [esp + 4]
    mov dword [0x600C], eax
    mov eax, [esp + 8]
    mov dword [0x6010], eax
    pushad
    mov eax, [esp + 32]
    mov edx, cr2
    push 0x0E
    push eax
    push edx
    push esp
    call fault_handler
    add esp, 16
    test eax, eax
    jz .fatal              ; not recoverable, handler has already stopped us
    popad
    add esp, 4
    iretd                  ; retry the instruction that faulted
.fatal:
    cli
.hang:
    hlt
    jmp .hang

FAULT_ERR 0x0D      ; general protection fault
