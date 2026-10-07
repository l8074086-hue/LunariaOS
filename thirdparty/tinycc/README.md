# TinyCC (vendored)

This directory holds the core of [TinyCC](https://github.com/TinyCC/tinycc)
version **0.9.28rc**, pruned down to what the LunariaOS on-device compiler
needs. It is the upstream source, not a fork — the only file written by this
project is `config.h`.

Included:

- the compiler core and preprocessor: `libtcc.c`, `tccpp.c`, `tccgen.c`,
  `tccdbg.c`, `tccasm.c`, `tccelf.c`, `tccrun.c`
- the i386 target: `i386-gen.c`, `i386-link.c`, `i386-asm.c`, `i386-asm.h`,
  `i386-tok.h`
- shared data: `tcctok.h`, `elf.h`, `stab.h`, `stab.def`, `dwarf.h`,
  `include/tccdefs.h`
- the public API header `libtcc.h`

Everything else (the CLI driver, other architectures, tests) is omitted. The
whole core is compiled as one translation unit (`ONE_SOURCE=1`), so a single
`libtcc.c` build produces `bin/libtcc.a`.

`conftest.c` is also kept: built with `-DC2STR` on the host it becomes the
`c2str` tool that converts `include/tccdefs.h` into the `tccdefs_.h` string
table compiled into the core (`CONFIG_TCC_PREDEFS`).

Beyond `config.h`, two tiny extensions are added to upstream code, both in
service of the on-device flat-binary linker:

- `tcc_relocate_at(TCCState *, void *)` (`tccrun.c`, declared in
  `libtcc.h`): like `tcc_relocate()`, but lays the linked image out (and
  resolves every absolute reference) against a fixed virtual address chosen
  by the caller. Returning the image size, it lets `tcc file.c -o out.bin`
  emit a self-contained image for a loader that maps it at that address.

See `config.h` for the target choices and `docs/userspace.md` for how it is
wired into the build. TinyCC is distributed under the GNU LGPL (see
`COPYING`).
