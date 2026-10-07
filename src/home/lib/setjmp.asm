[bits 32]

; setjmp()/longjmp() for the userspace library.
;
; A jmp_buf (see setjmp.h) holds the callee-saved registers plus the stack
; pointer and return address captured by setjmp. longjmp reloads them and
; resumes as if setjmp had just returned the requested value.

section .text

global setjmp
setjmp:
    mov eax, [esp + 4]          ; jmp_buf
    mov [eax + 0], ebx
    mov [eax + 4], esi
    mov [eax + 8], edi
    mov [eax + 12], ebp
    lea ecx, [esp + 4]          ; esp after the return address is popped
    mov [eax + 16], ecx
    mov ecx, [esp]              ; return address
    mov [eax + 20], ecx
    xor eax, eax
    ret

global longjmp
longjmp:
    mov eax, [esp + 4]          ; jmp_buf
    mov edx, [esp + 8]          ; value
    test edx, edx
    jnz .nonzero
    inc edx                     ; longjmp(env, 0) still returns 1
.nonzero:
    mov ebx, [eax + 0]
    mov esi, [eax + 4]
    mov edi, [eax + 8]
    mov ebp, [eax + 12]
    mov ecx, [eax + 20]         ; eip
    mov esp, [eax + 16]         ; esp
    mov eax, edx
    jmp ecx
