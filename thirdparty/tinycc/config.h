/* TinyCC configuration for LunariaOS.

   LunariaOS is a freestanding 32-bit x86 system, so the target is pinned to
   i386 and every host-specific luxury (threads, stack backtraces, the bounds
   checker, dynamic loading) is turned off.  tccrun.c supplies its own
   dlopen/dlsym/dlclose stubs when CONFIG_TCC_STATIC is set, which is what
   lets the whole compiler build without <dlfcn.h>. */

#ifndef TCC_CONFIG_H
#define TCC_CONFIG_H

#define TCC_VERSION "0.9.28rc"

/* Only recorded for parity with a configure-generated config.h; nothing in
   the core reads these back. */
#define CC_NAME CC_gcc
#define GCC_MAJOR 12
#define GCC_MINOR 0

#define TCC_TARGET_I386 1

/* Single-threaded, no bounds checker, no builtin backtraces. */
#define CONFIG_TCC_SEMLOCK 0
#define CONFIG_TCC_BCHECK 0
#define CONFIG_TCC_BACKTRACE 0

/* No libdl on Lunaria: tccrun.c provides the dl* stubs itself. */
#define CONFIG_TCC_STATIC 1

/* include/tccdefs.h is compiled in as strings (tccdefs_.h). */
#define CONFIG_TCC_PREDEFS 1

/* Where the compiler looks for its own headers and libraries at runtime. */
#define CONFIG_TCCDIR "/tcc"

#endif
