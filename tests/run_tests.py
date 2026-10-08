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
from harness import Qemu, screen, screen_text  # noqa: E402

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


def test_tcc_flat_libc(disk):
    def body(q):
        q.command("run tcc print.c -o print", wait=20.0)
        text = screen_text(q.dump("v_tccpl.bin"))
        check("tcc links libc.a into flat image", "tcc: wrote print" in text)

        q.command("run print hello", wait=3.0)
        text = screen_text(q.dump("v_tccpl2.bin"))
        check("flat printf formats argc", "print.c: argc=2" in text)
        check("flat printf formats strings", "print.c: argv[1]=hello" in text)
        check("flat printf formats 64-bit", "print.c: u64=1234567890123" in text)
    run(disk, "tcc flat libc", body)


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


def test_paint(disk):
    def body(q):
        q.command("run paint", wait=1.5)
        rows = screen(q.dump("v_paint1.bin"))
        check("paint status bar", "brush" in rows[24] and "esc quit" in rows[24])
        check("paint canvas starts blank", rows[0] == "")

        q.type_text("h")                  # brush 'h', stamped at (0,0)
        for _ in range(4):
            q.sendkey("right")            # drag: stamps 'h' at (1,0)..(4,0)
        time.sleep(0.3)
        rows = screen(q.dump("v_paint2.bin"))
        check("paint pen draws a line", rows[0][:5] == "hhhhh")

        q.sendkey("bracket_right")        # grey(7) -> dkgrey -> lblue
        q.sendkey("bracket_right")
        for _ in range(3):
            q.sendkey("down")             # vertical leg in the new colour
        time.sleep(0.3)
        rows = screen(q.dump("v_paint3.bin"))
        check("paint cycles colours", "lblue" in rows[24])
        check("paint draws a column", rows[1][4] == "h" and rows[3][4] == "h")

        q.sendkey("backspace")            # eraser on, scrubs the cell at (4,3)
        time.sleep(0.2)
        rows = screen(q.dump("v_paint4.bin"))
        check("paint eraser engages", "ERASE" in rows[24])
        check("paint eraser scrubs cell", rows[3] == "")
        q.sendkey("right")                # keeps erasing while dragging (5,3)
        q.sendkey("backspace")            # eraser off again
        time.sleep(0.2)
        rows = screen(q.dump("v_paint5.bin"))
        check("paint eraser disengages", "ERASE" not in rows[24])

        q.sendkey("ret")                  # clear the canvas
        time.sleep(0.2)
        rows = screen(q.dump("v_paint6.bin"))
        check("paint enter clears", rows[0] == "" and rows[1] == "")

        q.sendkey("esc")                  # leave
        time.sleep(0.8)
        q.command("echo ok", wait=0.6)
        text = screen_text(q.dump("v_paint7.bin"))
        check("paint returns to the shell", "ok" in text)
    run(disk, "paint", body)


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
    test_tcc_flat_libc(disk)
    test_mirror(disk)
    test_edit_roundtrip(disk)
    test_paint(disk)

    passed = sum(1 for r in results if r)
    print(f"\n{passed}/{len(results)} checks passed")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
