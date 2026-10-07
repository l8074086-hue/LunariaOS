#ifndef UNISTD_H
#define UNISTD_H

#include <stddef.h>
#include <fcntl.h>

typedef long ssize_t;

int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
long lseek(int fd, long offset, int whence);
int unlink(const char *path);

char *getcwd(char *buf, size_t size);
int access(const char *path, int mode);

int isatty(int fd);
int dup(int fd);
int dup2(int oldfd, int newfd);

#define _SC_PAGESIZE 30
long sysconf(int name);

extern char **environ;

#endif
