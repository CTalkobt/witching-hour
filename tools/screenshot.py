#!/usr/bin/env python3
"""Screenshot of a PRG in headless xemu (from M65Compiler's examples/capture_screens.py).

Usage:
    python3 tools/screenshot.py program.prg /abs/path/out.png [disk.d81]

A disk image goes in drive 8 (a program with code overlays loads them from
there).

The PRG runs at real speed; the screenshot is taken CAPTURE_DELAY seconds
(default 2.5) after the program is in memory. PEEK="1c000 d400" prints
memory (the monitor's m command) just before. The script watches xemu's UART
monitor until the PRG's bytes are at its load address, waits, and stops xemu,
which saves its -screenshot on exit.
"""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

DELAY = float(os.environ.get("CAPTURE_DELAY", "2.5"))
OUT = "doc/examples/images"

# name: the PRG (the examples' build outputs)
EXAMPLES = {
    "hello_world": "examples/c/hello_world/hello.prg",
    "hello_linked": "examples/c/hello_linked/hello.prg",
    "multi_module": "examples/c/multi_module/program.prg",
    "unit_convert": "examples/c/unit_convert/convert.prg",
    "palette_fade": "examples/c/palette_fade/palette_fade.prg",
    "bitfield_registers": "examples/c/bitfield_registers/bitfield_demo.prg",
    "game_of_life": "examples/c/game_of_life/life.prg",
    "memset_screen": "examples/c/memset_screen/memset_screen.prg",
    "text_scroller": "examples/c/text_scroller/scroller.prg",
    "vic4_colors": "examples/c/vic4_colors/vic4_colors.prg",
    "far_scroller": "examples/c/far_scroller/far_scroller.prg",
    "invaders": "examples/c/invaders/invaders.prg",
    "mandelbrot": "examples/c/mandelbrot/mandelbrot.prg",
    "musicgen": "examples/c/musicgen/musicgen.prg",
    "calc_graph": "examples/c/calc_graph/calc_graph.prg",
    "forth": "examples/c/forth/forth.prg",
    "rogue": "examples/c/rogue/rogue.prg",
    "rtc_clock": "examples/c/rtc_clock/rtc_clock.prg",
    "keyboard_viewer": "examples/c/keyboard_viewer/keyboard_viewer.prg",
    "sample_player": "examples/c/sample_player/sample_player.prg",
    "ctype_demo": "examples/c/stdlib/ctype_h/ctype_demo.prg",
    # the library checks print nothing: one picture stands for them all
    "test_strlen": "examples/c/stdlib/string_h/test_strlen.prg",
    "c_routines": "examples/basic/c_routines/menu.prg",
}


def monitor(sock, cmd):
    """Send a monitor command, return its output (up to the next prompt)."""
    sock.sendall((cmd + "\r").encode())
    data = b""
    end = time.time() + 2
    while time.time() < end:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        data += chunk
        if b"." in chunk.split(b"\n")[-1] or data.rstrip().endswith(b"."):
            break
    return data.decode(errors="replace")


def loaded(sock, addr, want):
    """Are the PRG's first bytes at addr?"""
    out = monitor(sock, "m%x" % addr)
    for line in out.splitlines():
        line = line.strip()
        if line.startswith(":"):
            parts = line[1:].split(":", 1)
            if len(parts) == 2 and int(parts[0], 16) == addr:
                got = bytes.fromhex(parts[1].strip()[:2 * len(want)])
                return got == want
    return False


def capture(name, prg, disk=None):
    data = open(prg, "rb").read()
    addr = data[0] | data[1] << 8
    want = data[2:18]
    png = os.path.abspath(os.path.join(OUT, name + ".png"))
    if os.path.exists(png):
        os.remove(png)
    tmp = tempfile.mkdtemp()
    sockname = os.path.join(tmp, "umon")
    xemu = subprocess.Popen(
        ["xemu-xmega65", "-headless", "-besure", "-fastboot", "-prgmode", "65",
         "-uartmon", sockname, "-screenshot", png, "-prg", os.path.abspath(prg)]
        + (["-8", os.path.abspath(disk)] if disk else []),
        stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        sock = None
        deadline = time.time() + 30
        while time.time() < deadline and sock is None:
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.connect(sockname)
                s.settimeout(0.3)
                sock = s
            except OSError:
                time.sleep(0.1)
        if sock is None:
            print("%s: no monitor" % name)
            return False
        started = None
        while time.time() < deadline:
            if loaded(sock, addr, want):
                started = time.time()
                break
            time.sleep(0.05)
        if started is None:
            print("%s: the program never appeared at $%04X" % (name, addr))
            return False
        time.sleep(max(0, started + DELAY - time.time()))
        # PEEK="addr addr ...": memory (hex, 28-bit) printed before stopping
        for a in os.environ.get("PEEK", "").split():
            print(monitor(sock, "m" + a).strip())
        sock.close()
        xemu.send_signal(signal.SIGTERM)
        xemu.wait(timeout=15)
    finally:
        if xemu.poll() is None:
            xemu.kill()
        try:
            os.remove(sockname)
            os.rmdir(tmp)
        except OSError:
            pass
    ok = os.path.exists(png) and os.path.getsize(png) > 0
    print("%s: %s" % (name, png if ok else "no screenshot"))
    return ok


def main():
    if len(sys.argv) in (3, 4) and sys.argv[1].endswith(".prg"):
        global OUT
        OUT = os.path.dirname(os.path.abspath(sys.argv[2]))
        disk = sys.argv[3] if len(sys.argv) == 4 else None
        sys.exit(0 if capture(os.path.splitext(os.path.basename(sys.argv[2]))[0], sys.argv[1], disk) else 1)
    os.makedirs(OUT, exist_ok=True)
    names = sys.argv[1:] or list(EXAMPLES)
    bad = 0
    for n in names:
        if not capture(n, EXAMPLES[n]):
            bad += 1
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
