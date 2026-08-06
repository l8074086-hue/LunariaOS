#ifndef USER_H
#define USER_H

#define USER_BASE    0x100000
#define USER_STACK   0x200000
#define USER_PROG_MAX 0x100000

extern void enter_user(unsigned int eip, unsigned int esp);
extern void exit_to_shell(void);

#endif
