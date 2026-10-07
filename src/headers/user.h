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

/* tcc flat binaries (`run tcc file.c -o out.bin`). The compiler bakes the
   image for TCC_FLAT_BASE (the running tcc program cannot use USER_BASE,
   which it occupies itself); the loader detects the 12-byte header
   "LUNB" + u32 size + u32 entry, copies the image to TCC_FLAT_BASE and
   enters it. Keep in step with src/home/lib/user_api.h. */
#define TCC_FLAT_MAGIC0 'L'
#define TCC_FLAT_MAGIC1 'U'
#define TCC_FLAT_MAGIC2 'N'
#define TCC_FLAT_MAGIC3 'B'
#define TCC_FLAT_BASE   0x680000
#define TCC_FLAT_MAX    0x10000  /* disk writable-file buffer cap, see fs.c */

extern void enter_user(unsigned int eip, unsigned int esp);
extern void exit_to_shell(void);

#endif
