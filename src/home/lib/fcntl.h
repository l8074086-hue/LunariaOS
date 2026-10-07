#ifndef FCNTL_H
#define FCNTL_H

/* open() shares the kernel's flag bits with the rest of the userspace API, so
   the O_* values live in user_api.h and are re-exported here for code that
   expects the POSIX header. */

#include "user_api.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_TEXT
#define O_TEXT 0
#endif

/* Extra mode argument (if any) is ignored; the filesystem has no permissions. */
int open(const char *path, int flags, ...);

#endif
