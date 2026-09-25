#!/usr/bin/env python3
"""Turn rgnano-sim-crash.txt (written by the simulator's crash handler) into
function names and source lines using MSYS2 addr2line.

Usage:
    python tools/symbolize_crash.py [crash.txt] [exe]
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MSYS_BASH = r"C:\msys64\usr\bin\bash.exe"
PREFERRED_BASE = 0x400000


def msys_path(path: Path) -> str:
    resolved = path.resolve()
    drive = resolved.drive.rstrip(":").lower()
    return "/" + drive + "/" + "/".join(resolved.parts[1:])


def main() -> int:
    crash = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "projects" / "rgnano-sim-crash.txt"
    exe = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "projects" / "lgpt-rgnano-sim.exe"
    text = crash.read_text()
    print(text.splitlines()[0])
    base = int(re.search(r"base=(?:0x)?([0-9A-Fa-f]+)", text).group(1), 16)
    addrs = [int(m, 16) for m in re.findall(r"frame s?\d+ (?:0x)?([0-9A-Fa-f]+)", text)]
    fixed = [hex(a - base + PREFERRED_BASE) for a in addrs if a >= base]
    # addr2line only starts inside the MSYS2 environment on this machine
    cmd = f"PATH=/mingw32/bin:/usr/bin addr2line -e '{msys_path(exe)}' -f -C -p " + " ".join(fixed)
    out = subprocess.run([MSYS_BASH, "-lc", cmd], capture_output=True, text=True)
    for i, line in enumerate(out.stdout.splitlines()):
        print(f"#{i} {line}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
