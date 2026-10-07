#ifndef SYS_MMAN_H
#define SYS_MMAN_H

#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4

#define MAP_SHARED  0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED   0x10
#define MAP_ANON    0x20
#define MAP_ANONYMOUS MAP_ANON

#define MAP_FAILED ((void *)-1)

void *mmap(void *addr, unsigned int len, int prot, int flags, int fd, int off);
int munmap(void *addr, unsigned int len);
int mprotect(void *addr, unsigned int len, int prot);

#endif
