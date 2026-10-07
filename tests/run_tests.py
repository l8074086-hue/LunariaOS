#!/usr/bin/env python3
"""LunariaOS regression tests.

    python3 tests/run_tests.py [bin/disk.img]

Boots the image once per scenario, drives the shell, and checks what lands on
screen. Exits non-zero if anything fails.
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Qemu, screen_text  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = "/tmp/opencode/lunatest"

results = []


def check(name, ok):
    results.append(ok)
    print(("  ok   " if ok else "  FAIL ") + name)


def run(disk, name, body):
    print(f"[{name}]")
    os.makedirs(WORK, exist_ok=True)
    q = Qemu(disk, WORK)
    try:
        body(q)
    finally:
        q.close()


def test_libctest(disk):
    def body(q):
        q.command("run libctest", wait=1.5)
        text = screen_text(q.dump("v_libc.bin"))
        check("libctest all pass", "LIBCTEST ALL PASS" in text)
        check("libctest no failures", "FAIL" not in text)
    run(disk, "libctest", body)


def test_filetest(disk):
    def body(q):
        q.command("run filetest", wait=1.5)
        text = screen_text(q.dump("v_file.bin"))
        check("filetest all pass", "FILETEST ALL PASS" in text)
        check("filetest no failures", "FAIL" not in text)
    run(disk, "filetest", body)


def test_stdiotest(disk):
    def body(q):
        q.command("run stdiotest", wait=1.5)
        text = screen_text(q.dump("v_stdio.bin"))
        check("stdiotest all pass", "STDIOTEST ALL PASS" in text)
        check("stdiotest all checks ran", "23/23 checks" in text)
    run(disk, "stdiotest", body)


def test_tcc(disk):
    def body(q):
        q.command("run tcc", wait=12.0)
        text = screen_text(q.dump("v_tcc.bin"))
        check("tcc compiles and runs code",
              "tcc: add(2,3)=5 fib(10)=55" in text)
    run(disk, "tcc", body)


def test_tcc_file(disk):
    def body(q):
        q.command("run tcc demo.c fib", wait=12.0)
        text = screen_text(q.dump("v_tccf.bin"))
        check("tcc compiles a disk file", "tcc: main() -> 55" in text)
        q.command("run tcc demo.c", wait=8.0)
        text = screen_text(q.dump("v_tccf2.bin"))
        check("compiled main sees its argv", "tcc: main() -> 1" in text)
    run(disk, "tcc file", body)


def test_tcc_flat(disk):
    def body(q):
        q.command("run tcc demo.c -o out.bin", wait=12.0)
        text = screen_text(q.dump("v_tccb.bin"))
        check("tcc writes a flat binary", "tcc: wrote out.bin" in text)

        q.command("run out.bin fib", wait=3.0)
        text = screen_text(q.dump("v_tccb2.bin"))
        check("flat binary runs in the shell", "demo: fib(10)=55" in text)

        q.command("run out.bin", wait=3.0)
        text = screen_text(q.dump("v_tccb3.bin"))
        check("flat binary sees its argv", "demo: argc=1" in text)
    run(disk, "tcc flat", body)


def test_mirror(disk):
    def body(q):
        q.command("run mirrordemo", wait=1.2)
        text = screen_text(q.dump("v_mirror.bin"))
        check("mirror launches", "Mirror" in text)
        q.sendkey("esc")
    run(disk, "mirror", body)


def test_edit_roundtrip(disk):
    def body(q):
        q.command("run edit", wait=1.2)
        splash = screen_text(q.dump("v_edit1.bin"))
        check("edit title", "lunaria edit" in splash)

        q.type_text("t1")          # splash: name the file to open
        q.sendkey("ret")
        time.sleep(0.6)
        q.type_text("hi")          # type two characters
        time.sleep(0.3)
        q.sendkey("esc")           # command mode
        q.type_text("wq")          # write and quit
        q.sendkey("ret")
        time.sleep(0.8)

        q.command("cat t1", wait=0.6)   # shell should now see the saved bytes
        after = screen_text(q.dump("v_edit2.bin"))
        check("edit saved to disk", "hi" in after)
    run(disk, "edit roundtrip", body)


def main():
    disk = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "bin/disk.img"))
    if not os.path.exists(disk):
        print(f"no disk image at {disk}; run make first")
        return 2

    test_libctest(disk)
    test_filetest(disk)
    test_stdiotest(disk)
    test_tcc(disk)
    test_tcc_file(disk)
    test_tcc_flat(disk)
    test_mirror(disk)
    test_edit_roundtrip(disk)

    passed = sum(1 for r in results if r)
    print(f"\n{passed}/{len(results)} checks passed")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
