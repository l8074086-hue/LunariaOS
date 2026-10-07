"""Small QEMU driver for the LunariaOS tests.

Boots a disk image headless, drives the keyboard through the QEMU monitor,
and reads the VGA text buffer back with `pmemsave`. No pytest, no deps.
"""

import os
import subprocess
import time

VGA_BASE = 0xB8000
VGA_BYTES = 4000


def key_name(ch):
    table = {
        " ": "spc",
        "\n": "ret",
        "\t": "tab",
        "\b": "backspace",
        ".": "dot",
        ",": "comma",
        "-": "minus",
        "/": "slash",
        "=": "equal",
        ";": "semicolon",
        "'": "apostrophe",
        "[": "bracket_left",
        "]": "bracket_right",
        "\\": "backslash",
        "`": "grave_accent",
    }
    if ch in table:
        return table[ch]
    if ch.islower() or ch.isdigit():
        return ch
    raise ValueError(f"no sendkey mapping for {ch!r}")


class Qemu:
    def __init__(self, disk, workdir, boot=4.0):
        self.workdir = workdir
        self.proc = subprocess.Popen(
            [
                "qemu-system-x86_64",
                "-no-reboot",
                "-display", "none",
                "-monitor", "stdio",
                "-drive", f"format=raw,file={disk}",
            ],
            stdin=subprocess.PIPE,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
            cwd=workdir,
        )
        time.sleep(boot)

    def monitor(self, line, wait=0.25):
        self.proc.stdin.write((line + "\n").encode())
        self.proc.stdin.flush()
        time.sleep(wait)

    def sendkey(self, key, wait=0.08):
        self.monitor("sendkey " + key, wait)

    def type_text(self, text, wait=0.07):
        for ch in text:
            self.sendkey(key_name(ch), wait)

    def command(self, text, wait=0.9):
        """Type a shell command and press Enter."""
        self.type_text(text)
        self.sendkey("ret")
        time.sleep(wait)

    def dump(self, name):
        path = os.path.join(self.workdir, name)
        if os.path.exists(path):
            os.remove(path)
        self.monitor(f"pmemsave {VGA_BASE} {VGA_BYTES} {name}", 0.4)
        return path

    def close(self):
        try:
            self.monitor("quit", 0.2)
        except (BrokenPipeError, ValueError):
            pass
        time.sleep(0.2)
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()


def screen(path):
    """Return the 25 VGA rows as stripped strings."""
    data = open(path, "rb").read()
    rows = []
    for y in range(25):
        chars = "".join(chr(data[(y * 80 + x) * 2]) for x in range(80))
        rows.append(chars.rstrip())
    return rows


def screen_text(path):
    return "\n".join(screen(path))
