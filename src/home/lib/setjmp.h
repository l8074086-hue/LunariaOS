#ifndef SETJMP_H
#define SETJMP_H

/* Saved context: ebx, esi, edi, ebp, esp, eip. The implementation lives in
   setjmp.asm so the callee-saved registers are captured exactly. */

typedef unsigned int jmp_buf[6];

int setjmp(jmp_buf env) __attribute__((returns_twice));
void longjmp(jmp_buf env, int val) __attribute__((noreturn));

#endif
