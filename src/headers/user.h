#ifndef USER_H
#define USER_H

/* Layout of the ring-3 window. This is the single source of truth: the
   kernel's page table code includes this header rather than repeating it.
   USER_BASE must stay 2 MB aligned, because the U/S bit lives in the page
   directory and the window needs page directory entries of its own. */
#define USER_BASE    0x400000
#define USER_TOP     0x800000
#define USER_STACK   0x7FF000
#define USER_PROG_MAX 0x200000

extern void enter_user(unsigned int eip, unsigned int esp);
extern void exit_to_shell(void);

#endif
